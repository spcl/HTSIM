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

void UecSrc::do_fast_drop(bool is_trimmed) {

    if (eventlist().now() >= next_window_end) {

        previous_window_end = next_window_end;
        saved_acked_bytes = acked_bytes;
        acked_bytes = 0;
        next_window_end = eventlist().now() + (_target_rtt * 1);

        if ((is_trimmed || need_fast_drop) && previous_window_end != 0) {

            // Assign New Window
            double bonus_based_on_target = 1;
            _cwnd = max((double)(saved_acked_bytes * bonus_based_on_target *
                                 bonus_drop),
                        bonus_based_on_target * bonus_drop * _mss);

            // Ignore for and reset counters
            ignore_for = (get_unacked() / (double)_mss) * 1.25;
            count_received = 0;
            need_fast_drop = false;
        }
    }
}

void UecSrc::processNack(UecNack &pkt) {

    consecutive_good_medium = 0;
    acked_bytes += 64;

    if (count_received >= ignore_for) {
        need_fast_drop = true;
    }

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

    // retransmit only the NACK'ed packet
    bool success = resend_packet(i);
    if (!_rtx_pending && !success) {
        _rtx_pending = true;
    }
}

void UecSrc::processBts(UecPacket *pkt) { return; }

void UecSrc::processAck(UecAck &pkt, bool force_marked) {
    UecAck::seq_t seqno = pkt.ackno();
    simtime_picosec ts = pkt.ts();

    consecutive_nack = 0;
    bool marked = pkt.flags() &
                  ECN_ECHO; // ECN was marked on data packet and echoed on ACK

    uint64_t newRtt = eventlist().now() - ts;
    avg_rtt = avg_rtt * (1 - 0.15) + 0.15 * newRtt;
    _received_ecn.push_back(std::make_tuple(
            eventlist().now(), marked, _mss,
            newRtt)); // TODO: assuming same size for all packets
    mark_received(pkt);

    count_total_ack++;
    if (marked) {
        count_total_ecn++;
        consecutive_good_medium = 0;
    }

    if (!marked) {
        add_ack_path(pkt.inRoute);
        _consecutive_no_ecn += _mss;
    } else {
        ecn_last_rtt = true;
        _consecutive_no_ecn = 0;
    }
    if (newRtt < _target_rtt) {
        ++_consecutive_low_rtt;
    } else {
        _consecutive_low_rtt = 0;
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

        printf("Completion Time Flow is %lu\n",
               eventlist().now() - _flow_start_time);
    }

    if (seqno > _last_acked || true) { // TODO: new ack, we don't care about
                                       // ordering for now. Check later though
        if (seqno >= _highest_sent) {
            _highest_sent = seqno;
        }

        _last_acked = seqno;

        adjust_window(ts, marked, newRtt);
        acked_bytes += _mss;
        good_bytes += _mss;

        _effcwnd = _cwnd;
        send_packets();
        return;
    }
}

uint64_t UecSrc::get_unacked() {
    uint64_t missing = 0;
    for (const auto &sp : _sent_packets) {
        if (!sp.acked && !sp.nacked && !sp.timedOut) {
            missing += _mss;
        }
    }
    return missing;
}

void UecSrc::receivePacket(Packet &pkt) {

    if (pkt._queue_full || pkt.bounced() == false) {
        reduce_unacked(_mss);
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
        count_received++;
        processAck(dynamic_cast<UecAck &>(pkt), false);
        pkt.free();
        break;
    case UECNACK:
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
}

void UecSrc::fast_increase() {
    if (use_fast_drop) {
        if (count_received > ignore_for) {
            if (use_super_fast_increase) {
                _cwnd += 2 * _mss *
                         (LINK_SPEED_MODERN /
                          100); // LINK_SPEED_MODERN is linkspeed in Gbps
            } else {
                _cwnd += _mss;
            }
        }
    } else {
        if (use_super_fast_increase) {
            _cwnd += 2 * _mss *
                     (LINK_SPEED_MODERN /
                      100); // LINK_SPEED_MODERN is linkspeed in Gbps
        } else {
            _cwnd += _mss;
        }
    }

    increasing = true;
}

void UecSrc::adjust_window(simtime_picosec ts, bool ecn, simtime_picosec rtt) {

    // We set Buffer size equal to BDP and Kmin to 20% of that. Kmax 80% of BDP.
    // IMPORTANT PARAMETERS, these are true for 100Gbps and 2048B MTU. Check
    // comment below for 800Gbps
    x_gain = 0.25;
    y_gain = 1.25;
    w_gain = 2;
    z_gain = 0.8;
    use_fast_drop = true;
    use_fast_increase = true;
    use_super_fast_increase = true;
    // target rtt is 1.5x base_rtt

    // For 800 Gbps and 2048B MTU
    /*
    x_gain = 1.6;
    y_gain = 8;
    w_gain = 1.25;
    z_gain = 1;
    use_fast_drop = true;
    use_fast_increase = true;
    use_super_fast_increase = true;
    // target rtt is 1.5x base_rtt
    */

    if (rtt <= (_base_rtt + (_mss * 8 / LINK_SPEED_MODERN * 3 * 1000)) &&
        !ecn) {
        counter_consecutive_good_bytes += _mss;
    } else {
        target_window = _cwnd;
        counter_consecutive_good_bytes = 0;
        increasing = false;
    }

    // Check QuickAdapt
    if (use_fast_drop) {
        if (count_received >= ignore_for) {
            do_fast_drop(false);
        }
    }

    // Check Fast Increase
    if ((increasing || counter_consecutive_good_bytes > target_window) &&
        use_fast_increase) {
        fast_increase();
        // Case 1 RTT Based Increase
    } else if (!ecn && rtt < _target_rtt) {
        _cwnd += min(uint32_t((((_target_rtt - rtt) / (double)rtt) * y_gain *
                               _mss * (_mss / (double)_cwnd))),
                     uint32_t(_mss));

        _cwnd += ((double)_mss / _cwnd) * ideal_x * _mss;
        // Case 2 Hybrid Based Decrease || RTT Decrease
    } else if (ecn && rtt > _target_rtt) {
        if (count_received >= ignore_for) {

            last_decrease = min((w_gain * ((rtt - (double)_target_rtt) / rtt) *
                                 _mss) + _cwnd / (double)_bdp * z_gain * _mss,
                                (double)_mss);
            _cwnd -= last_decrease;
        }
        // Case 3 Gentle Decrease (Window based)
    } else if (ecn && rtt < _target_rtt) {
        if (count_received >= ignore_for) {
            reduce_cwnd(static_cast<double>(_cwnd) / _bdp * _mss * z_gain);
        }
        // Case 4
    } else if (!ecn && rtt > _target_rtt) {
        // Do nothing but fairness
        if (count_received >= ignore_for) {
            _cwnd += ((double)_mss / _cwnd) * ideal_x * _mss;
        }
    }

    // Check Limits
    if (_cwnd > _maxcwnd) {
        _cwnd = _maxcwnd;
    }
    if (_cwnd <= _mss) {
        _cwnd = _mss;
    }
}

const string &UecSrc::nodename() { return _nodename; }

void UecSrc::connect(const Route &routeout, const Route &routeback,
                     UecSink &sink, simtime_picosec startTime) {
    _route = &routeout;

    assert(_route);
    _sink = &sink;
    _sink->connect(*this, routeback);

    eventlist().sourceIsPending(*this, startTime);
}

void UecSrc::startflow() {
    ideal_x = x_gain;
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
}

void UecSrc::send_packets() {
    if (_rtx_pending) {
        retransmit_packet();
    }

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
