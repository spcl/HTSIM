// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "swifttrimming.h"
#include "ecn.h"
#include "queue.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <math.h>
#include <regex>
#include <stdio.h>
#include <utility>

#define timeInf 0

// Parameters
std::string SwiftTrimmingSrc::queue_type = "composite";
std::string SwiftTrimmingSrc::algorithm_type = "standard_trimming";
bool SwiftTrimmingSrc::use_fast_drop = false;
int SwiftTrimmingSrc::fast_drop_rtt = 1;
bool SwiftTrimmingSrc::do_jitter = false;
bool SwiftTrimmingSrc::do_exponential_gain = false;
bool SwiftTrimmingSrc::use_fast_increase = false;
bool SwiftTrimmingSrc::use_super_fast_increase = false;
double SwiftTrimmingSrc::exp_gain_value_med_inc = 1;
double SwiftTrimmingSrc::jitter_value_med_inc = 1;
double SwiftTrimmingSrc::delay_gain_value_med_inc = 5;
int SwiftTrimmingSrc::target_rtt_percentage_over_base = 50;

double SwiftTrimmingSrc::y_gain = 1;
double SwiftTrimmingSrc::x_gain = 0.15;
double SwiftTrimmingSrc::z_gain = 1;
double SwiftTrimmingSrc::w_gain = 1;
bool SwiftTrimmingSrc::disable_case_3 = false;
double SwiftTrimmingSrc::starting_cwnd = 1;
double SwiftTrimmingSrc::bonus_drop = 1;
double SwiftTrimmingSrc::buffer_drop = 1.2;
int SwiftTrimmingSrc::ratio_os_stage_1 = 1;

RouteStrategy SwiftTrimmingSrc::_route_strategy = NOT_SET;
RouteStrategy SwiftTrimmingSink::_route_strategy = NOT_SET;

SwiftTrimmingSrc::SwiftTrimmingSrc(SwiftTrimmingLogger *logger,
                                   TrafficLogger *pktLogger,
                                   EventList &eventList, uint64_t rtt,
                                   uint64_t bdp, uint64_t queueDrainTime,
                                   int hops)
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
    _base_rtt =
            ((_hop_count * LINK_DELAY_MODERN) +
             (PKT_SIZE_MODERN * 8 / LINK_SPEED_MODERN * (_hop_count - 1)) +
             (PKT_SIZE_MODERN * 8 / (LINK_SPEED_MODERN / ratio_os_stage_1)) +
             (_hop_count * LINK_DELAY_MODERN) +
             (64 * 8 / LINK_SPEED_MODERN * (_hop_count - 1)) +
             (64 * 8 / (LINK_SPEED_MODERN / ratio_os_stage_1))) *
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

    _next_pathid = 1;

    _bdp = (_base_rtt * LINK_SPEED_MODERN / 8) / 1000;
    _queue_size = _bdp; // Temporary
    initial_x_gain = x_gain;

    _maxcwnd = starting_cwnd;
    _cwnd = starting_cwnd;
    _consecutive_low_rtt = 0;
    target_window = _cwnd;
    _target_based_received = true;

    printf("Link Delay %d - Link Speed %lu - Pkt Size %d - Base RTT %lu - "
           "Target RTT is %lu - BDP %lu - CWND %u - Hops %d\n",
           LINK_DELAY_MODERN, LINK_SPEED_MODERN, PKT_SIZE_MODERN, _base_rtt,
           _target_rtt, _bdp, _cwnd, _hop_count);

    _max_good_entropies = 10; // TODO: experimental value
    _enableDistanceBasedRtx = false;
    f_flow_over_hook = nullptr;

    if (queue_type == "composite_bts") {
        _bts_enabled = true;
    } else {
        _bts_enabled = false;
    }

    // swift cc init
    _ai = 2;        // increase constant.  Value is a guess
    _beta = 1;      // decrease constant.  Value is a guess
    _max_mdf = 0.5; // max multiplicate decrease factor.  Value is a guess
    _base_delay = _base_rtt; // configured base target delay.  To be confirmed
                             // by experiment - reproduce fig 17
    _h = _base_delay / 6.55 / 6; // path length scaling constant.  Value is a
                                 // guess, will be clarified by experiment

    // flow scaling
    _fs_range = 5 * _base_delay;
    _fs_min_cwnd = 0.1;               // note: in packets
    _fs_max_cwnd = (_maxcwnd / _mss); // note: in packets
    _fs_alpha = _fs_range /
                ((1.0 / sqrt(_fs_min_cwnd)) - (1.0 / sqrt(_fs_max_cwnd)));

    // cout << "_fs_alpha: " << _fs_alpha << endl;
    _fs_beta = -_fs_alpha / sqrt(_fs_max_cwnd);
}

void SwiftTrimmingSrc::set_end_trigger(Trigger &end_trigger) {
    _end_trigger = &end_trigger;
}

// Add deconstructor and save data once we are done.
SwiftTrimmingSrc::~SwiftTrimmingSrc() {
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

void SwiftTrimmingSrc::doNextEvent() { startflow(); }

std::size_t SwiftTrimmingSrc::get_sent_packet_idx(uint32_t pkt_seqno) {
    for (std::size_t i = 0; i < _sent_packets.size(); ++i) {
        if (pkt_seqno == _sent_packets[i].seqno) {
            return i;
        }
    }
    return _sent_packets.size();
}

void SwiftTrimmingSrc::update_rtx_time() {
    _rtx_timeout = timeInf;
    for (const auto &sp : _sent_packets) {
        auto timeout = sp.timer;
        if (!sp.acked && !sp.nacked && !sp.timedOut &&
            (timeout < _rtx_timeout || _rtx_timeout == timeInf)) {
            _rtx_timeout = timeout;
        }
    }
}

void SwiftTrimmingSrc::mark_received(SwiftTrimmingAck &pkt) {
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
        _sent_packets[i] = SentPacketSwift(timer, seqno, true, false, false);
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

void SwiftTrimmingSrc::add_ack_path(const Route *rt) {
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

void SwiftTrimmingSrc::set_traffic_logger(TrafficLogger *pktlogger) {
    _flow.set_logger(pktlogger);
}

void SwiftTrimmingSrc::reduce_cwnd(uint64_t amount) {
    if (_cwnd >= amount + _mss) {
        _cwnd -= amount * 1;
    } else {
        _cwnd = _mss;
    }
}

void SwiftTrimmingSrc::reduce_unacked(uint64_t amount) {
    if (_unacked >= amount) {
        _unacked -= amount;
    } else {
        _unacked = 0;
    }
}

void SwiftTrimmingSrc::check_limits_cwnd() {
    // Upper Limit
    if (_cwnd > _maxcwnd) {
        _cwnd = _maxcwnd;
    }
    // Lower Limit
    if (_cwnd < _mss) {
        _cwnd = _mss;
    }
}

void SwiftTrimmingSrc::do_fast_drop(bool ecn_or_trimmed) {

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
        /*printf("Using Fast Drop1 - Flow %d@%d@%d, Ecn %d, CWND %d, Saved "
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
               previous_window_end / 1000, _base_rtt / 1000);*/
        saved_trimmed_bytes = 0;
        if ((ecn_or_trimmed || need_fast_drop) &&
            (saved_acked_bytes > 0 ||
             (saved_acked_bytes == 0 && previous_window_end != 0)) &&
            previous_window_end != 0) {

            saved_acked_bytes =
                    saved_acked_bytes *
                    ((double)_target_rtt /
                     (eventlist().now() - previous_window_end + _target_rtt));

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
                        (double)_mss);

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
            check_limits_cwnd();

            // Update XGAIN
            x_gain = min(initial_x_gain,
                         (_queue_size / 5.0) / (_mss * ((double)_bdp / _cwnd)));
        }
    }
}

void SwiftTrimmingSrc::processNack(SwiftTrimmingNack &pkt) {

    printf("Nack from %d - ECN 1, Path %d\n", from, pkt.pathid_echo);
    count_trimmed_in_rtt++;
    consecutive_nack++;
    trimmed_last_rtt++;
    consecutive_good_medium = 0;
    acked_bytes += 64;
    saved_trimmed_bytes += 64;

    if (count_received >= ignore_for) {
        need_fast_drop = true;
    }

    // printf("Just NA CK from %d at %lu\n", from, eventlist().now() / 1000);

    // Reduce Window Or Do Fast Drop
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
    check_limits_cwnd();

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

    bool success = resend_packet(i);
    if (!_rtx_pending && !success) {
        _rtx_pending = true;
    }
}

/* Choose a route for a particular packet */
int SwiftTrimmingSrc::choose_route() {

    switch (_route_strategy) {
    case PULL_BASED: {
        /* this case is basically SCATTER_PERMUTE, but avoiding bad paths. */

        assert(_paths.size() > 0);
        if (_paths.size() == 1) {
            // special case - no choice
            return 0;
        }
        // otherwise we've got a choice
        _crt_path++;
        if (_crt_path == _paths.size()) {
            // permute_paths();
            _crt_path = 0;
        }
        uint32_t path_id = _path_ids[_crt_path];
        _avoid_score[path_id] = _avoid_ratio[path_id];
        int ctr = 0;
        while (_avoid_score[path_id] > 0 /* && ctr < 2*/) {
            printf("as[%d]: %d\n", path_id, _avoid_score[path_id]);
            _avoid_score[path_id]--;
            ctr++;
            // re-choosing path
            cout << "re-choosing path " << path_id << endl;
            _crt_path++;
            if (_crt_path == _paths.size()) {
                // permute_paths();
                _crt_path = 0;
            }
            path_id = _path_ids[_crt_path];
            _avoid_score[path_id] = _avoid_ratio[path_id];
        }
        // cout << "AS: " << _avoid_score[path_id] << " AR: " <<
        // _avoid_ratio[path_id] << endl;
        assert(_avoid_score[path_id] == 0);
        break;
    }
    case SCATTER_RANDOM:
        // ECMP
        assert(_paths.size() > 0);
        _crt_path = random() % _paths.size();
        break;
    case SCATTER_PERMUTE:
    case SCATTER_ECMP:
        // Cycle through a permutation.  Generally gets better load balancing
        // than SCATTER_RANDOM.
        _crt_path++;
        assert(_paths.size() > 0);
        if (_crt_path / 1 == _paths.size()) {
            // permute_paths();
            _crt_path = 0;
        }
        break;
    case ECMP_FIB:
        // Cycle through a permutation.  Generally gets better load balancing
        // than SCATTER_RANDOM.
        _crt_path++;
        if (_crt_path == _paths.size()) {
            // permute_paths();
            _crt_path = 0;
        }
        break;
    case ECMP_RANDOM2_ECN: {
        uint64_t allpathssizes = _mss * _paths.size();
        if (_highest_sent < max(_maxcwnd, allpathssizes)) {
            /* printf("Trying this for %d // Highest Sent %d - cwnd %d - "
                    "allpathsize %d\n",
                    from, _highest_sent, _maxcwnd, allpathssizes);*/
            _crt_path++;
            // printf("Trying this for %d\n", from);
            if (_crt_path == _paths.size()) {
                // permute_paths();
                _crt_path = 0;
            }
        } else {
            if (_next_pathid == -1) {
                assert(_paths.size() > 0);
                _crt_path = random() % _paths.size();
                // printf("New Path %d is %d\n", from, _crt_path);
            } else {
                // printf("Recycling Path %d is %d\n", from, _next_pathid);
                _crt_path = _next_pathid;
            }
        }
        break;
    }
    case ECMP_RANDOM_ECN: {
        if (_highest_sent < _maxcwnd) { //_mss*_paths.size()
            _crt_path++;
            if (_crt_path == _paths.size()) {
                // permute_paths();
                _crt_path = 0;
            }
        } else {
            if (_next_pathid == -1) {
                assert(_paths.size() > 0);
                _crt_path = random() % _paths.size();
            } else {
                _crt_path = _next_pathid;
            }
        }
        break;
    }
    case ECMP_FIB_ECN: {
        // Cycle through a permutation, but use ECN to skip paths
        while (1) {
            _crt_path++;
            if (_crt_path == _paths.size()) {
                // permute_paths();
                _crt_path = 0;
            }
            if (_path_ecns[_path_ids[_crt_path]] > 0) {
                _path_ecns[_path_ids[_crt_path]]--;
                /*
                if (_log_me) {
                    cout << eventlist().now() << " skipped " <<
                _path_ids[_crt_path] << " " << _path_ecns[_path_ids[_crt_path]]
                << endl;
                }
                */
            } else {
                // eventually we'll find one that's zero
                break;
            }
        }
        break;
    }
    case SINGLE_PATH:
        abort(); // not sure if this can ever happen - if it can, remove this
                 // line
    case REACTIVE_ECN:
        return _crt_path;
    case NOT_SET:
        abort(); // shouldn't be here at all
    default:
        abort();
        break;
    }

    return _crt_path / 1;
}

int SwiftTrimmingSrc::next_route() {
    // used for reactive ECN.
    // Just move on to the next path blindly
    assert(_route_strategy == REACTIVE_ECN);
    _crt_path++;
    assert(_paths.size() > 0);
    if (_crt_path == _paths.size()) {
        // permute_paths();
        _crt_path = 0;
    }
    return _crt_path;
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

void SwiftTrimmingSrc::processBts(SwiftTrimmingPacket *pkt) {
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
        // //fflush(stdout);
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
    // //fflush(stdout);
}

void SwiftTrimmingSrc::processAck(SwiftTrimmingAck &pkt, bool force_marked) {
    SwiftTrimmingAck::seq_t seqno = pkt.ackno();
    simtime_picosec ts = pkt.ts();

    consecutive_nack = 0;
    bool marked = pkt.flags() &
                  ECN_ECHO; // ECN was marked on data packet and echoed on ACK

    /*printf("Packet from %d - Seq %d %d is ECN Marked %d - Time %lu - Next "
           "Window End "
           "%lu\n",
           from, pkt.seqno(), pkt.ackno(), marked, GLOBAL_TIME / 1000,
           next_window_end / 1000);*/

    uint64_t newRtt = eventlist().now() - ts;
    mark_received(pkt);

    count_total_ack++;
    if (marked) {
        count_total_ecn++;
        consecutive_good_medium = 0;
    }

    if (from == 0 && count_total_ack % 10 == 0) {
        // printf("Currently at Pkt %d\n", count_total_ack);
        //  fflush(stdout);
    }

    if (!marked) {
        _consecutive_no_ecn += _mss;
        _next_pathid = pkt.pathid_echo;
    } else {
        // printf("Ack %d - ECN 1, Path %d\n", from, pkt.pathid_echo);
        _next_pathid = -1;
        ecn_last_rtt = true;
        _consecutive_no_ecn = 0;
    }

    if (COLLECT_DATA) {
        _received_ecn.push_back(
                std::make_tuple(eventlist().now(), marked, _mss, newRtt));
        _list_rtt.push_back(std::make_tuple(
                eventlist().now() / 1000, newRtt / 1000, pkt.seqno(),
                pkt.ackno(), _base_rtt / 1000, _target_rtt / 1000));
    }

    if (seqno >= _flow_size && _sent_packets.empty() && !_flow_finished) {
        _flow_finished = true;
        if (f_flow_over_hook) {
            f_flow_over_hook(pkt);
        }

        cout << "Flow " << nodename() << " finished at "
             << timeAsMs(eventlist().now()) << endl;
        cout << "Flow " << nodename() << " completion time is "
             << timeAsMs(eventlist().now() - _flow_start_time) << endl;

        printf("Completion Time Flow is %lu - Overall Time %lu\n",
               eventlist().now() - _flow_start_time, GLOBAL_TIME);
        printf("Overall Completion at %lu\n", GLOBAL_TIME);
        if (_end_trigger) {
            _end_trigger->activate();
        }

        return;
    }

    if (seqno > _last_acked || true) { // TODO: new ack, we don't care about
                                       // ordering for now. Check later though
        if (seqno >= _highest_sent) {
            _highest_sent = seqno;
        }

        _last_acked = pkt.ackno();

        _list_cwd.push_back(std::make_pair(eventlist().now() / 1000, _cwnd));
        // printf("Window Is %d - From %d To %d\n", _cwnd, from, to);
        adjust_window(ts, marked, newRtt, pkt.ackno());
        acked_bytes += _mss;
        good_bytes += _mss;

        _effcwnd = _cwnd;
        // printf("Received From %d - Sending More\n", from);
        send_packets();
        return; // TODO: if no further code, this can be removed
    }
}

uint64_t SwiftTrimmingSrc::get_unacked() {
    // return _unacked;
    uint64_t missing = 0;
    for (const auto &sp : _sent_packets) {
        if (!sp.acked && !sp.nacked && !sp.timedOut) {
            missing += _mss;
        }
    }
    return missing;
}

void SwiftTrimmingSrc::receivePacket(Packet &pkt) {
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
        _logger->logSwiftTrimming(*this,
                                  SwiftTrimmingLogger::SWIFTTRIMMING_RCV);
    }
    switch (pkt.type()) {
    case SWIFTTRIMMING:
        // BTS
        if (_bts_enabled) {
            if (pkt.bounced()) {
                processBts((SwiftTrimmingPacket *)(&pkt));
                counter_consecutive_good_bytes = 0;
                increasing = false;
            }
        }
        break;
    case SWIFTTRIMMINGACK:
        // fflush(stdout);
        count_received++;
        processAck(dynamic_cast<SwiftTrimmingAck &>(pkt), false);
        pkt.free();
        break;
    case SWIFTTRIMMINGNACK:
        printf("NACK %d@%d@%d\n", from, to, tag);
        // fflush(stdout);
        if (_trimming_enabled) {
            _next_pathid = -1;
            count_received++;
            processNack(dynamic_cast<SwiftTrimmingNack &>(pkt));
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
    // //fflush(stdout);
}

uint32_t SwiftTrimmingSrc::medium_increase(simtime_picosec rtt) {
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

    // Delay Gain Automated (1 / (target_ratio - 1)) vs manually set at 0
    // (put close to target_rtt)

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

void SwiftTrimmingSrc::fast_increase() {
    if (use_fast_drop) {
        if (count_received > ignore_for) {
            // counter_consecutive_good_bytes =
            // counter_consecutive_good_bytes / 2;
            if (use_super_fast_increase) {
                _cwnd += 4 * _mss * (LINK_SPEED_MODERN / 100);
                //_cwnd *= 2;
            } else {
                _cwnd += _mss;
            }
        }
    } else {
        // counter_consecutive_good_bytes = counter_consecutive_good_bytes /
        // 2;
        if (use_super_fast_increase) {
            _cwnd += 4 * _mss * (LINK_SPEED_MODERN / 100);
            //_cwnd *= 2;
        } else {
            _cwnd += _mss;
        }
    }

    increasing = true;
    _list_fast_increase_event.push_back(
            std::make_pair(eventlist().now() / 1000, 1));
}

void SwiftTrimmingSrc::update_rtt(simtime_picosec delay) {
    // calculate TCP-like RTO.  Not clear this is right for Swift
    if (delay != 0) {
        if (_rtt > 0) {
            uint64_t abs;
            if (delay > _rtt)
                abs = delay - _rtt;
            else
                abs = _rtt - delay;

            _mdev = 3 * _mdev / 4 + abs / 4;
            _rtt = 7 * _rtt / 8 + delay / 8;
            //_rto = _rtt + 4 * _mdev;
        } else {
            _rtt = delay;
            _mdev = delay / 2;
            //_rto = _rtt + 4 * _mdev;
        }
    }

    // if (_rto < _min_rto)
    //_rto = _min_rto;
}

simtime_picosec SwiftTrimmingSrc::targetDelay(uint32_t cwnd,
                                              const Route &route) {
    // note fs_delay can be negative, so don't use simtime_picosec here!
    double fs_delay = _fs_alpha / sqrt(cwnd / _mss) + _fs_beta;

    if (fs_delay > _fs_range) {
        fs_delay = _fs_range;
    }
    if (fs_delay < 0.0) {
        fs_delay = 0.0;
    }

    if (cwnd == 0) {
        fs_delay = 0.0;
    }

    simtime_picosec hop_delay = 1 * _h;
    /*printf("Target Component: Hop Count %d - h %f - Base Delay %lu - FsDelay "
           "%f - HopDelay %lu\n",
           1, _h, _base_rtt, fs_delay, hop_delay);*/
    return _base_rtt + fs_delay + hop_delay;
}

void SwiftTrimmingSrc::adjust_window(simtime_picosec ts, bool ecn,
                                     simtime_picosec rtt,
                                     SwiftTrimmingAck::seq_t ackno) {

    //_prev_cwnd = _cwnd;
    simtime_picosec now = eventlist().now();
    _can_decrease = (now - _last_decrease) >= _rtt;

    // compute rtt
    uint64_t delay = now - ts;
    update_rtt(delay);

    // Swift cwnd calculation.  Doing this here does it for every ack, no matter
    // if we're in fast recovery or not.  Need to be careful.
    _retransmit_cnt = 0;
    simtime_picosec target_delay = targetDelay(_cwnd, *_route);

    /*printf("Current Delay %lu vs Target Delay %lu - Cwnd %d - %f \n", delay,
           target_delay, _cwnd,
           max(1 - _beta * (delay - target_delay) / delay, 1 - _max_mdf));*/

    if (delay < target_delay) {
        // cout << "ackno " << ackno << " cwnd " << _swift_cwnd << " inf " <<
        // _inflate << " last_acked " <<  _last_acked << endl;
        int num_acked = ackno - _last_acked;
        // cout << "num_acked " << num_acked << endl;
        if (num_acked < 0)
            num_acked = 0; // doesn't make sense to do additive increase with
                           // negative num_acked.
        if (_cwnd >= _mss) {
            /*printf("Increasing by %f - Ai %f - NumAcked %d --> Acko %d, "
                   "LastAcked %d, NumAcked %d\n",
                   (_mss * _ai * 1) / _cwnd * _mss, _ai, 1, ackno, _last_acked,
                   1);*/
            _cwnd += ((_mss * _ai * 1) / _cwnd) * _mss;
        } else {
            printf("Below 1 MSS Window, not supported Yet\n");
            abort();
        }
    } else if (_can_decrease) {
        // multiplicative decrease
        _cwnd = _cwnd *
                max(1 - _beta * (delay - target_delay) / delay, 1 - _max_mdf);
        _last_decrease = eventlist().now();
    }

    check_limits_cwnd();
}

void SwiftTrimmingSrc::drop_old_received() {
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

bool SwiftTrimmingSrc::no_ecn_last_target_rtt() {
    drop_old_received();
    for (const auto &[ts, ecn, size, rtt] : _received_ecn) {
        if (ecn) {
            return false;
        }
    }
    return true;
}

bool SwiftTrimmingSrc::no_rtt_over_target_last_target_rtt() {
    drop_old_received();
    for (const auto &[ts, ecn, size, rtt] : _received_ecn) {
        if (rtt > _target_rtt) {
            return false;
        }
    }
    return true;
}

std::size_t SwiftTrimmingSrc::getEcnInTargetRtt() {
    drop_old_received();
    std::size_t ecn_count = 0;
    for (const auto &[ts, ecn, size, rtt] : _received_ecn) {
        if (ecn) {
            ++ecn_count;
        }
    }
    return ecn_count;
}

bool SwiftTrimmingSrc::ecn_congestion() {
    if (getEcnInTargetRtt() >= _received_ecn.size() / 2) {
        return true;
    }
    return false;
}

const string &SwiftTrimmingSrc::nodename() { return _nodename; }

void SwiftTrimmingSrc::connect(Route *routeout, Route *routeback,
                               SwiftTrimmingSink &sink,
                               simtime_picosec starttime) {
    if (_route_strategy == SINGLE_PATH || _route_strategy == ECMP_FIB ||
        _route_strategy == ECMP_FIB_ECN || _route_strategy == REACTIVE_ECN ||
        _route_strategy == ECMP_RANDOM2_ECN ||
        _route_strategy == ECMP_RANDOM_ECN) {
        assert(routeout);
        _route = routeout;
    }

    _sink = &sink;
    _flow.set_id(get_id()); // identify the packet flow with the NDP source
                            // that generated it
    _flow._name = _name;
    _sink->connect(*this, routeback);

    eventlist().sourceIsPending(*this, starttime);
}

void SwiftTrimmingSrc::startflow() {
    ideal_x = x_gain;
    _flow_start_time = eventlist().now();
    send_packets();
}

const Route *SwiftTrimmingSrc::get_path() {
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
        current_entropy = current_entropy + 1;
        current_entropy = current_entropy % _num_entropies;
    }

    total_routes = _paths.size();
    return _paths.at(_crt_path);
}

void SwiftTrimmingSrc::map_entropies() {
    for (int i = 0; i < _num_entropies; i++) {
        _entropy_array.push_back(random() % _paths.size());
    }
    printf("Printing my Paths: ");
    // //fflush(stdout);
    for (int i = 0; i < _num_entropies; i++) {
        printf("%d - ", _entropy_array[i]);
    }
    printf("\n");
}

void SwiftTrimmingSrc::send_packets() {
    //_list_cwd.push_back(std::make_pair(eventlist().now() / 1000, _cwnd));
    if (_rtx_pending) {
        retransmit_packet();
    }
    // printf("Sent Packet Called, %d\n", from);
    _list_unacked.push_back(std::make_pair(eventlist().now() / 1000, _unacked));
    unsigned c = _cwnd;

    while ( //_last_acked + c >= _highest_sent + _mss &&
            get_unacked() + _mss <= c && _highest_sent < _flow_size) {
        uint64_t data_seq = 0;

        // create packet
        // printf("Dest 1 is %d\n", _dstaddr);
        // //fflush(stdout);
        SwiftTrimmingPacket *p =
                SwiftTrimmingPacket::newpkt(_flow, *_route, _highest_sent + 1,
                                            data_seq, _mss, false, _dstaddr);

        // p->set_route(*_route);
        // int path_chosen = choose_route();
        // printf("Path Chosen %d - Size %d\n", path_chosen,
        // _path_ids.size()); p->set_pathid(_path_ids[path_chosen]);

        p->set_route(*_route);
        int crt = choose_route();
        // crt = random() % _paths.size();

        p->set_pathid(_path_ids[crt]);

        /*printf("From %d - CRT %d - PathID %d - Size %d\n", from, crt,
               _path_ids[crt], _path_ids.size());
        //fflush(stdout);*/

        p->from = this->from;
        p->to = this->to;
        p->tag = this->tag;
        p->my_idx = data_count_idx++;

        p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);
        p->set_ts(eventlist().now());

        if (COLLECT_DATA) {
            // Sent
            std::string file_name =
                    PROJECT_ROOT_PATH / ("sim/output/sent/s" +
                                         std::to_string(this->from) + ".txt ");
            std::ofstream MyFile(file_name, std::ios_base::app);

            MyFile << (GLOBAL_TIME) / 1000 << "," << 1 << std::endl;

            MyFile.close();
        }

        // send packet
        _highest_sent += _mss;
        _packets_sent += _mss;
        _unacked += _mss;

        // Getting time until packet is really sent
        // printf("Sent Packet, %d\n", from);
        PacketSink *sink = p->sendOn();
        HostQueue *q = dynamic_cast<HostQueue *>(sink);
        assert(q);
        uint32_t service_time = q->serviceTime(*p);
        _sent_packets.push_back(
                SentPacketSwift(eventlist().now() + service_time + _rto,
                                p->seqno(), false, false, false));

        if (_rtx_timeout == timeInf) {
            update_rtx_time();
        }
    }
}

void permute_sequence_swift(vector<int> &seq) {
    size_t len = seq.size();
    for (uint32_t i = 0; i < len; i++) {
        seq[i] = i;
    }
    for (uint32_t i = 0; i < len; i++) {
        int ix = random() % (len - i);
        int tmpval = seq[ix];
        seq[ix] = seq[len - 1 - i];
        seq[len - 1 - i] = tmpval;
    }
}

void SwiftTrimmingSrc::set_paths(uint32_t no_of_paths) {
    if (_route_strategy != ECMP_FIB && _route_strategy != ECMP_FIB_ECN &&
        _route_strategy != ECMP_FIB2_ECN && _route_strategy != REACTIVE_ECN &&
        _route_strategy != ECMP_RANDOM_ECN &&
        _route_strategy != ECMP_RANDOM2_ECN) {
        cout << "Set paths (path_count) called with wrong route strategy"
             << endl;
        abort();
    }

    _path_ids.resize(no_of_paths);
    permute_sequence_swift(_path_ids);

    _paths.resize(no_of_paths);
    _original_paths.resize(no_of_paths);
    _path_acks.resize(no_of_paths);
    _path_ecns.resize(no_of_paths);
    _path_nacks.resize(no_of_paths);
    _bad_path.resize(no_of_paths);
    _avoid_ratio.resize(no_of_paths);
    _avoid_score.resize(no_of_paths);

    _path_ids.resize(no_of_paths);
    // permute_sequence(_path_ids);
    _paths.resize(no_of_paths);
    _path_ecns.resize(no_of_paths);

    for (size_t i = 0; i < no_of_paths; i++) {
        _paths[i] = NULL;
        _original_paths[i] = NULL;
        _path_acks[i] = 0;
        _path_ecns[i] = 0;
        _path_nacks[i] = 0;
        _avoid_ratio[i] = 0;
        _avoid_score[i] = 0;
        _bad_path[i] = false;
        _path_ids[i] = i;
    }
}

void SwiftTrimmingSrc::set_paths(vector<const Route *> *rt_list) {
    uint32_t no_of_paths = rt_list->size();
    switch (_route_strategy) {
    case NOT_SET:
    case SINGLE_PATH:
    case ECMP_FIB:
    case ECMP_FIB_ECN:
    case REACTIVE_ECN:
        // shouldn't call this with these strategies
        abort();
    case SCATTER_PERMUTE:
    case SCATTER_RANDOM:
    case PULL_BASED:
    case SCATTER_ECMP: {
        no_of_paths = min(_num_entropies, (int)no_of_paths);
        _path_ids.resize(no_of_paths);
        _paths.resize(no_of_paths);
        _original_paths.resize(no_of_paths);
        _path_acks.resize(no_of_paths);
        _path_ecns.resize(no_of_paths);
        _path_nacks.resize(no_of_paths);
        _bad_path.resize(no_of_paths);
        _avoid_ratio.resize(no_of_paths);
        _avoid_score.resize(no_of_paths);
#ifdef DEBUG_PATH_STATS
        _path_counts_new.resize(no_of_paths);
        _path_counts_rtx.resize(no_of_paths);
        _path_counts_rto.resize(no_of_paths);
#endif

        // generate a randomize sequence of 0 .. size of rt_list - 1
        vector<int> randseq(rt_list->size());
        if (_route_strategy == SCATTER_ECMP) {
            // randsec may have duplicates, as with ECMP
            // randomize_sequence(randseq);
        } else {
            // randsec will have no duplicates
            // permute_sequence(randseq);
        }

        for (size_t i = 0; i < no_of_paths; i++) {
            // we need to copy the route before adding endpoints, as
            // it may be used in the reverse direction too.
            // Pick a random route from the available ones
            Route *tmp = new Route(*(rt_list->at(randseq[i])), *_sink);
            // Route* tmp = new Route(*(rt_list->at(i)));
            tmp->add_endpoints(this, _sink);
            tmp->set_path_id(i, rt_list->size());
            _paths[i] = tmp;
            _path_ids[i] = i;
            _original_paths[i] = tmp;
#ifdef DEBUG_PATH_STATS
            _path_counts_new[i] = 0;
            _path_counts_rtx[i] = 0;
            _path_counts_rto[i] = 0;
#endif
            _path_acks[i] = 0;
            _path_ecns[i] = 0;
            _path_nacks[i] = 0;
            _avoid_ratio[i] = 0;
            _avoid_score[i] = 0;
            _bad_path[i] = false;
        }
        _crt_path = 0;
        // permute_paths();
        break;
    }
    default:
        abort();
        break;
    }
}

void SwiftTrimmingSrc::apply_timeout_penalty() {
    if (_trimming_enabled) {
        reduce_cwnd(_mss);
    } else {
        _cwnd = _mss;
    }
}

void SwiftTrimmingSrc::rtx_timer_hook(simtime_picosec now,
                                      simtime_picosec period) {
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

bool SwiftTrimmingSrc::resend_packet(std::size_t idx) {

    if (get_unacked() >= _cwnd) {
        // printf("This si FALSE\n");
        return false;
    }
    assert(!_sent_packets[idx].acked);

    // this will cause retransmission not only of the offending
    // packet, but others close to timeout
    _rto_margin = _rtt / 2;

    // if (_use_good_entropies && !_good_entropies.empty()) {
    //     rt = _good_entropies[_next_good_entropy];
    //     ++_next_good_entropy;
    //     _next_good_entropy %= _good_entropies.size();
    // } else {
    // }
    // Getting time until packet is really sent
    _unacked += _mss;
    SwiftTrimmingPacket *p = SwiftTrimmingPacket::newpkt(
            _flow, *_route, _sent_packets[idx].seqno, 0, _mss, true, _dstaddr);
    p->set_ts(eventlist().now());

    p->set_route(*_route);
    int crt = choose_route();
    // crt = random() % _paths.size();

    p->set_pathid(_path_ids[crt]);

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
void SwiftTrimmingSrc::retransmit_packet() {
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
 * SwiftTrimmingSink *
 **********/

SwiftTrimmingSink::SwiftTrimmingSink()
        : DataReceiver("sink"), _cumulative_ack{0}, _drops{0} {
    _nodename = "uecsink";
}

void SwiftTrimmingSink::set_end_trigger(Trigger &end_trigger) {
    _end_trigger = &end_trigger;
}

void SwiftTrimmingSink::send_nack(simtime_picosec ts, bool marked,
                                  SwiftTrimmingAck::seq_t seqno,
                                  SwiftTrimmingAck::seq_t ackno,
                                  const Route *rt, int path_id) {

    SwiftTrimmingNack *nack = SwiftTrimmingNack::newpkt(
            _src->_flow, *_route, seqno, ackno, 0, _srcaddr);

    // printf("Sending NACK\n");
    nack->set_pathid(_path_ids[_crt_path]);
    _crt_path++;
    if (_crt_path == _paths.size()) {
        _crt_path = 0;
    }

    nack->pathid_echo = path_id;
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

bool SwiftTrimmingSink::already_received(SwiftTrimmingPacket &pkt) {
    SwiftTrimmingPacket::seq_t seqno = pkt.seqno();

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

void SwiftTrimmingSink::receivePacket(Packet &pkt) {
    // printf("Sink Received\n");
    // fflush(stdout);
    switch (pkt.type()) {
    case SWIFTTRIMMINGACK:
    case SWIFTTRIMMINGNACK:
        // bounced, ignore
        pkt.free();
        // printf("Free4\n");
        // //fflush(stdout);
        return;
    case SWIFTTRIMMING:
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
        // //fflush(stdout);
        return;
    }
    SwiftTrimmingPacket *p = dynamic_cast<SwiftTrimmingPacket *>(&pkt);
    SwiftTrimmingPacket::seq_t seqno = p->seqno();
    SwiftTrimmingPacket::seq_t ackno = p->seqno() + p->data_packet_size() - 1;
    simtime_picosec ts = p->ts();

    if (p->type() == SWIFTTRIMMING) {
        /*printf("NORMALACK, %d at %lu - Time %lu - ID %d\n", this->from,
               GLOBAL_TIME, (GLOBAL_TIME - ts) / 1000, p->id());*/
    }

    bool marked = p->flags() & ECN_CE;

    // printf("Packet %d ECN %d\n", from, marked);

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
            int32_t path_id = p->pathid();
            send_ack(ts, marked, 1, _cumulative_ack, _paths.at(crt_path),
                     pkt.get_route(), path_id);
        }
        return;
    }

    // packet was trimmed
    if (pkt.header_only()) {
        send_nack(ts, marked, seqno, ackno, _paths.at(crt_path), pkt.pathid());
        pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_RCVDESTROY);
        p->free();
        // printf("Free6\n");
        // //fflush(stdout);
        // cout << "trimmed packet";
        return;
    }

    int size = p->data_packet_size();
    // pkt._flow().logTraffic(pkt, *this,
    // TrafficLogger::PKT_RCVDESTROY);
    p->free();
    // printf("Free7\n");
    // //fflush(stdout);

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
    int32_t path_id = p->pathid();
    send_ack(ts, marked, seqno, ackno, _paths.at(crt_path), pkt.get_route(),
             path_id);
}

void SwiftTrimmingSink::send_ack(simtime_picosec ts, bool marked,
                                 SwiftTrimmingAck::seq_t seqno,
                                 SwiftTrimmingAck::seq_t ackno, const Route *rt,
                                 const Route *inRoute, int path_id) {

    SwiftTrimmingAck *ack = 0;

    switch (_route_strategy) {
    case ECMP_FIB:
    case ECMP_FIB_ECN:
    case REACTIVE_ECN:
    case ECMP_RANDOM2_ECN:
    case ECMP_RANDOM_ECN:
        ack = SwiftTrimmingAck::newpkt(_src->_flow, *_route, seqno, ackno, 0,
                                       _srcaddr);

        ack->set_pathid(_path_ids[_crt_path]);
        _crt_path++;
        if (_crt_path == _paths.size()) {
            _crt_path = 0;
        }
        ack->inc_id++;
        ack->my_idx = ack_count_idx++;

        // set ECN echo only if that is selected strategy
        if (marked) {
            // printf("ACK - ECN %d - From %d\n", path_id, from);
            ack->set_flags(ECN_ECHO);
        } else {
            // printf("ACK - NO ECN\n");
            ack->set_flags(0);
        }

        /*printf("Sending ACk FlowID %d - SrcAddr %d - Id %d -------- TIme
           now "
               "%lu vs %lu\n",
               _src->_flow.flow_id(), _srcaddr, ack->inc_id, GLOBAL_TIME /
           1000, ts / 1000);*/
        break;
    case NOT_SET:
        abort();
    default:
        break;
    }
    assert(ack);
    ack->pathid_echo = path_id;

    // ack->inRoute = inRoute;
    ack->is_ack = true;
    ack->flow().logTraffic(*ack, *this, TrafficLogger::PKT_CREATE);
    ack->set_ts(ts);

    // printf("Setting TS to %lu at %lu\n", ts / 1000, GLOBAL_TIME / 1000);
    ack->from = this->from;
    ack->to = this->to;
    ack->tag = this->tag;

    ack->sendOn();
}

const string &SwiftTrimmingSink::nodename() { return _nodename; }

uint64_t SwiftTrimmingSink::cumulative_ack() { return _cumulative_ack; }

uint32_t SwiftTrimmingSink::drops() { return _drops; }

void SwiftTrimmingSink::connect(SwiftTrimmingSrc &src, const Route *route) {
    _src = &src;
    switch (_route_strategy) {
    case SINGLE_PATH:
    case ECMP_FIB:
    case ECMP_FIB_ECN:
    case REACTIVE_ECN:
    case ECMP_RANDOM2_ECN:
    case ECMP_RANDOM_ECN:
        assert(route);
        //("Setting route\n");
        _route = route;
        break;
    default:
        // do nothing we shouldn't be using this route - call
        // set_paths() to set routing information
        _route = NULL;
        break;
    }

    _cumulative_ack = 0;
    _drops = 0;
}

void SwiftTrimmingSink::set_paths(uint32_t no_of_paths) {
    switch (_route_strategy) {
    case SCATTER_PERMUTE:
    case SCATTER_RANDOM:
    case PULL_BASED:
    case SCATTER_ECMP:
    case SINGLE_PATH:
    case NOT_SET:
        abort();

    case ECMP_FIB:
    case ECMP_FIB_ECN:
    case ECMP_RANDOM_ECN:
    case ECMP_RANDOM2_ECN:
    case REACTIVE_ECN:
        assert(_paths.size() == 0);
        _paths.resize(no_of_paths);
        _path_ids.resize(no_of_paths);
        for (unsigned int i = 0; i < no_of_paths; i++) {
            _paths[i] = NULL;
            _path_ids[i] = i;
        }
        _crt_path = 0;
        // permute_paths();
        break;
    default:
        break;
    }
}

/**********************
 * SwiftTrimmingRtxTimerScanner *
 **********************/

SwiftTrimmingRtxTimerScanner::SwiftTrimmingRtxTimerScanner(
        simtime_picosec scanPeriod, EventList &eventlist)
        : EventSource(eventlist, "RtxScanner"), _scanPeriod{scanPeriod} {
    eventlist.sourceIsPendingRel(*this, 0);
}

void SwiftTrimmingRtxTimerScanner::registerSwiftTrimming(
        SwiftTrimmingSrc &uecsrc) {
    _uecs.push_back(&uecsrc);
}

void SwiftTrimmingRtxTimerScanner::doNextEvent() {
    simtime_picosec now = eventlist().now();
    swifts_t::iterator i;
    for (i = _uecs.begin(); i != _uecs.end(); i++) {
        (*i)->rtx_timer_hook(now, _scanPeriod);
    }
    eventlist().sourceIsPendingRel(*this, _scanPeriod);
}
