// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "compositequeue.h"
#include "ecn.h"
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
int CompositeQueue::_phantom_queue_size = 0;
int CompositeQueue::_phantom_queue_slowdown = 10;

CompositeQueue::CompositeQueue(linkspeed_bps bitrate, mem_b maxsize,
                               EventList &eventlist, QueueLogger *logger)
        : Queue(bitrate, maxsize, eventlist, logger) {
    _ratio_high = 100;
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

    _draining_time_phantom =
            ((4096 + 64) * 8.0) / 80; // Add paramters here eventually
    _draining_time_phantom +=
            (_draining_time_phantom * _phantom_queue_slowdown / 100.0);
    _draining_time_phantom *= 1000;

    _queuesize_high = _queuesize_low = _current_queuesize_phatom = 0;
    _serv = QUEUE_INVALID;
    stringstream ss;
    ss << "compqueue(" << bitrate / 1000000 << "Mb/s," << maxsize << "bytes)";
    _nodename = ss.str();
}

void CompositeQueue::decreasePhantom() {
    _current_queuesize_phatom -= (4096 + 64);
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
            eventlist().sourceIsPendingRel(*this,
                                           drainTime(_enqueued_high.back()));
        } else {
            assert(_crt < _ratio_high + _ratio_low);
            _serv = QUEUE_LOW;
            eventlist().sourceIsPendingRel(*this,
                                           drainTime(_enqueued_low.back()));
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

    if (_use_phantom) {

        _ecn_maxthresh = _phantom_queue_size / 100 * 80;
        _ecn_minthresh = _phantom_queue_size / 100 * 20;

        /*printf("Current Phantom Queue Size %lld - Min %lld - Max %lld\n",
               _current_queuesize_phatom, _ecn_minthresh, _ecn_maxthresh);*/

        if (_current_queuesize_phatom > _ecn_maxthresh) {
            return true;
        } else if (_current_queuesize_phatom > _ecn_minthresh) {
            uint64_t p = (0x7FFFFFFF *
                          (_current_queuesize_phatom - _ecn_minthresh)) /
                         (_ecn_maxthresh - _ecn_minthresh);
            if ((uint64_t)random() < p) {
                return true;
            }
        }
        return false;
    } else {
        if (_queuesize_low > _ecn_maxthresh) {
            return true;
        } else if (_queuesize_low > _ecn_minthresh) {
            uint64_t p = (0x7FFFFFFF * (_queuesize_low - _ecn_minthresh)) /
                         (_ecn_maxthresh - _ecn_minthresh);
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
                int64_t diff =
                        GLOBAL_TIME - temp_queue[_enqueued_low._next_pop + i]
                                              ->enter_timestamp;
                int64_t diff_budget = temp_queue[_enqueued_low._next_pop + i]
                                              ->timeout_budget -
                                      diff;
                int size_p = temp_queue[_enqueued_low._next_pop + i]->size();
                if (diff_budget > 0) {
                    _enqueued_low._queue[count_good] =
                            temp_queue[_enqueued_low._next_pop + i];

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
                       initial_count, dropped_p, _enqueued_low._count,
                       _enqueued_low._next_pop, _enqueued_low._next_push);
            }
        }

        assert(!_enqueued_low.empty());

        pkt = _enqueued_low.pop();
        /*printf("Budget From %d - ID %d - Budget %li\n", pkt->from, pkt->id(),
               pkt->timeout_budget);*/

        packets_seen++;
        // printf("Queue %s - Packets %d\n", _nodename.c_str(), packets_seen);
        _queuesize_low -= pkt->size();
        //_current_queuesize_phatom -= pkt->size();
        printf("Queue %s - Packets Normal %lu - Packets Ghost %lu\n",
               _nodename.c_str(), _queuesize_low, _current_queuesize_phatom);
        /*printf("Considering Queue %s - From %d - Header Only %d - Size
        %d - " "Arrayt Size "
               "%d\n",
               _nodename.c_str(), pkt->from, pkt->header_only(),
        _queuesize_low, _enqueued_low.size()); fflush(stdout);*/
        // ECN mark on deque
        if (decide_ECN()) {
            pkt->set_flags(pkt->flags() | ECN_CE);
        }

        if (COLLECT_DATA && !pkt->header_only()) {
            if (_nodename.find("US_0") != std::string::npos &&
                pkt->type() == UEC) {
                std::regex pattern("-CS_(\\d+)");
                std::smatch matches;
                if (std::regex_search(_nodename, matches, pattern)) {
                    std::string numberStr = matches[1].str();
                    int number = std::stoi(numberStr);
                    std::string file_name =
                            PROJECT_ROOT_PATH / ("sim/"
                                                 "output/us_to_cs/us_to_cs" +
                                                 _name + ".txt");
                    std::ofstream MyFileUsToCs(file_name, std::ios_base::app);

                    MyFileUsToCs << eventlist().now() / 1000 << "," << number
                                 << std::endl;

                    MyFileUsToCs.close();
                }
            }
            // printf("Test1 %s\n", _nodename.c_str());
            if (_nodename.find("LS0") != std::string::npos &&
                pkt->type() == UEC) {
                // printf("Test2 %s\n", _nodename.c_str());
                std::regex pattern(">US(\\d+)");
                std::smatch matches;
                if (std::regex_search(_nodename, matches, pattern)) {
                    std::string numberStr = matches[1].str();
                    int number = std::stoi(numberStr);
                    std::string file_name =
                            PROJECT_ROOT_PATH / ("sim/"
                                                 "output/ls_to_us/ls_to_us" +
                                                 _name + ".txt");
                    std::ofstream MyFileUsToCs(file_name, std::ios_base::app);

                    MyFileUsToCs << eventlist().now() / 1000 << "," << number
                                 << std::endl;

                    MyFileUsToCs.close();
                }
            }
        }

        if (_logger)
            _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);
        _num_packets++;
    } else if (_serv == QUEUE_HIGH) {
        assert(!_enqueued_high.empty());
        pkt = _enqueued_high.pop();
        trimmed_seen++;
        // printf("Queue %s - Trimmed %d\n", _nodename.c_str(), trimmed_seen);
        _queuesize_high -= pkt->size();
        if (_logger)
            _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);
        if (pkt->type() == NDPACK)
            _num_acks++;
        else if (pkt->type() == NDPNACK)
            _num_nacks++;
        else if (pkt->type() == NDPPULL)
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
    if (!pkt.header_only()) {

        /*printf("Current Queue Size %d - Max %d - Bit Rate %lu - Name %s vs "
               "%s\n",
               _queuesize_low, _maxsize, _bitrate, _nodename.c_str(),
               _name.c_str());*/
        //  Queue
        if (COLLECT_DATA) {
            if (_queuesize_low != 0) {
                std::string file_name =
                        PROJECT_ROOT_PATH /
                        ("sim/output/queue/queue" +
                         _nodename.substr(_nodename.find(")") + 1) + ".txt");
                std::ofstream MyFile(file_name, std::ios_base::app);
                /*printf("Bit rate is %lu\n", _bitrate);
                fflush(stdout);*/
                MyFile << eventlist().now() / 1000 << ","
                       << int(_queuesize_low * 8 / (_bitrate / 1e9)) / (1)
                       << ","
                       << int(_ecn_minthresh * 8 / (_bitrate / 1e9)) / (1)
                       << ","
                       << int(_ecn_maxthresh * 8 / (_bitrate / 1e9)) / (1)
                       << std::endl;

                MyFile.close();
            }
        }

        if (COLLECT_DATA && _use_phantom) {
            if (_current_queuesize_phatom != 0) {
                std::string file_name =
                        PROJECT_ROOT_PATH /
                        ("sim/output/queue_phantom/queue_phantom" +
                         _nodename.substr(_nodename.find(")") + 1) + ".txt");
                std::ofstream MyFile(file_name, std::ios_base::app);

                MyFile << eventlist().now() / 1000 << ","
                       << int(_current_queuesize_phatom * 8 /
                              (_bitrate / 1e9)) /
                                  (1)
                       << ","
                       << int((_phantom_queue_size / 100 * 40) * 8 /
                              (_bitrate / 1e9)) /
                                  (1)
                       << ","
                       << int((_phantom_queue_size / 100 * 70) * 8 /
                              (_bitrate / 1e9)) /
                                  (1)
                       << std::endl;

                MyFile.close();
            }
        }

        if (_queuesize_low + pkt.size() <= _maxsize || drand() < 0.5) {
            // regular packet; don't drop the arriving packet

            // we are here because either the queue isn't full or,
            // it might be full and we randomly chose an
            // enqueued packet to trim

            if (_queuesize_low + pkt.size() > _maxsize) {
                /*printf("Trimming at %s - Packet from %d PathID %d\n",
                       _nodename.c_str(), pkt.from, pkt.pathid());*/
                //  we're going to drop an existing packet from the queue
                if (_enqueued_low.empty()) {
                    // cout << "QUeuesize " << _queuesize_low << "
                    // packetsize "
                    // << pkt.size() << " maxsize " << _maxsize << endl;
                    assert(0);
                }

                if (_drop_when_full) {
                    // Dropping Packet and returning
                    /*printf("Queue Size %d - Max %d\n", _queuesize_low,
                           _maxsize);*/
                    printf("Dropping a PKT\n");
                    pkt.free();
                    return;
                }
                // take last packet from low prio queue, make it a header
                // and place it in the high prio queue

                Packet *booted_pkt = _enqueued_low.pop_front();
                _queuesize_low -= booted_pkt->size();
                _current_queuesize_phatom -= booted_pkt->size();
                if (_logger)
                    _logger->logQueue(*this, QueueLogger::PKT_UNQUEUE,
                                      *booted_pkt);

                // cout << "A [ " << _enqueued_low.size() << " " <<
                // _enqueued_high.size() << " ] STRIP" << endl; cout <<
                // "booted_pkt->size(): " << booted_pkt->size();
                booted_pkt->strip_payload();
                _num_stripped++;
                booted_pkt->flow().logTraffic(*booted_pkt, *this,
                                              TrafficLogger::PKT_TRIM);
                if (_logger)
                    _logger->logQueue(*this, QueueLogger::PKT_TRIM, pkt);

                if (_queuesize_high + booted_pkt->size() > 200 * _maxsize) {
                    if (booted_pkt->reverse_route() &&
                        booted_pkt->bounced() == false) {
                        // return the packet to the sender
                        if (_logger)
                            _logger->logQueue(*this, QueueLogger::PKT_BOUNCE,
                                              *booted_pkt);
                        booted_pkt->flow().logTraffic(
                                pkt, *this, TrafficLogger::PKT_BOUNCE);
                        // XXX what to do with it now?
#if 0
                        printf("Bounce2 at %s\n", _nodename.c_str());
                        printf("Fwd route:\n");
                        print_route(*(booted_pkt->route()));
                        printf("nexthop: %d\n", booted_pkt->nexthop());
#endif
                        booted_pkt->bounce();
#if 0
                        printf("\nRev route:\n");
                        print_route(*(booted_pkt->reverse_route()));
                        printf("nexthop: %d\n", booted_pkt->nexthop());
#endif
                        _num_bounced++;
                        booted_pkt->sendOn();
                    } else {
                        cout << "Dropped\n";
                        booted_pkt->flow().logTraffic(*booted_pkt, *this,
                                                      TrafficLogger::PKT_DROP);
                        booted_pkt->free();
                        if (_logger)
                            _logger->logQueue(*this, QueueLogger::PKT_DROP,
                                              pkt);
                    }
                } else {
                    _enqueued_high.push(booted_pkt);
                    _queuesize_high += booted_pkt->size();
                    if (_logger)
                        _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE,
                                          *booted_pkt);
                }
            }

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

            _queuesize_low += pkt.size();
            _current_queuesize_phatom += pkt.size();
            if (_current_queuesize_phatom > _phantom_queue_size) {
                printf("inside if");
                _current_queuesize_phatom = _phantom_queue_size;
            }
            printf("QueueRec %s - Packets Normal %lu - Packets Ghost %lu\n",
                   _nodename.c_str(), _queuesize_low,
                   _current_queuesize_phatom);

            if (_logger)
                _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);

            if (_serv == QUEUE_INVALID) {
                beginService();
            }

            // cout << "BL[ " << _enqueued_low.size() << " " <<
            // _enqueued_high.size() << " ]" << endl;

            return;
        } else {
            // strip packet the arriving packet - low priority queue is full
            // cout << "B [ " << _enqueued_low.size() << " " <<
            // _enqueued_high.size() << " ] STRIP" << endl;
            /* printf("Trimming at %s - Packet PathID %d\n", _nodename.c_str(),
                    pkt.pathid());*/
            if (_queuesize_low + pkt.size() > _maxsize) {
                if (_drop_when_full) {
                    // Dropping Packet and returning
                    /*printf("Queue Size %d - Max %d\n", _queuesize_low,
                           _maxsize);*/
                    printf("Dropping a PKT\n");
                    pkt.free();
                    return;
                }
            }

            pkt.strip_payload();
            _num_stripped++;
            pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_TRIM);
            if (_logger)
                _logger->logQueue(*this, QueueLogger::PKT_TRIM, pkt);
        }
    }
    assert(pkt.header_only());

    if (_queuesize_high + pkt.size() > 200 * _maxsize) {
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
            cout << "B[ " << _enqueued_low.size() << " "
                 << _enqueued_high.size() << " ] DROP " << pkt.flow().get_id()
                 << endl;
            pkt.free();
            _num_drops++;
            return;
        }
    }

    // if (pkt.type()==NDP)
    //   cout << "H " << pkt.flow().str() << endl;
    Packet *pkt_p = &pkt;
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

mem_b CompositeQueue::queuesize() const {
    return _queuesize_low + _queuesize_high;
}
