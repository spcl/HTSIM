// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "compositequeue.h"
#include "ecn.h"
#include "uecpacket.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <math.h>
#include <regex>
#include <sstream>
#include <stdio.h>
#include <utility>

bool CompositeQueue::_drop_when_full = false;
bool CompositeQueue::_use_mixed = false;
bool CompositeQueue::_use_phantom = false;
bool CompositeQueue::_use_both_queues = false;
int CompositeQueue::_phantom_queue_size = 0;
bool CompositeQueue::_phantom_in_series = false;
int CompositeQueue::_kmin_from_input = 20;
int CompositeQueue::_kmax_from_input = 80;
int CompositeQueue::_phantom_queue_slowdown = 10;
bool CompositeQueue::use_bts = false;

CompositeQueue::CompositeQueue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, QueueLogger *logger)
        : Queue(bitrate, maxsize, eventlist, logger) {
    _ratio_high = 20;
    _ratio_low = 1;
    _crt = 0;
    _num_headers = 0;
    _num_packets = 0;
    _num_acks = 0;
    _num_nacks = 0;
    _num_pulls = 0;
    _num_drops = 0;
    _num_stripped = 0;
    _num_bounced = 0;
    _ecn_minthresh = maxsize * 2; // don't set ECN by default
    _ecn_maxthresh = maxsize * 2; // don't set ECN by default

    _draining_time_phantom = ((4096 + 64) * 8.0) / LINK_SPEED_MODERN; // Add paramters here eventually
    _draining_time_phantom += (_draining_time_phantom * _phantom_queue_slowdown / 100.0);
    _draining_time_phantom *= 1000;

    _queuesize_high = _queuesize_low = _current_queuesize_phatom = 0;
    _serv = QUEUE_INVALID;
    stringstream ss;
    ss << "compqueue(" << bitrate / 1000000 << "Mb/s," << maxsize << "bytes)";
    _nodename = ss.str();
    /* printf("Created Queue with following params: %s - %d - %d - %d - %d - %d - %lu %lu\n", _nodename.c_str(),
           _use_mixed, _use_phantom, _use_both_queues, _phantom_queue_size, _phantom_in_series, maxsize, bitrate); */
}

void CompositeQueue::decreasePhantom() {
    _current_queuesize_phatom -= (4096 + 64); // parameterize
    if (_current_queuesize_phatom < 0) {
        _current_queuesize_phatom = 0;
    }
    /*printf("Calling Phantom - Name %s - Size %lld - Current Time %lu - "
           "Draining "
           "Phantom "
           "%lu\n",
           _nodename.c_str(), _current_queuesize_phatom, GLOBAL_TIME / 1000,
           _draining_time_phantom / 1000);
    fflush(stdout);*/
    _decrease_phantom_next = eventlist().now() + _draining_time_phantom;
    eventlist().sourceIsPendingRel(*this, _draining_time_phantom);
}

void CompositeQueue::beginService() {
    if (!_enqueued_high.empty() && !_enqueued_low.empty()) {
        _crt++;

        if (_crt >= (_ratio_high + _ratio_low))
            _crt = 0;

        if (_crt < _ratio_high) {
            _serv = QUEUE_HIGH;
            eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_high.back()));
        } else {
            assert(_crt < _ratio_high + _ratio_low);
            _serv = QUEUE_LOW;
            eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_low.back()));
        }
        return;
    }

    if (!_enqueued_high.empty()) {
        _serv = QUEUE_HIGH;
        eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_high.back()));
    } else if (!_enqueued_low.empty()) {
        _serv = QUEUE_LOW;
        eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_low.back()));
    } else {
        assert(0);
        _serv = QUEUE_INVALID;
    }
}

bool CompositeQueue::decide_ECN() {

    // ECN mark on deque
    if (_use_both_queues && _use_phantom) {

        // printf("Using both Queues - Size Real %lu\n", _maxsize);

        bool real_queue_ecn = false;
        _ecn_maxthresh = _maxsize / 100 * 80;
        _ecn_minthresh = _maxsize / 100 * 20;
        if (_queuesize_low > _ecn_minthresh) {
            /* printf("Using both Queues1 %s - Size Real %lu %lu - ECN %lu %lu\n",_nodename.c_str(),  _maxsize,
             * _queuesize_low, _ecn_minthresh, _ecn_maxthresh); */

            uint64_t p = (0x7FFFFFFF * (_queuesize_low - _ecn_minthresh)) / (_ecn_maxthresh - _ecn_minthresh);

            if ((uint64_t)random() < p) {
                real_queue_ecn = true;
                /* printf("Using both Queues2 - Size Real %lu\n", _maxsize); */
            }
        }

        _ecn_maxthresh = _phantom_queue_size / 100 * _kmax_from_input;
        _ecn_minthresh = _phantom_queue_size / 100 * _kmin_from_input;
        if (_current_queuesize_phatom > _ecn_maxthresh) {
            return true;
        } else if (_current_queuesize_phatom > _ecn_minthresh) {
            uint64_t p =
                    (0x7FFFFFFF * (_current_queuesize_phatom - _ecn_minthresh)) / (_ecn_maxthresh - _ecn_minthresh);
            if (((uint64_t)random() < p) || real_queue_ecn) {
                return true;
            }
        }
        return false;

    } else if (_use_phantom) {
        _ecn_maxthresh = _phantom_queue_size / 100 * _kmax_from_input;
        _ecn_minthresh = _phantom_queue_size / 100 * _kmin_from_input;

        if (_current_queuesize_phatom > _ecn_maxthresh) {
            return true;
        } else if (_current_queuesize_phatom > _ecn_minthresh) {
            uint64_t p =
                    (0x7FFFFFFF * (_current_queuesize_phatom - _ecn_minthresh)) / (_ecn_maxthresh - _ecn_minthresh);
            if ((uint64_t)random() < p) {
                return true;
            }
        }
        return false;
    } else {
        /* printf("Using both Queues1 %s - Time %lu - Size Real %lu %lu - ECN %lu %lu\n",_nodename.c_str(), GLOBAL_TIME
         * / 1000, _maxsize, _queuesize_low, _ecn_minthresh, _ecn_maxthresh); */
        _ecn_maxthresh = _maxsize / 100 * _kmax_from_input;
        _ecn_minthresh = _maxsize / 100 * _kmin_from_input;
        if (_queuesize_low > _ecn_maxthresh) {
            return true;
        } else if (_queuesize_low > _ecn_minthresh) {
            uint64_t p = (0x7FFFFFFF * (_queuesize_low - _ecn_minthresh)) / (_ecn_maxthresh - _ecn_minthresh);
            if ((uint64_t)random() < p) {
                return true;
            }
        }
        return false;
    }
}

void CompositeQueue::completeService() {
    Packet *pkt;
    if (_serv == QUEUE_LOW) {

        if (_use_mixed) {
            std::vector<Packet *> new_queue;
            std::vector<Packet *> temp_queue = _enqueued_low._queue;
            int count_good = 0;
            int dropped_p = 0;
            int initial_count = _enqueued_low._count;
            for (int i = 0; i < _enqueued_low._count; i++) {
                int64_t diff = GLOBAL_TIME - temp_queue[_enqueued_low._next_pop + i]->enter_timestamp;
                int64_t diff_budget = temp_queue[_enqueued_low._next_pop + i]->timeout_budget - diff;
                int size_p = temp_queue[_enqueued_low._next_pop + i]->size();
                if (diff_budget > 0) {
                    _enqueued_low._queue[count_good] = temp_queue[_enqueued_low._next_pop + i];

                    /*int64_t diff =
                            GLOBAL_TIME -
                            _enqueued_low._queue[count_good]->enter_timestamp;
                    _enqueued_low._queue[count_good]->enter_timestamp =
                            GLOBAL_TIME;
                    int64_t diff_budget =
                            _enqueued_low._queue[count_good]->timeout_budget -
                            diff;*/
                    count_good++;
                } else {
                    dropped_p++;
                    _queuesize_low -= size_p;
                }
            }
            _enqueued_low._next_pop = 0;
            _enqueued_low._next_push = count_good;
            _enqueued_low._count = count_good;

            if (dropped_p > 0) {
                printf("Started With %d Pkts - Dropped %d Pkts - Count %d - "
                       "Pop %d "
                       "- Push %d\n",
                       initial_count, dropped_p, _enqueued_low._count, _enqueued_low._next_pop,
                       _enqueued_low._next_push);
            }
        }

        assert(!_enqueued_low.empty());

        pkt = _enqueued_low.pop();
        /*printf("Budget From %d - ID %d - Budget %li\n", pkt->from, pkt->id(),
               pkt->timeout_budget);*/

        /* if (_nodename == "compqueue(50000Mb/s,1250000bytes)DC1-DST15->LS7") {
                printf("Sending: Queue %s - From %d to %d size %d - time %lu - "
                       "Last %f\n",
                       _nodename.c_str(), pkt->from, pkt->to, pkt->size(),
                       GLOBAL_TIME / 1000,
                       GLOBAL_TIME / 1000.0 - last_recv / 1000.0);
                last_recv = GLOBAL_TIME;
            } */

        packets_seen++;
        _queuesize_low -= pkt->size();

        if (_phantom_in_series) {
            _current_queuesize_phatom += pkt->size();
            if (_current_queuesize_phatom > _phantom_queue_size) {
                _current_queuesize_phatom = _phantom_queue_size;
            }
        }

        // ECN mark on deque
        if (decide_ECN()) {
            pkt->set_flags(pkt->flags() | ECN_CE);
        }

        if (_logger)
            _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);
        _num_packets++;
    } else if (_serv == QUEUE_HIGH) {
        assert(!_enqueued_high.empty());
        pkt = _enqueued_high.pop();
        trimmed_seen++;
        // printf("Queue %s - %d@%d\n", _nodename.c_str(), pkt->from, pkt->to);
        _queuesize_high -= pkt->size();
        /* if (_nodename == "compqueue(50000Mb/s,1250000bytes)DC1-DST15->LS7" || true) {
                printf("Sending: Queue %s - From %d to %d size %d - time %lu - "
                       "Last %f\n",
                       _nodename.c_str(), pkt->from, pkt->to, pkt->size(),
                       GLOBAL_TIME / 1000,
                       GLOBAL_TIME / 1000.0 - last_recv / 1000.0);
                last_recv = GLOBAL_TIME;
            } */
        if (_logger)
            _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);
        if (pkt->type() == NDPACK)
            _num_acks++;
        else if (pkt->type() == NDPNACK || pkt->type() == UECNACK || pkt->_is_trim) {
            _num_nacks++;
            if (_phantom_in_series) {
                _current_queuesize_phatom += 4160;
                if (_current_queuesize_phatom > _phantom_queue_size) {
                    _current_queuesize_phatom = _phantom_queue_size;
                }
            }
        } else if (pkt->type() == NDPPULL)
            _num_pulls++;
        else {
            // cout << "Hdr: type=" << pkt->type() << endl;
            _num_headers++;
            // ECN mark on deque of a header, if low priority queue is still
            // over threshold
            if (decide_ECN()) {
                pkt->set_flags(pkt->flags() | ECN_CE);
            }
        }
    } else {
        assert(0);
    }

    pkt->flow().logTraffic(*pkt, *this, TrafficLogger::PKT_DEPART);
    pkt->sendOn();

    //_virtual_time += drainTime(pkt);

    _serv = QUEUE_INVALID;

    // cout << "E[ " << _enqueued_low.size() << " " << _enqueued_high.size() <<
    // " ]" << endl;

    if (!_enqueued_high.empty() || !_enqueued_low.empty())
        beginService();

    if (_current_queuesize_phatom >= 0 && first_time && _use_phantom) {
        first_time = false;
        decreasePhantom();
    }
}

void CompositeQueue::doNextEvent() {
    // printf("Doing next event\n");
    if (eventlist().now() == _decrease_phantom_next && _use_phantom) {
        decreasePhantom();
        return;
    }
    completeService();
}

void CompositeQueue::receivePacket(Packet &pkt) {
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);
    if (_logger)
        _logger->logQueue(*this, QueueLogger::PKT_ARRIVE, pkt);
    // is this a Tofino packet from the egress pipeline?

    if (failed_link && !pkt.header_only()) {
        pkt.strip_payload();
        pkt.is_failed = true;
        /* printf("Queue %s - Time %lu - Broken Link", _nodename.c_str(),
               GLOBAL_TIME / 1000); */

    } else if (!pkt.header_only()) {
        //  Queue
        /* printf("Remote is %s vs %s %d %d - Switch ID - %d %d\n", this->getRemoteEndpoint()->nodename().c_str(),
               getSwitch()->nodename().c_str(), this->getRemoteEndpoint()->dc_id,
               ((Switch *)getRemoteEndpoint())->getID(), pkt.previous_switch_id, getSwitch()->getID()); */
        if (COLLECT_DATA) {
            if (_queuesize_low != 0) {
                std::string file_name = PROJECT_ROOT_PATH /
                                        ("sim/output/queue/queue" + _nodename.substr(_nodename.find(")") + 1) + ".txt");
                std::ofstream MyFile(file_name, std::ios_base::app);
                /*printf("Bit rate is %lu\n", _bitrate);
                fflush(stdout);*/
                MyFile << eventlist().now() / 1000 << "," << int(_queuesize_low * 8 / (_bitrate / 1e9)) / (1) << ","
                       << int(_ecn_minthresh * 8 / (_bitrate / 1e9)) / (1) << ","
                       << int(_ecn_maxthresh * 8 / (_bitrate / 1e9)) / (1) << ","
                       << int(_maxsize * 8 / (_bitrate / 1e9)) / (1) << std::endl;

                MyFile.close();
            }
        }

        if (COLLECT_DATA && _use_phantom) {
            if (_current_queuesize_phatom != 0) {
                std::string file_name = PROJECT_ROOT_PATH / ("sim/output/queue_phantom/queue_phantom" +
                                                             _nodename.substr(_nodename.find(")") + 1) + ".txt");
                std::ofstream MyFile(file_name, std::ios_base::app);

                MyFile << eventlist().now() / 1000 << "," << int(_current_queuesize_phatom * 8 / (_bitrate / 1e9)) / (1)
                       << "," << int((_phantom_queue_size / 100 * _kmin_from_input) * 8 / (_bitrate / 1e9)) / (1) << ","
                       << int((_phantom_queue_size / 100 * _kmax_from_input) * 8 / (_bitrate / 1e9)) / (1) << std::endl;

                MyFile.close();
            }
        }

        if (_queuesize_low + pkt.size() <= _maxsize) {
            // regular packet; don't drop the arriving packet

            // we are here because either the queue isn't full or,
            // it might be full and we randomly chose an
            // enqueued packet to trim

            assert(_queuesize_low + pkt.size() <= _maxsize);
            Packet *pkt_p = &pkt;
            /*printf("Considering Queue2 %s - From %d - Header Only %d - Size %d
            "
                   "- "
                   "Arrayt Size "
                   "%d\n",
                   _nodename.c_str(), pkt.from, pkt.header_only(),
                   _queuesize_low, _enqueued_low.size());
            fflush(stdout);*/
            pkt_p->enter_timestamp = GLOBAL_TIME;
            _enqueued_low.push(pkt_p);

            /* if (_nodename == "compqueue(50000Mb/s,1250000bytes)DC1-LS7->DST15") {
                printf("Receive: Queue %s - From %d to %d size %d - time %lu - "
                       "Last %f\n",
                       _nodename.c_str(), pkt_p->from, pkt_p->to, pkt_p->size(),
                       GLOBAL_TIME / 1000,
                       GLOBAL_TIME / 1000.0 - last_recv / 1000.0);
                last_recv = GLOBAL_TIME;
            } */

            // Increase PQ on data packet, if in parallel
            if (!_phantom_in_series) {

                _current_queuesize_phatom += pkt.size();
                if (_current_queuesize_phatom > _phantom_queue_size) {
                    _current_queuesize_phatom = _phantom_queue_size;
                }
            }
            _queuesize_low += pkt.size();

            if (_logger)
                _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);

            if (_serv == QUEUE_INVALID) {
                beginService();
            }

            // cout << "BL[ " << _enqueued_low.size() << " " <<
            // _enqueued_high.size() << " ]" << endl;

            return;
        } else {
            // Increase Phantom Queue when also getting a trim
            if (!_phantom_in_series) {
                _current_queuesize_phatom += 4160; // We increase as if it was a full pkt, not just
                                                   // the header of the trim
                if (_current_queuesize_phatom > _phantom_queue_size) {
                    _current_queuesize_phatom = _phantom_queue_size;
                }
            }
            // strip packet the arriving packet - low priority queue is full
            // cout << "B [ " << _enqueued_low.size() << " " <<
            // _enqueued_high.size() << " ] STRIP" << endl;
            /* printf("Trimming at %s - Packet PathID %d\n", _nodename.c_str(),
                    pkt.pathid());*/
            if (_queuesize_low + pkt.size() > _maxsize) {
                /* printf("Dropping1 %s Packet From %d-%d \n", nodename().c_str(), pkt.from, pkt.id()); */
                if (use_bts && !pkt.is_bts_pkt) {
                    /* printf("Dropping2 %s Packet From %d-%d \n", nodename().c_str(), pkt.from, pkt.id()); */
                    assert(!pkt.header_only());
                    UecPacket *bts_pkt = UecPacket::newpkt(dynamic_cast<UecPacket &>(pkt));
                    bts_pkt->strip_payload();
                    bts_pkt->is_bts_pkt = true;
                    bts_pkt->set_dst(bts_pkt->from);

                    /* for (int i = 0; i < bts_pkt->route()->size(); i++) {
                        printf(" Route is %s -->", bts_pkt->route()->at(i)->nodename().c_str());
                    }
                    Route *r = new Route(); */

                    // bts_pkt->set_route bts_pkt->set_next_hop(bts_pkt->route()->at(0));
                    /* for (int i = 0; i < bts_pkt->route()->size(); i++) {
                        printf(" Updated Route is %s -->", bts_pkt->route()->at(i)->nodename().c_str());
                        r->push_back(bts_pkt->route()->at(i));
                    } */
                    // bts_pkt->set_route(*r);
                    /* printf("BTS Event at %s - Now %d-%d-%d to %d - %lu\n", _nodename.c_str(), bts_pkt->from,
                    pkt.from, bts_pkt->id(), bts_pkt->dst(), GLOBAL_TIME / 1000); printf("\n\n"); */

                    // bts_pkt->sendOn();
                    pkt.free();
                    getSwitch()->receivePacket(*bts_pkt);

                    return;
                }

                if (_drop_when_full) {
                    // Dropping Packet and returning
                    /*printf("Queue Size %d - Max %d\n", _queuesize_low,
                           _maxsize);*/
                    printf("Dropping a PKT\n");
                    pkt.free();
                    return;
                }
            }

            // printf("Trimming %d@%d - Time %lu\n", pkt.from, pkt.to,
            //        GLOBAL_TIME / 1000);
            pkt.strip_payload();
            _num_stripped++;
            pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_TRIM);
            if (_logger)
                _logger->logQueue(*this, QueueLogger::PKT_TRIM, pkt);
        }
    }
    assert(pkt.header_only());

    if (_queuesize_high + pkt.size() > 2000 * _maxsize) {

        /* printf("Dropping a PKT - Max Size %lu - Queue Size %lu \n", _maxsize, _queuesize_high); */
        // drop header
        // cout << "drop!\n";
        if (pkt.reverse_route() && pkt.bounced() == false) {
            // return the packet to the sender
            if (_logger)
                _logger->logQueue(*this, QueueLogger::PKT_BOUNCE, pkt);
            pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_BOUNCE);
            // XXX what to do with it now?
#if 0
            printf("Bounce1 at %s\n", _nodename.c_str());
            printf("Fwd route:\n");
            print_route(*(pkt.route()));
            printf("nexthop: %d\n", pkt.nexthop());
#endif
            pkt.bounce();
#if 0
            printf("\nRev route:\n");
            print_route(*(pkt.reverse_route()));
            printf("nexthop: %d\n", pkt.nexthop());
#endif
            _num_bounced++;
            pkt.sendOn();
            return;
        } else {
            if (_logger)
                _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
            pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
            cout << "B1[ " << _enqueued_low.size() << " " << _enqueued_high.size() << " ] DROP " << pkt.flow().get_id()
                 << endl;
            pkt.free();
            _num_drops++;
            return;
        }
    }

    // if (pkt.type()==NDP)
    //   cout << "H " << pkt.flow().str() << endl;
    Packet *pkt_p = &pkt;
    if (pkt.is_failed) {
        pkt_p->is_failed = true;
        printf("Setting Failure Bit\n");
    }
    _enqueued_high.push(pkt_p);
    _queuesize_high += pkt.size();
    if (_logger)
        _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);

    // cout << "BH[ " << _enqueued_low.size() << " " <<
    // _enqueued_high.size() << " ]" << endl;

    if (_serv == QUEUE_INVALID) {
        beginService();
    }
}

mem_b CompositeQueue::queuesize() const { return _queuesize_low + _queuesize_high; }
