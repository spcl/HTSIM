// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "compositequeuebts.h"
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

CompositeQueueBts::CompositeQueueBts(linkspeed_bps bitrate, mem_b maxsize,
                                     EventList &eventlist, QueueLogger *logger)
        : Queue(bitrate, maxsize, eventlist, logger) {
    _ratio_high = 100000;
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
    _bts_triggering = maxsize;

    _queuesize_high = _queuesize_low = 0;
    _serv = QUEUE_INVALID;
    stringstream ss;
    ss << "compqueue(" << bitrate / 1000000 << "Mb/s," << maxsize << "bytes)";
    _nodename = ss.str();
}

void CompositeQueueBts::beginService() {
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

bool CompositeQueueBts::decide_ECN() {
    // ECN mark on deque
    uint64_t randValue = (uint64_t)random();
    if (_queuesize_low > _ecn_maxthresh) {
        /*printf("Queue From %d - %s - %d - Min ECN %d - Max ECN %d - Current %d
           "
               "- Time %lu - ID %d\n",
               _current_from, _nodename.c_str(), 1,
               int(_ecn_minthresh * 8 / (_bitrate / 1e9)),
               int(_ecn_maxthresh * 8 / (_bitrate / 1e9)), _queuesize_low,
               GLOBAL_TIME / 1000, my_id);*/
        return true;
    } else if (_queuesize_low > _ecn_minthresh) {
        uint64_t p = (0x7FFFFFFF * (_queuesize_low - _ecn_minthresh)) /
                     (_ecn_maxthresh - _ecn_minthresh);
        /*printf("Queue From %d - %s - %d - Min ECN %d - Max ECN %d - Current %d
           "
               "- Time %lu - ID %d\n",
               _current_from, _nodename.c_str(), randValue < p,
               int(_ecn_minthresh * 8 / (_bitrate / 1e9)),
               int(_ecn_maxthresh * 8 / (_bitrate / 1e9)), _queuesize_low,
               GLOBAL_TIME / 1000, my_id);*/
        if (randValue < p) {
            return true;
        }
    }
    /*printf("Queue From %d - %s - FALSE - Time %lu\n", _current_from,
           _nodename.c_str(), GLOBAL_TIME / 1000);*/
    return false;
}

void CompositeQueueBts::completeService() {
    Packet *pkt;
    if (_serv == QUEUE_LOW) {

        assert(!_enqueued_low.empty());
        pkt = _enqueued_low.pop();
        _queuesize_low -= pkt->size();
        /*printf("Considering Queue2 %s - From %d - Header Only %d - Size %d - "
               "Arrayt Size "
               "%d\n",
               _nodename.c_str(), pkt->from, pkt->header_only(), _queuesize_low,
               _enqueued_low.size());*/
        fflush(stdout);

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
            // if (decide_ECN()) {
            //    pkt->set_flags(pkt->flags() | ECN_CE);
            //}
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
}

void CompositeQueueBts::doNextEvent() { completeService(); }

void CompositeQueueBts::receivePacket(Packet &pkt) {
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);
    if (_logger)
        _logger->logQueue(*this, QueueLogger::PKT_ARRIVE, pkt);
    // is this a Tofino packet from the egress pipeline?

    /*printf("Considering Queue %s - ID %d - From %d - Header Only %d - "
           "IsSpecial %d - "
           "Size %d - "
           "Arrayt Size "
           "%d\n",
           _nodename.c_str(), pkt.id(), pkt.from, pkt.header_only(),
           pkt.is_special, _queuesize_low, _enqueued_low.size());
    fflush(stdout);*/

    if (!pkt.header_only()) {
        //  Queue
        if (COLLECT_DATA) {
            if (_queuesize_low != 0) {
                std::string file_name =
                        PROJECT_ROOT_PATH /
                        ("sim/output/queue/queue" +
                         _nodename.substr(_nodename.find(")") + 1) + ".txt");
                std::ofstream MyFile(file_name, std::ios_base::app);

                MyFile << eventlist().now() / 1000 << ","
                       << int(_queuesize_low * 8 / (_bitrate / 1e9))
                       << std::endl;

                MyFile.close();
            }
        }
        _current_from = pkt.from;
        my_id = pkt.id();
        bool prepare_bts = decide_ECN();
        if (_queuesize_low + pkt.size() > _maxsize || prepare_bts) {
            // If queue is full or BTS is triggered or we want to do
            // probabilistic BTS, we send it back
            // Packet bts_pkt = pkt;
            // UecPacket bts_pkt = UecPacket::newpkt((UecPacket)pkt);
            // printf("Pre Copy %d\n", pkt.nexthop());
            UecPacket *bts_pkt =
                    UecPacket::newpkt(dynamic_cast<UecPacket &>(pkt));

            // create packet
            /*UecPacket *bts_pkt = UecPacket::newpkt(
                    _flow, *rt, _highest_sent + 1, data_seq, 64);
            p->from = pkt.from;
            p->to = pkt.to;
            p->tag = pkt.tag;
            p->set_ts(eventlist().now());*/
            bool marked = pkt.flags() & ECN_CE; // ECN check

            if (_queuesize_low + pkt.size() > _maxsize) {
                bts_pkt->_queue_full = true;
                if (marked) {
                    // printf("Marking BTS");
                    bts_pkt->set_flags(pkt.flags() | ECN_CE);
                }
            } else {
                bts_pkt->_queue_full = false;
                // We set ECN to the former data packet, so it doesn't
                // trigger more BTS events
                if (_bts_ignore_ecn_data) {
                    pkt.set_flags(pkt.flags() | ECN_CE);
                    if (marked && bts_pkt->_queue_full) {
                        // printf("Edge Case\n");
                    }
                }
            }

            // printf("BTS Case %d\n", bts_pkt->from);
            if (bts_pkt->reverse_route() && bts_pkt->bounced() == false) {
                if (bts_pkt->_queue_full || (!marked)) {
                    bts_pkt->switch_name = _nodename;
                    /*printf("BTS Case1 - Size %d - Queue Full %d\n",
                           _queuesize_low, bts_pkt->_queue_full);*/
                    bts_pkt->queue_status =
                            ((_queuesize_low + bts_pkt->size()) * 64.0) /
                            _maxsize;
                    // bts_pkt->queue_status -= pkt.previous_queue_bts;
                    /*pkt.previous_queue_bts =
                            ((_queuesize_low + bts_pkt->size()) * 64.0) /
                            _maxsize;*/
                    bts_pkt->strip_payload();
                    bts_pkt->bounce();
                    // bts_pkt->reverse_route();
                    _num_bounced++;
                    bts_pkt->is_special = true;

                    /*printf("Bounce1 at %s\n", _nodename.c_str());
                    printf("Fwd route:\n");
                    print_route(*(bts_pkt->route()));
                    printf("nexthop: %d\n", bts_pkt->nexthop());
                    printf("\nRev route:\n");
                    print_route(*(bts_pkt->reverse_route()));
                    fflush(stdout);*/
                    // printf("Post Copy %d\n", pkt.nexthop());
                    fflush(stdout);

                    bts_pkt->sendOn();
                }
            }
        }

        if (_queuesize_low + pkt.size() <= _maxsize) {
            assert(_queuesize_low + pkt.size() <= _maxsize);
            Packet *pkt_p = &pkt;
            _enqueued_low.push(pkt_p);
            _queuesize_low += pkt.size();
            /*printf("Considering Queue %s - ID %d - From %d - Header Only %d -
            " "IsSpecial %d - " "Size %d - " "Arrayt Size "
                   "%d\n",
                   _nodename.c_str(), pkt.id(), pkt.from, pkt.header_only(),
                   pkt.is_special, _queuesize_low, _enqueued_low.size());
            fflush(stdout);*/
            if (_logger)
                _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);

            if (_serv == QUEUE_INVALID) {
                beginService();
            }
        }
        return;
    } else {
        assert(pkt.header_only());

        if (_queuesize_high + pkt.size() > 200 * _maxsize) {
            // drop header
            // cout << "drop!\n";
            // printf("Should never reach here");
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

                // printf("Bounce2 at %s\n", _nodename.c_str());
                // printf("Fwd route:\n");
                // print_route(*(pkt.route()));
                // printf("nexthop: %d\n", pkt.nexthop());
                // printf("\nRev route:\n");
                // print_route(*(pkt.reverse_route()));
                // fflush(stdout);

                pkt.sendOn();
                return;
            } else {
                if (_logger)
                    _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
                pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
                cout << "B[ " << _enqueued_low.size() << " "
                     << _enqueued_high.size() << " ] DROP "
                     << pkt.flow().get_id() << endl;
                // printf("Free8\n");
                fflush(stdout);
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
}

mem_b CompositeQueueBts::queuesize() const {
    return _queuesize_low + _queuesize_high;
}
