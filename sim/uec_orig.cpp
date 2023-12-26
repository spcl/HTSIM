// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "ecn.h"
#include "queue.h"
#include "uec.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <math.h>
#include <regex>
#include <stdio.h>
#include <utility>

#define timeInf 0

// Parameters
std::string UecSrc::queue_type = "composite";
std::string UecSrc::algorithm_type = "standard_trimming";
bool UecSrc::use_fast_drop = false;
int UecSrc::fast_drop_rtt = 1;
bool UecSrc::do_jitter = false;
bool UecSrc::do_exponential_gain = false;
bool UecSrc::use_fast_increase = false;
bool UecSrc::use_super_fast_increase = false;
double UecSrc::exp_gain_value_med_inc = 1;
double UecSrc::jitter_value_med_inc = 1;
double UecSrc::delay_gain_value_med_inc = 5;
int UecSrc::target_rtt_percentage_over_base = 50;

double UecSrc::y_gain = 1;
double UecSrc::x_gain = 0.15;
double UecSrc::z_gain = 1;
double UecSrc::w_gain = 1;
double UecSrc::bonus_drop = 1;
double UecSrc::buffer_drop = 1.2;

UecSrc::UecSrc(UecLogger *logger, TrafficLogger *pktLogger,
               EventList &eventList, uint64_t rtt, uint64_t bdp,
               uint64_t queueDrainTime, int hops)
        : EventSource(eventList, "uec"), _logger(logger), _flow(pktLogger) {
    _mss = Packet::data_packet_size();
    _unacked = 0;
    _nodename = "uecsrc";

    _last_acked = 0;
    _highest_sent = 0;
    _use_good_entropies = false;
    _next_good_entropy = 0;

    _nack_rtx_pending = 0;

    // new CC variables
    _hop_count = hops;
    _base_rtt = ((_hop_count * LINK_DELAY_MODERN) +
                 (PKT_SIZE_MODERN * 8 / LINK_SPEED_MODERN * _hop_count) +
                 (_hop_count * LINK_DELAY_MODERN) +
                 (64 * 8 / LINK_SPEED_MODERN * _hop_count)) *
                1000;
    _target_rtt =
            _base_rtt * ((target_rtt_percentage_over_base + 1) / 100.0 + 1);

    _rtt = _base_rtt;
    _rto = rtt + _hop_count * queueDrainTime + (rtt * 90000);
    _rto_margin = _rtt / 2;
    _rtx_timeout = timeInf;
    _rtx_timeout_pending = false;
    _rtx_pending = false;
    _crt_path = 0;
    _flow_size = _mss * 934;
    _trimming_enabled = true;
    target_window = _cwnd;

    _bdp = (_base_rtt * LINK_SPEED_MODERN / 8) / 1000;
    printf("Link Delay %lu - Link Speed %lu - Pkt Size %d - Base RTT %lu - "
           "Target RTT is %lu - BDP/CWDN %lu\n",
           LINK_DELAY_MODERN, LINK_SPEED_MODERN, PKT_SIZE_MODERN, _base_rtt,
           _target_rtt, _bdp);
    _maxcwnd = _bdp;
    _cwnd = _bdp;
    _consecutive_low_rtt = 0;

    _target_based_received = true;

    _max_good_entropies = 10; // TODO: experimental value
    _enableDistanceBasedRtx = false;
    f_flow_over_hook = nullptr;

    if (queue_type == "composite_bts") {
        _bts_enabled = true;
    } else {
        _bts_enabled = false;
    }
}

// Add deconstructor and save data once we are done.
// Add deconstructor and save data once we are done.
UecSrc::~UecSrc() {
    // If we are collecting specific logs
    if (COLLECT_DATA) {
        // RTT
        std::string file_name =
                PROJECT_ROOT_PATH / ("sim/output/rtt/rtt" + _name + "_" +
                                     std::to_string(tag) + ".txt");
        std::ofstream MyFile(file_name, std::ios_base::app);

        for (const auto &p : _list_rtt) {
            MyFile << get<0>(p) << "," << get<1>(p) << "," << get<2>(p) << ","
                   << get<3>(p) << "," << get<4>(p) << "," << get<5>(p)
                   << std::endl;
        }

        MyFile.close();

        // CWD
        file_name = PROJECT_ROOT_PATH / ("sim/output/cwd/cwd" + _name + "_" +
                                         std::to_string(tag) + ".txt");
        std::ofstream MyFileCWD(file_name, std::ios_base::app);

        for (const auto &p : _list_cwd) {
            MyFileCWD << p.first << "," << p.second << std::endl;
        }

        MyFileCWD.close();

        // Unacked
        file_name = PROJECT_ROOT_PATH / ("sim/output/unacked/unacked" + _name +
                                         "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileUnack(file_name, std::ios_base::app);

        for (const auto &p : _list_unacked) {
            MyFileUnack << p.first << "," << p.second << std::endl;
        }

        MyFileUnack.close();

        // NACK
        file_name = PROJECT_ROOT_PATH / ("sim/output/nack/nack" + _name + "_" +
                                         std::to_string(tag) + ".txt");
        std::ofstream MyFileNack(file_name, std::ios_base::app);

        for (const auto &p : _list_nack) {
            MyFileNack << p.first << "," << p.second << std::endl;
        }

        MyFileNack.close();

        // BTS
        if (_list_bts.size() > 0) {
            file_name =
                    PROJECT_ROOT_PATH / ("sim/output/bts/bts" + _name + "_" +
                                         std::to_string(tag) + ".txt");
            std::ofstream MyFileBTS(file_name, std::ios_base::app);

            for (const auto &p : _list_bts) {
                MyFileBTS << p.first << "," << p.second << std::endl;
            }

            MyFileBTS.close();
        }

        // Acked Bytes
        file_name = PROJECT_ROOT_PATH / ("sim/output/acked/acked" + _name +
                                         "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileAcked(file_name, std::ios_base::app);

        for (const auto &p : _list_acked_bytes) {
            MyFileAcked << p.first << "," << p.second << std::endl;
        }

        MyFileAcked.close();

        // Acked ECN
        file_name = PROJECT_ROOT_PATH / ("sim/output/ecn_rtt/ecn_rtt" + _name +
                                         "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileEcnRTT(file_name, std::ios_base::app);

        for (const auto &p : _list_ecn_rtt) {
            MyFileEcnRTT << p.first << "," << p.second << std::endl;
        }

        MyFileEcnRTT.close();

        // Acked Trimmed
        file_name = PROJECT_ROOT_PATH /
                    ("sim/output/trimmed_rtt/trimmed_rtt" + _name + "_" +
                     std::to_string(tag) + ".txt");
        std::ofstream MyFileTrimmedRTT(file_name, std::ios_base::app);

        for (const auto &p : _list_trimmed_rtt) {
            MyFileTrimmedRTT << p.first << "," << p.second << std::endl;
        }

        MyFileTrimmedRTT.close();

        // Fast Increase
        file_name = PROJECT_ROOT_PATH / ("sim/output/fasti/fasti" + _name +
                                         "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileFastInc(file_name, std::ios_base::app);

        for (const auto &p : _list_fast_increase_event) {
            MyFileFastInc << p.first << "," << p.second << std::endl;
        }

        MyFileFastInc.close();

        // Fast Decrease
        file_name = PROJECT_ROOT_PATH / ("sim/output/fastd/fastd" + _name +
                                         "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileFastDec(file_name, std::ios_base::app);

        for (const auto &p : _list_fast_decrease) {
            MyFileFastDec << p.first << "," << p.second << std::endl;
        }

        MyFileFastDec.close();

        // Medium Increase
        file_name = PROJECT_ROOT_PATH / ("sim/output/mediumi/mediumi" + _name +
                                         "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileMediumInc(file_name, std::ios_base::app);

        for (const auto &p : _list_medium_increase_event) {
            MyFileMediumInc << p.first << "," << p.second << std::endl;
        }

        MyFileMediumInc.close();

        // Case 1
        file_name = PROJECT_ROOT_PATH / ("sim/output/case1/case1" + _name +
                                         "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileCase1(file_name, std::ios_base::app);

        for (const auto &p : count_case_1) {
            MyFileCase1 << p.first << "," << p.second << std::endl;
        }

        MyFileCase1.close();

        // Case 2
        file_name = PROJECT_ROOT_PATH / ("sim/output/case2/case2" + _name +
                                         "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileCase2(file_name, std::ios_base::app);

        for (const auto &p : count_case_2) {
            MyFileCase2 << p.first << "," << p.second << std::endl;
        }

        MyFileCase2.close();

        // Case 3
        file_name = PROJECT_ROOT_PATH / ("sim/output/case3/case3" + _name +
                                         "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileCase3(file_name, std::ios_base::app);

        for (const auto &p : count_case_3) {
            MyFileCase3 << p.first << "," << p.second << std::endl;
        }

        MyFileCase3.close();

        // Case 4
        file_name = PROJECT_ROOT_PATH / ("sim/output/case4/case4" + _name +
                                         "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileCase4(file_name, std::ios_base::app);

        for (const auto &p : count_case_4) {
            MyFileCase4 << p.first << "," << p.second << std::endl;
        }

        MyFileCase4.close();
    }
}

void UecSrc::doNextEvent() { startflow(); }

std::size_t UecSrc::get_sent_packet_idx(uint32_t pkt_seqno) {
    for (std::size_t i = 0; i < _sent_packets.size(); ++i) {
        if (pkt_seqno == _sent_packets[i].seqno) {
            return i;
        }
    }
    return _sent_packets.size();
}

void UecSrc::update_rtx_time() {
    _rtx_timeout = timeInf;
    for (const auto &sp : _sent_packets) {
        auto timeout = sp.timer;
        if (!sp.acked && !sp.nacked && !sp.timedOut &&
            (timeout < _rtx_timeout || _rtx_timeout == timeInf)) {
            _rtx_timeout = timeout;
        }
    }
}

void UecSrc::mark_received(UecAck &pkt) {
    // cummulative ack
    if (pkt.seqno() == 1) {
        while (!_sent_packets.empty() &&
               (_sent_packets[0].seqno <= pkt.ackno() ||
                _sent_packets[0].acked)) {
            _sent_packets.erase(_sent_packets.begin());
        }
        update_rtx_time();
        return;
    }
    if (_sent_packets.empty() || _sent_packets[0].seqno > pkt.ackno()) {
        // duplicate ACK -- since we support OOO, this must be caused by
        // duplicate retransmission
        return;
    }
    auto i = get_sent_packet_idx(pkt.seqno());
    if (i == 0) {
        // this should not happen because of cummulative acks, but
        // shouldn't cause harm either
        do {
            _sent_packets.erase(_sent_packets.begin());
        } while (!_sent_packets.empty() && _sent_packets[0].acked);
    } else {
        assert(i < _sent_packets.size());
        auto timer = _sent_packets[i].timer;
        auto seqno = _sent_packets[i].seqno;
        auto nacked = _sent_packets[i].nacked;
        _sent_packets[i] = SentPacket(timer, seqno, true, false, false);
        if (nacked) {
            --_nack_rtx_pending;
        }
        _last_acked = seqno + _mss - 1;
        if (_enableDistanceBasedRtx) {
            bool trigger = true;
            // TODO: this could be optimized with counters or bitsets,
            // but I'm doing this the simple way to avoid bugs while
            // we don't need the optimizations
            for (std::size_t k = 1; k < _sent_packets.size() / 2; ++k) {
                if (!_sent_packets[k].acked) {
                    trigger = false;
                    break;
                }
            }
            if (trigger) {
                // TODO: what's the proper way to act if this packet was
                // NACK'ed? Not super relevant right now as we are not enabling
                // this feature anyway
                _sent_packets[0].timer = eventlist().now();
                _rtx_timeout_pending = true;
            }
        }
    }
    update_rtx_time();
}

void UecSrc::add_ack_path(const Route *rt) {
    for (auto &r : _good_entropies) {
        if (r == rt) {
            return;
        }
    }
    if (_good_entropies.size() < _max_good_entropies) {
        // printf("Pushing Back Now, %d\n", from);
        _good_entropies.push_back(rt);
    } else {
        // TODO: this could cause some weird corner cases that would
        // preserve old entropies, but it probably won't be an issue
        // for simulations.
        // Example corner case: if a path is used, then the other
        // paths are replaced up to the point that _next_good_entropy
        // comes back to that path, it could be used again before any
        // of the newer paths.
        _good_entropies[_next_good_entropy] = rt;
        ++_next_good_entropy;
        _next_good_entropy %= _max_good_entropies;
    }
}

void UecSrc::set_traffic_logger(TrafficLogger *pktlogger) {
    _flow.set_logger(pktlogger);
}

void UecSrc::reduce_cwnd(uint64_t amount) {
    /*if (_cwnd <= saved_acked_bytes || count_received < ignore_for) {
        printf("Nothing to be done");
        return;
    }*/

    // printf("Reducing from %d (%s) - %lu - Now %d\n", from, _name.c_str(),
    //        amount, _cwnd);
    if (_cwnd >= amount + _mss) {
        _cwnd -= amount * 1;
    } else {
        _cwnd = _mss;
    }
}

void UecSrc::reduce_unacked(uint64_t amount) {
    if (_unacked >= amount) {
        _unacked -= amount;
    } else {
        _unacked = 0;
    }
}

void UecSrc::do_fast_drop(bool ecn_or_trimmed) {

    if (from == 0) {
        printf("%d@%d@%d -- Considering fast Drop -- Current %lu vs %lu\n",
               from, to, tag, eventlist().now(), next_window_end);
    }
    if (eventlist().now() >= next_window_end) {
        previous_window_end = next_window_end;
        _list_acked_bytes.push_back(
                std::make_pair(eventlist().now() / 1000, acked_bytes));
        saved_acked_bytes = acked_bytes;
        saved_good_bytes = good_bytes;
        acked_bytes = 0;
        good_bytes = 0;

        _list_ecn_rtt.push_back(std::make_pair(eventlist().now() / 1000,
                                               count_ecn_in_rtt * _mss));
        _list_trimmed_rtt.push_back(std::make_pair(
                eventlist().now() / 1000, count_trimmed_in_rtt * _mss));
        count_trimmed_in_rtt = 0;
        count_ecn_in_rtt = 0;
        next_window_end = eventlist().now() + (_target_rtt * 1);

        ecn_last_rtt = false;

        // Enable Fast Drop
        printf("Using Fast Drop1 - Flow %d@%d@%d, Ecn %d, CWND %d, Saved "
               "Acked %d (%d) - Previous Window %lu - Next Window %lu -  "
               "get_unacked() %lu - // "
               "Time "
               "%lu - Ratio is %f (%lu vs %lu vs %lu) - Trimmed %d\n",
               from, to, tag, 1, _cwnd, saved_acked_bytes, saved_trimmed_bytes,
               previous_window_end / 1000, next_window_end / 1000,
               get_unacked(), eventlist().now() / 1000,
               ((eventlist().now() - previous_window_end + _base_rtt) /
                (double)_base_rtt),
               (eventlist().now() - previous_window_end + _base_rtt) / 1000,
               previous_window_end / 1000, _base_rtt / 1000);
        saved_trimmed_bytes = 0;
        if ((ecn_or_trimmed || need_fast_drop) &&
            (saved_acked_bytes > 0 ||
             (saved_acked_bytes == 0 && previous_window_end != 0)) &&
            previous_window_end != 0) {

            /*saved_acked_bytes =
                    saved_acked_bytes *
                    ((double)_base_rtt /
                     (eventlist().now() - previous_window_end + _base_rtt));*/

            double bonus_based_on_target = buffer_drop;

            printf("Using Fast Drop2 - Flow %d@%d%d, Ecn %d, CWND %d, Saved "
                   "Acked %d (dropping to %f - bonus1 %f 2 %f -> %f and %f) - "
                   "Previous "
                   "Window %lu - Next "
                   "Window %lu// "
                   "Time "
                   "%lu\n",
                   from, to, tag, 1, _cwnd, saved_acked_bytes,
                   max((double)(saved_acked_bytes * bonus_based_on_target *
                                bonus_drop),
                       saved_acked_bytes * bonus_based_on_target * bonus_drop +
                               _mss),
                   bonus_based_on_target, bonus_drop,
                   (saved_acked_bytes * bonus_based_on_target * bonus_drop),
                   (saved_acked_bytes * bonus_based_on_target * bonus_drop +
                    _mss),
                   previous_window_end / 1000, next_window_end / 1000,
                   eventlist().now() / 1000);

            _cwnd = max((double)(saved_acked_bytes * bonus_based_on_target *
                                 bonus_drop),
                        bonus_based_on_target * bonus_drop * _mss);

            //_cwnd = 40000;
            ignore_for = (get_unacked() / (double)_mss) * 1.25;
            // int random_integer_wait = rand() % ignore_for;
            //  ignore_for += random_integer_wait;
            printf("Ignoring %d for %d pkts - New Wnd %d (%d %d)\n", from,
                   ignore_for, _cwnd,
                   (uint32_t)(saved_acked_bytes * bonus_based_on_target),
                   saved_acked_bytes + _mss);
            count_received = 0;
            was_zero_before = false;
            need_fast_drop = false;
            _list_fast_decrease.push_back(
                    std::make_pair(eventlist().now() / 1000, 1));

            // Set x_gain
            printf("Starting Search, XGain is %f\n", ideal_x);
            for (int search_x = _bdp; search_x >= _mss;
                 search_x = search_x / 4) {
                if (_cwnd > search_x) {
                    printf("%d - Using XGAIN %f\n", from, ideal_x);
                    break;
                }
                ideal_x *= 0.5;
            }
        }
    }
}

void UecSrc::processNack(UecNack &pkt) {
    count_trimmed_in_rtt++;

    consecutive_nack++;
    trimmed_last_rtt++;
    consecutive_good_medium = 0;
    acked_bytes += 64;
    saved_trimmed_bytes += 64;
    // exp_avg_route = 1024;

    if (count_received >= ignore_for) {
        need_fast_drop = true;
    }

    printf("Just NA CK from %d at %lu\n", from, eventlist().now() / 1000);

    if (use_fast_drop) {
        if (count_received >= ignore_for) {
            do_fast_drop(true);
        }
        if (count_received > ignore_for) {
            reduce_cwnd(uint64_t(_mss * 1));
        }
    } else {
        reduce_cwnd(uint64_t(_mss * 1));
    }

    _list_cwd.push_back(std::make_pair(eventlist().now() / 1000, _cwnd));

    _consecutive_no_ecn = 0;
    _consecutive_low_rtt = 0;
    _received_ecn.push_back(std::make_tuple(eventlist().now(), true, _mss,
                                            _target_rtt + 10000));

    _list_nack.push_back(std::make_pair(eventlist().now() / 1000, 1));
    // mark corresponding packet for retransmission
    auto i = get_sent_packet_idx(pkt.seqno());
    assert(i < _sent_packets.size());

    assert(!_sent_packets[i].acked); // TODO: would it be possible for a packet
                                     // to receive a nack after being acked?
    if (!_sent_packets[i].nacked) {
        // ignore duplicate nacks for the same packet
        _sent_packets[i].nacked = true;
        ++_nack_rtx_pending;
    }

    /*
    // update ECN
    simtime_picosec ts = pkt.ts();
    bool marked = pkt.flags() &
                  ECN_ECHO; // ECN was marked on data packet and echoed on
    NACK _received_ecn.push_back( std::make_tuple(eventlist().now(), marked,
                            0)); // TODO: assuming same size for all packets
    if (marked) {
        adjust_window(ts, marked);
        }*/
    // retransmit only the NACK'ed packet
    bool success = resend_packet(i);
    if (!_rtx_pending && !success) {
        _rtx_pending = true;
    }
}

/*void btsReduce() {
    int cwnd = 0;
    float alpha = 0.5;
    float queue_deviation = (delta * observed_ecn) / max_ecn;
    float observed_queue_occupancy = cwnd =
            (delta * observed_ecn) / max_ecn + kmin + bdp;
    cwnd - (cwnd * alpha *
            (queue_deviation / (observed_queue_occupancy) / (cwnd / _mss)))
}*/

void UecSrc::processBts(UecPacket *pkt) {
    _list_cwd.push_back(std::make_pair(eventlist().now() / 1000, _cwnd));

    // Update variables
    counter_consecutive_good_bytes = 0;
    _consecutive_no_ecn = 0;
    _consecutive_low_rtt = 0;
    _received_ecn.push_back(std::make_tuple(eventlist().now(), true, _mss,
                                            _target_rtt + 10000));
    _list_nack.push_back(std::make_pair(eventlist().now() / 1000, 1));
    bool marked = pkt->flags() &
                  ECN_CE; // ECN was marked on data packet and echoed on ACK

    if (pkt->_queue_full) {
        printf("BTS %d - Queue is full - Level %d - %ld - Name %s\n", from,
               pkt->queue_status, eventlist().now() / 1000,
               pkt->switch_name.c_str());
        double reduce_by = exp_avg_bts / 64.0 * _mss;
        // reduce_by = 0;
        if (marked) {
            printf("Using ExpAvg %f and %d\n", reduce_by, exp_avg_bts);
            reduce_cwnd(uint64_t((_mss - reduce_by)));
        } else {
            reduce_cwnd(uint64_t(_mss));
        }

        // reduce_cwnd(uint64_t(_mss));
        exp_avg_route = 1024;

        _list_bts.push_back(std::make_pair(eventlist().now() / 1000, 1));
    } else {
        printf("BTS %d - Warning - Level %d - Reduce %lu (%f) - %lu - Name "
               "%s\n",
               from, pkt->queue_status,
               (uint64_t)(_mss * (pkt->queue_status / 64.0) *
                          ((double)_cwnd / _bdp)),
               (double)_cwnd / _bdp, eventlist().now() / 1000,
               pkt->switch_name.c_str());

        _list_bts.push_back(std::make_pair(eventlist().now() / 1000, 1));

        double alpha = 0.125;
        exp_avg_bts = alpha * pkt->queue_status + (1 - alpha) * exp_avg_bts;

        exp_avg_route = alpha_route * 1024 + (1 - alpha_route) * exp_avg_route;

        if (exp_avg_route >= 512 || false) {
            uint64_t value_d =
                    (uint64_t)(1 * (_mss * (pkt->queue_status / 64.0) *
                                    ((double)(_cwnd) / _bdp)));
            if (value_d >= _mss) {
                reduce_cwnd((uint64_t)(_mss));
            } else {
                reduce_cwnd((uint64_t)(value_d));
            }
        } else {
            // printf("%d Not changing cwnd\n", from);
        }

        // printf("Free1\n");
        // fflush(stdout);
        pkt->free();
        return;
    }

    // mark corresponding packet for retransmission
    pkt->unbounce(_mss);
    auto i = get_sent_packet_idx(pkt->seqno());
    assert(i < _sent_packets.size());

    assert(!_sent_packets[i].acked); // TODO: would it be possible for a packet
                                     // to receive a nack after being acked?
    if (!_sent_packets[i].nacked) {
        // ignore duplicate nacks for the same packet
        _sent_packets[i].nacked = true;
        ++_nack_rtx_pending;
    }

    // retransmit only the NACK'ed packet
    bool success = resend_packet(i);
    if (!_rtx_pending && !success) {
        _rtx_pending = true;
    }
    pkt->free();
    // printf("Free2\n");
    // fflush(stdout);
}

void UecSrc::processAck(UecAck &pkt, bool force_marked) {
    UecAck::seq_t seqno = pkt.ackno();
    simtime_picosec ts = pkt.ts();
    if (seqno < _last_acked) {
        // return; // TODO: not for now
    }
    consecutive_nack = 0;
    bool marked = pkt.flags() &
                  ECN_ECHO; // ECN was marked on data packet and echoed on ACK

    /*printf("Packet from %d is ECN Marked %d - Time %lu - Next Window End "
           "%lu\n",
           from, marked, GLOBAL_TIME / 1000, next_window_end / 1000);*/

    if (_start_timer_window) {
        _start_timer_window = false;
        _next_check_window = GLOBAL_TIME + TARGET_RTT_MODERN * 1000;
    }
    uint64_t newRtt = eventlist().now() - ts;
    avg_rtt = avg_rtt * (1 - 0.15) + 0.15 * newRtt;
    _received_ecn.push_back(std::make_tuple(
            eventlist().now(), marked, _mss,
            newRtt)); // TODO: assuming same size for all packets
    mark_received(pkt);

    /*if (update_next_window) {
        next_window_end = eventlist().now() + _base_rtt;
        update_next_window = false;
        printf("Scheduling Next Check %d is %d at %lu for %lu\n", from,
               saved_acked_bytes, eventlist().now() / 1000,
               next_window_end / 1000);
    }*/

    /*if (eventlist().now() >= next_window_end) {
        previous_window_end = next_window_end;
        if (next_window_end == 0) {
            next_window_end = eventlist().now() + (_base_rtt * fast_drop_rtt);
        } else {
            next_window_end += (_base_rtt * fast_drop_rtt);
        }
        _list_acked_bytes.push_back(
                std::make_pair(eventlist().now() / 1000, acked_bytes));
        if (count_total_ecn * 2 >= count_total_ack) {
            fast_drop = true;
            drop_amount = acked_bytes;
        } else {
            fast_drop = false;
        }

        ecn_last_rtt = false;
        count_total_ack = 0;
        count_total_ecn = 0;
        saved_acked_bytes = acked_bytes;
        _list_ecn_rtt.push_back(std::make_pair(eventlist().now() / 1000,
                                               count_ecn_in_rtt * _mss));
        _list_trimmed_rtt.push_back(std::make_pair(
                eventlist().now() / 1000, count_trimmed_in_rtt * _mss));
        count_trimmed_in_rtt = 0;
        count_ecn_in_rtt = 0;
        printf("Saved Acked %d is %d at %lu\n", from, saved_acked_bytes,
               eventlist().now() / 1000);
        acked_bytes = 0;
        trimmed_last_rtt = 0;
    }*/

    count_total_ack++;
    if (marked) {
        count_total_ecn++;
        consecutive_good_medium = 0;
    }

    if (!marked) {
        add_ack_path(pkt.inRoute);
        _consecutive_no_ecn += _mss;
        // printf("Not Marked!\n");
    } else {
        ecn_last_rtt = true;
        _consecutive_no_ecn = 0;
    }
    if (newRtt < _target_rtt) {
        ++_consecutive_low_rtt;
    } else {
        _consecutive_low_rtt = 0;
    }

    // printf("Current Time %lu - New RTT is %lu - Sent Time %lu - FlowName
    // %s - ECN %d\n",(long long)eventlist().now(), (long long)newRtt, (long
    // long) ts, _name.c_str(), marked);
    _list_rtt.push_back(std::make_tuple(eventlist().now() / 1000, newRtt / 1000,
                                        pkt.seqno(), pkt.ackno(),
                                        _base_rtt / 1000, _target_rtt / 1000));

    // printf("Received Good Ack %lu vs %lu || %lu\n", seqno, _flow_size,
    //        _last_acked);
    if (seqno >= _flow_size && _sent_packets.empty() && !_flow_finished) {
        _flow_finished = true;
        if (f_flow_over_hook) {
            f_flow_over_hook(pkt);
        }

        cout << "Flow " << nodename() << " finished at "
             << timeAsMs(eventlist().now()) << endl;
        cout << "Flow " << nodename() << " completion time is "
             << timeAsMs(eventlist().now() - _flow_start_time) << endl;

        printf("Completion Time Flow is %lu\n",
               eventlist().now() - _flow_start_time);
    }

    if (seqno > _last_acked || true) { // TODO: new ack, we don't care about
                                       // ordering for now. Check later though
        if (seqno >= _highest_sent) {
            _highest_sent = seqno;
        }

        _last_acked = seqno;

        _list_cwd.push_back(std::make_pair(eventlist().now() / 1000, _cwnd));
        // printf("Window Is %d - From %d To %d\n", _cwnd, from, to);
        adjust_window(ts, marked, newRtt);
        acked_bytes += _mss;
        good_bytes += _mss;

        _effcwnd = _cwnd;
        // printf("Received From %d - Sending More\n", from);
        send_packets();
        return; // TODO: if no further code, this can be removed
    }
}

uint64_t UecSrc::get_unacked() {
    // return _unacked;
    uint64_t missing = 0;
    for (const auto &sp : _sent_packets) {
        if (!sp.acked && !sp.nacked && !sp.timedOut) {
            missing += _mss;
        }
    }
    return missing;
}

void UecSrc::receivePacket(Packet &pkt) {
    // every packet received represents one less packet in flight

    // printf("Node %s - Received packet %d - From %d\n",
    // nodename().c_str(),
    //        pkt.id(), pkt.from);

    if (pkt._queue_full || pkt.bounced() == false) {
        reduce_unacked(_mss);
    } else {
        printf("Never here\n");
    }

    // TODO: receive window?
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_RCVDESTROY);

    if (_logger) {
        _logger->logUec(*this, UecLogger::UEC_RCV);
    }
    switch (pkt.type()) {
    case UEC:
        // BTS
        if (_bts_enabled) {
            if (pkt.bounced()) {
                processBts((UecPacket *)(&pkt));
                counter_consecutive_good_bytes = 0;
                increasing = false;
            }
        }
        break;
    case UECACK:
        // printf("NORMALACK, %d\n", pkt.from);
        count_received++;

        processAck(dynamic_cast<UecAck &>(pkt), false);
        pkt.free();
        break;
    case UECNACK:
        printf("NACK %d@%d@%d\n", from, to, tag);
        if (_trimming_enabled) {
            count_received++;
            processNack(dynamic_cast<UecNack &>(pkt));
            pkt.free();
        }
        break;
    default:
        std::cout << "unknown packet receive with type code: " << pkt.type()
                  << "\n";
        return;
    }
    if (get_unacked() < _cwnd && _rtx_timeout_pending) {
        eventlist().sourceIsPendingRel(*this, 0);
    }
    // pkt.free();
    // printf("Free3\n");
    // fflush(stdout);
}

uint32_t UecSrc::medium_increase(simtime_picosec rtt) {
    /*printf("Inceasing %d by %d at %lu\n", from,
           min(uint32_t((((_target_rtt - rtt) / (double)rtt) * 5 *
                         ((double)_mss / _cwnd) * _mss) +
                        (((double)_mss / _cwnd) * _mss)),
               uint32_t(_mss)),
           eventlist().now() / 1000);*/
    consecutive_good_medium += _mss;
    // Jitter
    if (do_jitter) {
        jitter_value_med_inc = (consecutive_good_medium / (double)_mss);
    } else {
        jitter_value_med_inc = 1;
    }
    // Exponential/Linear
    int none_rtt_gain = 1;
    if (do_exponential_gain) {
        exp_gain_value_med_inc = 1;
    } else {
        none_rtt_gain = 1; /*LINK_SPEED_MODERN / 100 / 8*/
        exp_gain_value_med_inc = (_mss / (double)_cwnd);
    }

    // Delay Gain Automated (1 / (target_ratio - 1)) vs manually set at 0 (put
    // close to target_rtt)

    if (consecutive_good_medium < _cwnd && do_jitter) {
        return 0;
    } else {
        consecutive_good_medium = 0;

        _list_medium_increase_event.push_back(
                std::make_pair(eventlist().now() / 1000, 1));
        if (delay_gain_value_med_inc == 0) {
            return min(uint32_t(_mss) * exp_gain_value_med_inc * none_rtt_gain,
                       double(_mss) * none_rtt_gain) *
                   jitter_value_med_inc;
        } else {
            return min(uint32_t((((_target_rtt - rtt) / (double)rtt) *
                                 delay_gain_value_med_inc * _mss) *
                                        exp_gain_value_med_inc +
                                (_mss) * (_mss / (double)_cwnd)) *
                               none_rtt_gain,
                       uint32_t(_mss)) *
                   none_rtt_gain * jitter_value_med_inc;
        }
    }
}

void UecSrc::fast_increase() {
    if (use_fast_drop) {
        if (count_received > ignore_for) {
            // counter_consecutive_good_bytes = counter_consecutive_good_bytes /
            // 2;
            if (use_super_fast_increase) {
                _cwnd += 2 * _mss * (LINK_SPEED_MODERN / 100);
                //_cwnd *= 2;
            } else {
                _cwnd += _mss;
            }
        }
    } else {
        // counter_consecutive_good_bytes = counter_consecutive_good_bytes / 2;
        if (use_super_fast_increase) {
            _cwnd += 2 * _mss * (LINK_SPEED_MODERN / 100);
            //_cwnd *= 2;
        } else {
            _cwnd += _mss;
        }
    }

    increasing = true;
    _list_fast_increase_event.push_back(
            std::make_pair(eventlist().now() / 1000, 1));
}

void UecSrc::adjust_window(simtime_picosec ts, bool ecn, simtime_picosec rtt) {

    if (rtt <= (_base_rtt + (_mss * 8 / LINK_SPEED_MODERN * 3 * 1000)) &&
        !ecn) {
        counter_consecutive_good_bytes += _mss;
    } else {
        target_window = _cwnd;
        counter_consecutive_good_bytes = 0;
        _consecutive_no_ecn = 0;
        increasing = false;
    }

    if (rtt >= _target_rtt) {
        consecutive_good_medium = 0;
    }

    // BTS Logic
    if (_bts_enabled) {
        if (ecn && _ignore_ecn_ack) {
            printf("BTS Case with ECN, ignore.");
            _cwnd += ((double)_mss / _cwnd) * 0.1 * _mss;
        } else if (counter_consecutive_good_bytes > _cwnd) {
            _cwnd = _maxcwnd;
            counter_consecutive_good_bytes = _cwnd / 2;
            exp_avg_bts = 0;
            exp_avg_route = 0;
            /*if (no_ecn_last_target_rtt() &&
                no_rtt_over_target_last_target_rtt()) {
                printf("Fast Increasen\n");
                _cwnd += _mss * ((double)_cwnd / _bdp) * 1;
                _consecutive_low_rtt = 0;
                _consecutive_no_ecn = 0;
            }*/
        } else if (!ecn) {
            // printf("Second Increase, %d", from);
            //_cwnd += ((double)_mss / _cwnd) * 1 * _mss;
            //_consecutive_no_ecn = 0;

            /*double ratio = (_bdp / (double)_cwnd);
            int threshold = max(1, (int)((_cwnd / ratio)));

            if (_consecutive_no_ecn >= threshold) {
                // printf("Actual Increase\n");
                _cwnd += _mss * (_cwnd / (double)_bdp) *
                         (_consecutive_no_ecn / threshold);
                _consecutive_no_ecn = 0;
            }
            */
            _cwnd += ((double)_mss / _cwnd) * 1 * _mss;

        } else {
            _cwnd += ((double)_mss / _cwnd) * 0 * _mss;
        }
    } else {
        if (ecn) {
            count_ecn_in_rtt++;
        }

        // Trimming Logic
        if (algorithm_type == "standard_trimming") {
            // Always small increase for fairness
            _cwnd += ((double)_mss / _cwnd) * 0.01 * _mss;

            // Check if we can fast drop if it is enabled and drop in case.
            if (use_fast_drop) {
                do_fast_drop(ecn);
            }

            // Normal, non fast drop cases. ECN
            if (ecn) {
                if (count_received >= ignore_for) {
                    reduce_cwnd(static_cast<double>(_cwnd) / _bdp * _mss);
                }
                // Fast Increase
            } else if ((counter_consecutive_good_bytes > target_window) &&
                       use_fast_increase) {
                fast_increase();
            } else if (rtt < _target_rtt) {
                if (use_fast_drop) {
                    if (count_received >= ignore_for) {
                        _cwnd += medium_increase(rtt);
                    }
                } else {
                    _cwnd += medium_increase(rtt);
                }
            } else if (rtt >= _target_rtt) {
                // We don't do anything in this case. Before we did the thing
                // below
                //_cwnd += ((double)_mss / _cwnd) * 1 * _mss;
            }

            // Delay Logic, Version A Logic
        } else if (algorithm_type == "delayA") {
            // printf("Name Running: UEC Version A\n");
            if (use_fast_drop) {
                if (count_received >= ignore_for) {
                    do_fast_drop(ecn);
                }
            }
            if ((counter_consecutive_good_bytes > target_window) &&
                use_fast_increase) {
                fast_increase();
                // Case 1
            } else if (!ecn && rtt < _target_rtt) {
                if (use_fast_drop) {
                    if (count_received >= ignore_for) {
                        _cwnd += medium_increase(rtt);
                    }
                } else {
                    _cwnd += medium_increase(rtt);
                }
                // Insta or Exp Avg (in Perm)
                // Case 2
            } else if (ecn && rtt > _target_rtt) {
                if (use_fast_drop) {
                    if (count_received >= ignore_for) {
                        reduce_cwnd(static_cast<double>(_cwnd) / _bdp * _mss);
                    }
                } else {
                    reduce_cwnd(static_cast<double>(_cwnd) / _bdp * _mss);
                }
                // Case 3
            } else if (ecn && rtt < _target_rtt) {
                if (use_fast_drop) {
                    if (count_received >= ignore_for) {
                        reduce_cwnd(static_cast<double>(_cwnd) / _bdp * _mss);
                    }
                } else {
                    reduce_cwnd(static_cast<double>(_cwnd) / _bdp * _mss);
                }
                // Case 4
            } else if (!ecn && rtt > _target_rtt) {
                if (ecn_last_rtt) {
                    if (use_fast_drop) {
                        if (count_received >= ignore_for) {
                            // reduce_cwnd(static_cast<double>(_cwnd) / _bdp *
                            //             _mss);
                        }
                    } else {
                        // reduce_cwnd(static_cast<double>(_cwnd) / _bdp *
                        // _mss);
                    }
                }
            }

            // Delay Logic, Version B Logic
        } else if (algorithm_type == "delayB") {

            bool can_decrease = true;

            if (ecn) {
                exp_avg_route =
                        alpha_route * 1024 + (1 - alpha_route) * exp_avg_route;
            } else {
                exp_avg_route = (1 - alpha_route) * exp_avg_route;
            }

            if (use_fast_drop) {
                if (count_received >= ignore_for) {
                    do_fast_drop(false);
                }
            }

            if (exp_avg_route >= 200) {
            } else {
                // printf("Not decreasing %lu\n", GLOBAL_TIME / 1000);
                //  can_decrease = false;
            }

            ideal_x = x_gain;

            if ((increasing ||
                 counter_consecutive_good_bytes > target_window) &&
                use_fast_increase) {
                fast_increase();
                // Case 1 RTT Based Increase
            } else if (!ecn && rtt < _target_rtt) {
                _cwnd += min(uint32_t((((_target_rtt - rtt) / (double)rtt) *
                                       y_gain * _mss * (_mss / (double)_cwnd))),
                             uint32_t(_mss));

                _cwnd += ((double)_mss / _cwnd) * ideal_x * _mss;

                /*_cwnd += min(((((double)_mss / _cwnd) * _mss) +
                              (_cwnd / _bdp * _mss)) *
                                     x_gain,
                             x_gain * _mss);*/

                _list_medium_increase_event.push_back(
                        std::make_pair(eventlist().now() / 1000, 1));
                count_case_1.push_back(
                        std::make_pair(eventlist().now() / 1000, 1));
                // Case 2 Hybrid Based Decrease || RTT Decrease
            } else if (ecn && rtt > _target_rtt) {
                if (count_received >= ignore_for && can_decrease) {
                    double scaling;
                    if (target_rtt_percentage_over_base == 20) {
                        scaling = 2.5;
                    } else if (target_rtt_percentage_over_base == 50) {
                        scaling = 4;
                    } else {
                        printf("Error, unknown Target value\n");
                        exit(0);
                    }
                    last_decrease =
                            min((w_gain * ((rtt - (double)_target_rtt) / rtt) *
                                 _mss) + _cwnd / (double)_bdp * z_gain * _mss,
                                (double)_mss);
                    _cwnd -= last_decrease;
                    count_case_2.push_back(
                            std::make_pair(eventlist().now() / 1000, 1));

                    // reduce_cwnd(static_cast<double>(_cwnd) / _bdp * _mss);
                    // printf("Case 2 from %d at %lu\n", from, GLOBAL_TIME /
                    // 1000);
                }
                // Case 3 Gentle Decrease (Window based)
            } else if (ecn && rtt < _target_rtt) {
                double scaling;
                if (target_rtt_percentage_over_base == 20) {
                    scaling = 2.5;
                    rtt = 1.5 * _base_rtt;
                } else if (target_rtt_percentage_over_base == 50) {
                    scaling = 4;
                    rtt = 1.65 * _base_rtt;
                } else {
                    printf("Error, unknown Target value\n");
                    exit(0);
                }
                if (count_received >= ignore_for && can_decrease) {
                    reduce_cwnd(static_cast<double>(_cwnd) / _bdp * _mss *
                                z_gain);
                    count_case_3.push_back(
                            std::make_pair(eventlist().now() / 1000, 1));
                    // printf("Case 3 from %d at %lu\n", from, GLOBAL_TIME /
                    // 1000);
                }
                // Case 4
            } else if (!ecn && rtt > _target_rtt) {
                // Do nothing but fairness
                if (count_received >= ignore_for) {
                    if (last_decrease != 0 && false) {
                        _cwnd += min(((double)_mss / _cwnd) * ideal_x * _mss,
                                     last_decrease / 2.0);
                    } else {
                        //_cwnd += ((double)_mss / _cwnd) * _mss;
                        //_cwnd = min(((double)_mss / _cwnd) * _mss * x_gain,
                        //           ((double)_cwnd / (4.0 * _mss)) * _mss);
                        //_cwnd += ((double)_mss / _cwnd) * _mss * ideal_x;
                        /*_cwnd += min(((((double)_mss / _cwnd) * _mss) +
                                      (_cwnd / _bdp * _mss)) *
                                             x_gain,
                                     x_gain * _mss);*/
                        // ideal_x = ideal_x * 0.5;
                        _cwnd += ((double)_mss / _cwnd) * ideal_x * _mss;
                    }
                    count_case_4.push_back(
                            std::make_pair(eventlist().now() / 1000, 1));
                }
            }

            // Delay Logic, Version C Logic
        } else if (algorithm_type == "delayC") {
            //
            if (use_fast_drop) {
                do_fast_drop(ecn);
            }
            if ((counter_consecutive_good_bytes > target_window) &&
                use_fast_increase) {
                fast_increase();
                // Case 1
            } else if (!ecn && rtt < _target_rtt) {
                _cwnd += min(uint32_t((((_target_rtt - rtt) / (double)rtt) *
                                       y_gain * _mss * (_mss / (double)_cwnd))),
                             uint32_t(_mss));
                _cwnd += ((double)_mss / _cwnd) * x_gain *
                         _mss; // For x 0.15, 0.5, 0.3
                _list_medium_increase_event.push_back(
                        std::make_pair(eventlist().now() / 1000, 1));
                // Case 2
            } else if (ecn && rtt > _target_rtt) {
                if (count_received >= ignore_for) {
                    double scaling = 0;
                    if (target_rtt_percentage_over_base == 20) {
                        scaling = 2.5;
                    } else if (target_rtt_percentage_over_base == 50) {
                        scaling = 4;
                    } else {
                        printf("Error, unknown Target value\n");
                        exit(0);
                    }
                    _cwnd -=
                            min(w_gain * (rtt - _target_rtt) / rtt, 1.0) * _mss;
                }
                // Case 3
            } else if (ecn && rtt < _target_rtt) {
                if (count_received >= ignore_for) {
                    double scaling = 0;
                    if (target_rtt_percentage_over_base == 20) {
                        scaling = 2.5;
                        rtt = 1.5 * _base_rtt;
                    } else if (target_rtt_percentage_over_base == 50) {
                        rtt = 1.65 * _base_rtt;
                        scaling = 4;
                    } else {
                        printf("Error, unknown Target value\n");
                        exit(0);
                    }
                    _cwnd -= min(scaling * 2 * (rtt - _target_rtt) / rtt, 1.0) *
                             _mss;
                }
            } else if (!ecn && rtt > _target_rtt) {
                // Do nothing
                _cwnd += ((double)_mss / _cwnd) * x_gain *
                         _mss; // For x 0.15, 0.5, 0.3
            }
            printf("Using DelayC");

            // Delay Logic, Version D Logic
        } else if (algorithm_type == "delayD") {
            printf("Name Running: STrack\n");
            int b = 5;
            uint64_t custom_target_delay =
                    _base_rtt * (1 + (target_rtt_percentage_over_base / 100.0));
            double scaling_a = _bdp / LINK_SPEED_MODERN * (_base_rtt / 1000);
            double scaling_b = double(_base_rtt) / custom_target_delay;
            double alpha_d = 8.0 * scaling_a * scaling_b / (_base_rtt / 1000);
            double ewma = 0.15;
            double y = 0.15 * scaling_a;
            printf("Alpha D is %lu %f %f %f\n", _bdp, scaling_a, scaling_b,
                   alpha_d);

            if (t_last_decrease == 0) {
                t_last_decrease = eventlist().now();
            }
            if (t_last_clear_byte == 0) {
                t_last_clear_byte = eventlist().now();
            }
            if (t_last_fairness == 0) {
                t_last_fairness = eventlist().now();
            }

            // D Logic
            can_fairness = (eventlist().now() - t_last_fairness) > _base_rtt;
            can_decrease = (eventlist().now() - t_last_decrease) > _base_rtt;
            can_clear_byte =
                    (eventlist().now() - t_last_clear_byte) > _base_rtt;
            rx_count += _mss;

            if (can_clear_byte) {
                printf("From %d - At %lu achieved BDP is %d\n", from,
                       eventlist().now() / 1000, rx_count);
                achieved_bdp = rx_count;
                rx_count = 0;
                t_last_clear_byte = eventlist().now();
            }

            if (avg_rtt == 0) {
                avg_rtt = rtt;
            } else {
                avg_rtt = avg_rtt * (1 - ewma) + ewma * rtt;
            }

            printf("Allowed to Decrease %d %d %d %d - RTT vs Current "
                   "AvgRTT vs "
                   "Target "
                   "%lu vs %lu vs %lu - Saved Acked %d\n",
                   ecn, can_decrease, avg_rtt > custom_target_delay,
                   _cwnd > achieved_bdp, rtt / 1000, avg_rtt / 1000,
                   custom_target_delay / 1000, achieved_bdp);
            if (rtt > custom_target_delay * 2 && !ecn) {
                _cwnd += ((double)b * _mss) / (_cwnd * _mss);
                printf("Increase 1 by %f\n", ((double)b / _cwnd) * _mss * _mss);
            } else if (rtt < custom_target_delay && !ecn) {
                printf("Increase 2 by %f\n",
                       (alpha_d *
                        ((custom_target_delay / 1000) - (rtt / 1000)) /
                        (double)_cwnd));
                _cwnd += alpha_d *
                         ((custom_target_delay / 1000) - (rtt / 1000)) /
                         (double)_cwnd;
            } else if (can_decrease && avg_rtt > custom_target_delay &&
                       _cwnd > achieved_bdp) {
                if (rtt > custom_target_delay * 1.5 && achieved_bdp > 0) {
                    _cwnd = achieved_bdp;
                    printf("Decreasing 1\n");
                } else if (rtt > custom_target_delay) {
                    printf("Decreasing 2 - Factor %f\n",
                           max(0.5, 1 - 0.8 * ((avg_rtt - custom_target_delay) /
                                               (double)avg_rtt)));
                    _cwnd = _cwnd *
                            max(0.5,
                                1 - 0.8 * ((avg_rtt - custom_target_delay) /
                                           (double)avg_rtt));
                }

                t_last_decrease = eventlist().now();
            }
            if (can_fairness) {
                _cwnd = _cwnd + (y / _mss);
                t_last_fairness = eventlist().now();
            }
        } else if (algorithm_type == "rtt") {
            printf("Name Running: RTT Only\n");
            int b = 5;
            uint64_t custom_target_delay =
                    _base_rtt * (1 + (target_rtt_percentage_over_base / 100.0));
            double scaling_a = _bdp / LINK_SPEED_MODERN * (_base_rtt / 1000);
            double scaling_b = double(_base_rtt) / custom_target_delay;
            double alpha_d = 8.0 * scaling_a * scaling_b / (_base_rtt / 1000);
            double ewma = 0.15;
            double y = 0.15 * scaling_a;
            printf("Alpha D is %lu %f %f %f\n", _bdp, scaling_a, scaling_b,
                   alpha_d);

            if (t_last_decrease == 0) {
                t_last_decrease = eventlist().now();
            }
            if (t_last_clear_byte == 0) {
                t_last_clear_byte = eventlist().now();
            }
            if (t_last_fairness == 0) {
                t_last_fairness = eventlist().now();
            }

            can_fairness = (eventlist().now() - t_last_fairness) > _base_rtt;
            can_decrease = (eventlist().now() - t_last_decrease) > _base_rtt;
            can_clear_byte =
                    (eventlist().now() - t_last_clear_byte) > _base_rtt;
            rx_count += _mss;

            if (use_fast_drop) {
                if (count_received >= ignore_for) {
                    do_fast_drop(false);
                }
            }

            if (rtt < _target_rtt) {
                _cwnd += min(uint32_t((((_target_rtt - rtt) / (double)rtt) *
                                       y_gain * _mss * (_mss / (double)_cwnd))),
                             uint32_t(_mss));
            } else if (can_decrease && rtt > custom_target_delay) {
                if (count_received >= ignore_for) {
                    _cwnd = _cwnd *
                            max(0.5, 1 - ((rtt - _target_rtt) / (double)rtt));
                    t_last_decrease = eventlist().now();
                }
            }
        } else if (algorithm_type == "ecn") {
            printf("Name Running: ECN Only\n");

            if (use_fast_drop) {
                if (count_received >= ignore_for) {
                    do_fast_drop(false);
                }
            }

            if ((counter_consecutive_good_bytes > target_window) &&
                use_fast_increase) {
                fast_increase();
            } else if (!ecn) {
                _cwnd += ((double)_mss / _cwnd) * _mss;
            } else if (ecn) {
                if (count_received >= ignore_for) {
                    reduce_cwnd(static_cast<double>(_mss) * 0.5);
                }
            }
        } else if (algorithm_type == "custom") {

            if (use_fast_drop) {
                if (count_received >= ignore_for) {
                    do_fast_drop(false);
                }
            }

            uint32_t increase_by = 0;
            uint32_t decrease_by = 0;

            if ((counter_consecutive_good_bytes > target_window) &&
                use_fast_increase) {
                fast_increase();
            } else if (rtt < _target_rtt) {
                if (count_received >= ignore_for) {
                    increase_by = min(
                            uint32_t((((_target_rtt - rtt) / (double)rtt) *
                                      y_gain * _mss * (_mss / (double)_cwnd))),
                            uint32_t(_mss));
                    _cwnd += increase_by +
                             (increase_by * 0 * (_mss / (double)_cwnd));
                    count_case_1.push_back(
                            std::make_pair(eventlist().now() / 1000, 1));
                }
            } else if (rtt > _target_rtt) {
                if (count_received >= ignore_for) {
                    decrease_by =
                            min((w_gain * ((rtt - (double)_target_rtt) / rtt) *
                                 _mss) + _cwnd / (double)_bdp * z_gain,
                                (double)_mss);
                    _cwnd -= decrease_by +
                             (decrease_by * 0 * (_cwnd / (double)_bdp));
                    count_case_2.push_back(
                            std::make_pair(eventlist().now() / 1000, 1));
                }
            }
            if (count_received >= ignore_for) {
                if (decrease_by != 0) {
                    _cwnd += max(((double)_mss / _cwnd) * x_gain * _mss,
                                 decrease_by * x_gain);
                } else if (increase_by != 0) {
                    _cwnd += max(((double)_mss / _cwnd) * x_gain * _mss,
                                 increase_by * x_gain);
                }
            }
        }
    }

    if (_cwnd > _maxcwnd) {
        _cwnd = _maxcwnd;
    }
    if (_cwnd <= _mss) {
        _cwnd = _mss;
    }
}

void UecSrc::drop_old_received() {
    if (true) {
        if (eventlist().now() > _target_rtt) {
            uint64_t lower_thresh = eventlist().now() - (_target_rtt * 1);
            while (!_received_ecn.empty() &&
                   std::get<0>(_received_ecn.front()) < lower_thresh) {
                _received_ecn.pop_front();
            }
        }
    } else {
        while (_received_ecn.size() > 10) {
            _received_ecn.pop_front();
        }
    }
}

bool UecSrc::no_ecn_last_target_rtt() {
    drop_old_received();
    for (const auto &[ts, ecn, size, rtt] : _received_ecn) {
        if (ecn) {
            return false;
        }
    }
    return true;
}

bool UecSrc::no_rtt_over_target_last_target_rtt() {
    drop_old_received();
    for (const auto &[ts, ecn, size, rtt] : _received_ecn) {
        if (rtt > _target_rtt) {
            return false;
        }
    }
    return true;
}

std::size_t UecSrc::getEcnInTargetRtt() {
    drop_old_received();
    std::size_t ecn_count = 0;
    for (const auto &[ts, ecn, size, rtt] : _received_ecn) {
        if (ecn) {
            ++ecn_count;
        }
    }
    return ecn_count;
}

bool UecSrc::ecn_congestion() {
    if (getEcnInTargetRtt() >= _received_ecn.size() / 2) {
        return true;
    }
    return false;
}

const string &UecSrc::nodename() { return _nodename; }

void UecSrc::connect(const Route *routeout, const Route *routeback,
                     UecSink &sink, simtime_picosec startTime) {
    _route = &routeout;

    assert(_route);
    _sink = &sink;
    _sink->connect(*this, routeback);

    eventlist().sourceIsPending(*this, startTime);
}

void UecSrc::startflow() {
    ideal_x = x_gain;
    printf("My Starting XGain %f\n", ideal_x);
    _flow_start_time = eventlist().now();
    send_packets();
}

const Route *UecSrc::get_path() {
    // TODO: add other ways to select paths
    // printf("Entropy Size %d\n", _good_entropies.size());
    if (_use_good_entropies && !_good_entropies.empty()) {
        // auto rt = _good_entropies[_next_good_entropy];
        // ++_next_good_entropy;
        // _next_good_entropy %= _good_entropies.size();
        auto rt = _good_entropies.back();
        _good_entropies.pop_back();
        return rt;
    }

    // Means we want to select a random one out of all paths, the original
    // idea
    if (_num_entropies == -1) {
        _crt_path = random() % _paths.size();
    } else {
        // Else we use our entropy array of a certain size and roud robin it
        _crt_path = _entropy_array[current_entropy];
        current_entropy = ++current_entropy % _num_entropies;
    }

    total_routes = _paths.size();
    return _paths.at(_crt_path);
}

void UecSrc::map_entropies() {
    for (int i = 0; i < _num_entropies; i++) {
        _entropy_array.push_back(random() % _paths.size());
    }
    printf("Printing my Paths: ");
    // fflush(stdout);
    for (int i = 0; i < _num_entropies; i++) {
        printf("%d - ", _entropy_array[i]);
    }
    printf("\n");
}

void UecSrc::send_packets() {
    //_list_cwd.push_back(std::make_pair(eventlist().now() / 1000, _cwnd));
    if (_rtx_pending) {
        retransmit_packet();
    }
    // printf("Sent Packet Called, %d\n", from);
    _list_unacked.push_back(std::make_pair(eventlist().now() / 1000, _unacked));
    unsigned c = _cwnd;

    while ( //_last_acked + c >= _highest_sent + _mss &&
            get_unacked() + _mss <= c && _highest_sent <= _flow_size + 1) {
        uint64_t data_seq = 0;

        // choose path
        const Route *rt = get_path();

        // create packet
        UecPacket *p = UecPacket::newpkt(_flow, *rt, _highest_sent + 1,
                                         data_seq, _mss);
        p->from = this->from;
        p->to = this->to;
        p->tag = this->tag;
        // printf("Sending packet %d %d %d - Name %s - Time %ld\n",
        // this->from, this->to, this->tag, _name.c_str(), (long
        // long)GLOBAL_TIME//);

        p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);
        p->set_ts(eventlist().now());

        // send packet
        _highest_sent += _mss; // XX beware wrapping
        _packets_sent += _mss;
        _unacked += _mss;

        // Getting time until packet is really sent
        // printf("Sent Packet, %d\n", from);
        PacketSink *sink = p->sendOn();
        HostQueue *q = dynamic_cast<HostQueue *>(sink);
        assert(q);
        uint32_t service_time = q->serviceTime(*p);
        _sent_packets.push_back(
                SentPacket(eventlist().now() + service_time + _rto, p->seqno(),
                           false, false, false));
        if (COLLECT_DATA) {
            for (size_t i = 0; i < rt->size(); i++) {
                if (i == 4) { // Intercept US TO CSf
                    PacketSink *sink = rt->at(i);
                    if (sink->nodename().find("US_0") != std::string::npos) {
                        std::regex pattern("CS_(\\d+)");
                        std::smatch matches;
                        if (std::regex_search(sink->nodename(), matches,
                                              pattern)) {
                            std::string numberStr = matches[1].str();
                            int number = std::stoi(numberStr);
                            us_to_cs.push_back(std::make_pair(
                                    eventlist().now() / 1000 +
                                            (service_time / 1000),
                                    number));
                        }
                    }
                }
                if (i == 2) { // Intercept US TO CS
                    PacketSink *sink = rt->at(i);
                    if (sink->nodename().find("LS_0") != std::string::npos) {
                        std::regex pattern("US_(\\d+)");
                        std::smatch matches;
                        if (std::regex_search(sink->nodename(), matches,
                                              pattern)) {
                            std::string numberStr = matches[1].str();
                            int number = std::stoi(numberStr);
                            ls_to_us.push_back(std::make_pair(
                                    eventlist().now() / 1000 +
                                            (service_time / 1000),
                                    number));
                        }
                    }
                }
            }
        }

        if (_rtx_timeout == timeInf) {
            update_rtx_time();
        }
    }
}

void UecSrc::set_paths(vector<const Route *> *rt) {
    _paths.clear();

    for (const Route *route : *rt) {
        Route *t = new Route(*route, *_sink);
        t->add_endpoints(this, _sink);
        _paths.push_back(t);
    }
    map_entropies();
}

void UecSrc::apply_timeout_penalty() {
    if (_trimming_enabled) {
        reduce_cwnd(_mss);
    } else {
        _cwnd = _mss;
    }
}

void UecSrc::rtx_timer_hook(simtime_picosec now, simtime_picosec period) {
    // #ifndef RESEND_ON_TIMEOUT
    //     return; // TODO: according to ndp.cpp, rtx is not necessary with
    //     RTS. Check
    //             // if this applies to us
    // #endif

    if (_highest_sent == 0)
        return;
    if (_rtx_timeout == timeInf || now + period < _rtx_timeout)
        return;

    // here we can run into phase effects because the timer is checked
    // only periodically for ALL flows but if we keep the difference
    // between scanning time and real timeout time when restarting the
    // flows we should minimize them !
    if (!_rtx_timeout_pending) {
        _rtx_timeout_pending = true;
        apply_timeout_penalty();

        cout << "At " << timeAsUs(now) << "us RTO " << timeAsUs(_rto)
             << "us RTT " << timeAsUs(_rtt) << "us SEQ " << _last_acked / _mss
             << " CWND " << _cwnd / _mss << " Flow ID " << str() << endl;

        _cwnd = _mss;

        // check the timer difference between the event and the real value
        simtime_picosec too_early = _rtx_timeout - now;
        if (now > _rtx_timeout) {
            // This might happen because we hold on retransmitting if we
            // have enough packets in flight cout << "late_rtx_timeout: " <<
            // _rtx_timeout << " now: " << now
            //     << " now+rto: " << now + _rto << " rto: " << _rto <<
            //     endl;
            too_early = 0;
        }
        eventlist().sourceIsPendingRel(*this, too_early);
    }
}

bool UecSrc::resend_packet(std::size_t idx) {

    if (get_unacked() >= _cwnd) {
        // printf("This si FALSE\n");
        return false;
    }
    assert(!_sent_packets[idx].acked);

    // this will cause retransmission not only of the offending
    // packet, but others close to timeout
    _rto_margin = _rtt / 2;

    const Route *rt;
    // if (_use_good_entropies && !_good_entropies.empty()) {
    //     rt = _good_entropies[_next_good_entropy];
    //     ++_next_good_entropy;
    //     _next_good_entropy %= _good_entropies.size();
    // } else {
    rt = get_path();
    // }
    // Getting time until packet is really sent
    _unacked += _mss;
    UecPacket *p = UecPacket::newpkt(_flow, *rt, _sent_packets[idx].seqno, 0,
                                     _mss, true);
    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATE);
    PacketSink *sink = p->sendOn();
    HostQueue *q = dynamic_cast<HostQueue *>(sink);
    assert(q);
    uint32_t service_time = q->serviceTime(*p);
    if (_sent_packets[idx].nacked) {
        --_nack_rtx_pending;
        _sent_packets[idx].nacked = false;
    }
    _sent_packets[idx].timer = eventlist().now() + service_time + _rto;
    _sent_packets[idx].timedOut = false;
    update_rtx_time();
    return true;
}

// retransmission for timeout
void UecSrc::retransmit_packet() {
    _rtx_pending = false;
    for (std::size_t i = 0; i < _sent_packets.size(); ++i) {
        auto &sp = _sent_packets[i];
        if (_rtx_timeout_pending && !sp.acked && !sp.nacked &&
            sp.timer <= eventlist().now() + _rto_margin) {
            _cwnd = _mss;
            sp.timedOut = true;
            reduce_unacked(_mss);
        }
        if (!sp.acked && (sp.timedOut || sp.nacked)) {
            if (!resend_packet(i)) {
                _rtx_pending = true;
            }
        }
    }
    _rtx_timeout_pending = false;
}

/**********
 * UecSink *
 **********/

UecSink::UecSink() : DataReceiver("sink"), _cumulative_ack{0}, _drops{0} {
    _nodename = "uecsink";
}

void UecSink::send_nack(simtime_picosec ts, bool marked, UecAck::seq_t seqno,
                        UecAck::seq_t ackno, const Route *rt) {

    UecNack *nack = UecNack::newpkt(_src->_flow, *rt, seqno, ackno, 0);
    nack->is_ack = false;
    nack->flow().logTraffic(*nack, *this, TrafficLogger::PKT_CREATESEND);
    nack->set_ts(ts);
    if (marked) {
        nack->set_flags(ECN_ECHO);
    } else {
        nack->set_flags(0);
    }

    nack->sendOn();
}

bool UecSink::already_received(UecPacket &pkt) {
    UecPacket::seq_t seqno = pkt.seqno();

    if (seqno <= _cumulative_ack) { // TODO: this assumes
                                    // that all data packets
                                    // have the same size
        return true;
    }
    for (auto it = _received.begin(); it != _received.end(); ++it) {
        if (seqno == *it) {
            return true; // packet received OOO
        }
    }
    return false;
}

void UecSink::receivePacket(Packet &pkt) {
    switch (pkt.type()) {
    case UECACK:
    case UECNACK:
        // bounced, ignore
        pkt.free();
        // printf("Free4\n");
        // fflush(stdout);
        return;
    case UEC:
        // do what comes after the switch
        if (pkt.bounced()) {
            printf("Bounced at Sink, no sense\n");
        }
        break;
    default:
        std::cout << "unknown packet receive with type code: " << pkt.type()
                  << "\n";
        pkt.free();
        // printf("Free5\n");
        // fflush(stdout);
        return;
    }
    UecPacket *p = dynamic_cast<UecPacket *>(&pkt);
    UecPacket::seq_t seqno = p->seqno();
    UecPacket::seq_t ackno = p->seqno() + p->data_packet_size() - 1;
    simtime_picosec ts = p->ts();

    bool marked = p->flags() & ECN_CE;

    // TODO: consider different ways to select paths
    auto crt_path = random() % _paths.size();
    if (already_received(*p)) {
        // duplicate retransmit
        if (_src->supportsTrimming()) { // we can assume
                                        // that they have
                                        // been configured
                                        // similarly, or
                                        // exchanged
                                        // information about
                                        // options somehow
            send_ack(ts, marked, 1, _cumulative_ack, _paths.at(crt_path),
                     pkt.get_route());
        }
        return;
    }

    // packet was trimmed
    if (pkt.header_only()) {
        send_nack(ts, marked, seqno, ackno, _paths.at(crt_path));
        pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_RCVDESTROY);
        p->free();
        // printf("Free6\n");
        // fflush(stdout);
        // cout << "trimmed packet";
        return;
    }

    int size = p->data_packet_size();
    // pkt._flow().logTraffic(pkt, *this,
    // TrafficLogger::PKT_RCVDESTROY);
    p->free();
    // printf("Free7\n");
    // fflush(stdout);

    _packets += size;

    if (seqno == _cumulative_ack + 1) { // next expected seq no
        _cumulative_ack = seqno + size - 1;
        seqno = 1;

        // handling packets received OOO
        while (!_received.empty() && _received.front() == _cumulative_ack + 1) {
            _received.pop_front();
            _cumulative_ack += size; // this assumes that all
                                     // packets have the same size
        }
        ackno = _cumulative_ack;
    } else if (seqno < _cumulative_ack + 1) { // already ack'ed
        // this space intentionally left empty
        seqno = 1;
        ackno = _cumulative_ack;
    } else { // not the next expected sequence number
        // TODO: what to do when a future packet is
        // received?
        if (_received.empty()) {
            _received.push_front(seqno);
            _drops += (1000 + seqno - _cumulative_ack - 1) /
                      1000; // TODO: figure out what is this
                            // calculating exactly
        } else if (seqno > _received.back()) {
            _received.push_back(seqno);
        } else {
            for (auto it = _received.begin(); it != _received.end(); ++it) {
                if (seqno == *it)
                    break; // bad retransmit
                if (seqno < (*it)) {
                    _received.insert(it, seqno);
                    break;
                }
            }
        }
    }
    // TODO: reverse_route is likely sending the packet
    // through the same exact links, which is not correct in
    // Packet Spray, but there doesn't seem to be a good,
    // quick way of doing that in htsim printf("Ack Sending
    // From %d - %d\n", this->from,
    send_ack(ts, marked, seqno, ackno, _paths.at(crt_path), pkt.get_route());
}

void UecSink::send_ack(simtime_picosec ts, bool marked, UecAck::seq_t seqno,
                       UecAck::seq_t ackno, const Route *rt,
                       const Route *inRoute) {

    UecAck *ack = UecAck::newpkt(_src->_flow, *rt, seqno, ackno, 0);
    ack->inRoute = inRoute;
    ack->is_ack = true;
    ack->flow().logTraffic(*ack, *this, TrafficLogger::PKT_CREATESEND);
    ack->set_ts(ts);

    // printf("Sending ACK at %lu\n", GLOBAL_TIME / 1000);

    if (marked) {
        // printf("ACK - ECN\n");
        ack->set_flags(ECN_ECHO);
    } else {
        // printf("ACK - NO ECN\n");
        ack->set_flags(0);
    }

    ack->from = this->from;
    ack->to = this->to;
    ack->tag = this->tag;
    ack->sendOn();
}

const string &UecSink::nodename() { return _nodename; }

uint64_t UecSink::cumulative_ack() { return _cumulative_ack; }

uint32_t UecSink::drops() { return _drops; }

void UecSink::connect(UecSrc &src, const Route &route) {
    _src = &src;
    _route = &route;
    _cumulative_ack = 0;
    _drops = 0;
}

void UecSink::set_paths(vector<const Route *> *rt) {
    _paths.clear();

    for (const Route *route : *rt) {
        Route *t = new Route(*route, *_src);
        t->add_endpoints(this, _src);
        _paths.push_back(t);
    }
}

/**********************
 * UecRtxTimerScanner *
 **********************/

UecRtxTimerScanner::UecRtxTimerScanner(simtime_picosec scanPeriod,
                                       EventList &eventlist)
        : EventSource(eventlist, "RtxScanner"), _scanPeriod{scanPeriod} {
    eventlist.sourceIsPendingRel(*this, 0);
}

void UecRtxTimerScanner::registerUec(UecSrc &uecsrc) {
    _uecs.push_back(&uecsrc);
}

void UecRtxTimerScanner::doNextEvent() {
    simtime_picosec now = eventlist().now();
    uecs_t::iterator i;
    for (i = _uecs.begin(); i != _uecs.end(); i++) {
        (*i)->rtx_timer_hook(now, _scanPeriod);
    }
    eventlist().sourceIsPendingRel(*this, _scanPeriod);
}
