// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "eqds.h"
#include <math.h>
#include "circular_buffer.h"
#include "eqds_logger.h"

using namespace std;

// Static stuff

// _path_entropy_size is the number of paths we spray across.  If you don't set it, it will default
// to all paths.
uint32_t EqdsSrc::_path_entropy_size = 256;
int EqdsSrc::_global_node_count = 0;

/* _min_rto can be tuned using setMinRTO. Don't change it here.  */
simtime_picosec EqdsSrc::_min_rto = timeFromUs((uint32_t)DEFAULT_EQDS_RTO_MIN);

mem_b EqdsSink::_bytes_unacked_threshold = 16384;
int EqdsSink::TGT_EV_SIZE = 7;

/* if you change _credit_per_pull, fix pktTime in the Pacer too - this assumes one pull per MTU */
EqdsBasePacket::pull_quanta EqdsSink::_credit_per_pull = 8;  // uints of typically 512 bytes

/* this default will be overridden from packet size*/
uint16_t EqdsSrc::_hdr_size = 64;
uint16_t EqdsSrc::_mss = 4096;
uint16_t EqdsSrc::_mtu = _mss + _hdr_size;

bool EqdsSrc::useReps = false;
bool EqdsSrc::use_exp_avg_ecn = false;
bool EqdsSrc::use_exp_avg_rtt = false;

bool EqdsSrc::_debug = false;

bool EqdsSrc::_sender_based_cc = false;
EqdsSrc::Sender_CC EqdsSrc::_sender_cc_algo = EqdsSrc::SMARTT;

bool EqdsSrc::_per_rtt_mode = false;

bool EqdsPullPacer::_oversubscribed_cc = false;

// scaling_factor_a = 1
// scaling_factor_b = 1

double EqdsSrc::_alpha = 0.33;
double EqdsSrc::_beta = .25;
double EqdsSrc::_gamma = 0.15;
double EqdsSrc::_gamma_g = 0.8;
double EqdsSrc::_md = 1;
double EqdsSrc::_mi = 2;
simtime_picosec EqdsSrc::_target_Qdelay = timeFromUs(7u);

#define INIT_PULL 10000000  // needs to be large enough we don't map
                            // negative pull targets (where
                            // credit_spec > backlog) to less than
                            // zero and suffer underflow.  Real
                            // implementations will properly handle
                            // modular wrapping.

//#define CORRECT_PULL_OFFSET

/*
scaling_factor_a = current_BDP/100Gbps*net_base_rtt //12us scaling_factor_b = 12/target_Qdelay
beta = 5*scaling_factor_a
gamma = 0.15* scaling_factor_a
alpha = 4.0* scaling_factor_a*scaling_factor_b/base_rtt gamma_g = 0.8
*/

////////////////////////////////////////////////////////////////
//  EQDS NIC
////////////////////////////////////////////////////////////////

EqdsNIC::EqdsNIC(id_t src_num, EventList& eventList, linkspeed_bps linkspeed, uint32_t ports)
    : EventSource(eventList, "eqdsNIC"), NIC(src_num)  {
    _nodename = "eqdsNIC" + to_string(_src_num);
    _linkspeed = linkspeed;
    _no_of_ports = ports;
    _ports.resize(_no_of_ports);
    for (uint32_t p = 0; p < _no_of_ports; p++) {
        _ports[p].send_end_time = 0;
        _ports[p].last_pktsize = 0;
        _ports[p].busy = false;
    }
    _busy_ports = 0;
    _rr_port = rand()%_no_of_ports; // start on a random port
    _num_queued_srcs = 0;
    _ratio_data = 1;
    _ratio_control = 10;
    _crt = 0;
}

// srcs call request_sending to see if they can send now.  If the
// answer is no, they'll be called back when it's time to send.
const Route* EqdsNIC::requestSending(EqdsSrc& src) {
    if (EqdsSrc::_debug) {
        cout << src.nodename() << " requestSending at "
             << timeAsUs(EventList::getTheEventList().now()) << endl;
    }
    if (_busy_ports == _no_of_ports) {
        // we're already sending on all ports
        /*
        if (_num_queued_srcs == 0 && _control.empty()) {
            // need to schedule the callback
            eventlist().sourceIsPending(*this, _send_end_time);
        }
        */
        _num_queued_srcs += 1;
        _active_srcs.push_back(&src);
        return NULL;
    }
    assert(/*_num_queued_srcs == 0 &&*/ _control.empty());
    uint32_t portnum = findFreePort();
    return src.getPortRoute(portnum);
}

uint32_t EqdsNIC::findFreePort() {
    assert(_busy_ports < _no_of_ports);
    do {
        _rr_port = (_rr_port + 1) % _no_of_ports;

    } while (_ports[_rr_port].busy);
    return _rr_port;
}

uint32_t EqdsNIC::sendOnFreePortNow(simtime_picosec endtime, const Route* rt) {
    if (rt) {
        assert(_ports[_rr_port].busy == false);
    } else {
        _rr_port = findFreePort();
    }
    _ports[_rr_port].send_end_time = endtime;
    _ports[_rr_port].busy = true;
    _busy_ports++;
    eventlist().sourceIsPending(*this, endtime);
    return _rr_port;
}

// srcs call startSending when they are allowed to actually send
void EqdsNIC::startSending(EqdsSrc& src, mem_b pkt_size, const Route* rt) {
    if (EqdsSrc::_debug) {
        cout << src.nodename() << " startSending at "
             << timeAsUs(EventList::getTheEventList().now()) << endl;
    }

    
    if (_num_queued_srcs > 0) {
        EqdsSrc* queued_src = _active_srcs.front();
        _active_srcs.pop_front();
        _num_queued_srcs--;
        assert(_num_queued_srcs >= 0);
        assert(queued_src == &src);
    }

    simtime_picosec endtime = eventlist().now() + (pkt_size * 8 * timeFromSec(1.0)) / _linkspeed;
    sendOnFreePortNow(endtime, rt);
}

// srcs call cantSend when they previously requested to send, and now its their turn, they can't for
// some reason.
void EqdsNIC::cantSend(EqdsSrc& src) {
    if (EqdsSrc::_debug) {
        cout << src.nodename() << " cantSend at " << timeAsUs(EventList::getTheEventList().now())
             << endl;
    }

    if (_num_queued_srcs == 0 && _control.empty()) {
        // it was an immediate send, so nothing to do if we can't send after all
        return;
    }
    if (_num_queued_srcs > 0) {
        _num_queued_srcs--;

        EqdsSrc* queued_src = _active_srcs.front();
        _active_srcs.pop_front();

        assert(queued_src == &src);
        assert(_busy_ports < _no_of_ports);

        if (_num_queued_srcs > 0) {
            // give the next src a chance.
            queued_src = _active_srcs.front();
            const Route* route = queued_src->getPortRoute(findFreePort());
            queued_src->timeToSend(*route);
            return;
        }
    }
    if (!_control.empty()) {
        // need to send a control packet, since we didn't manage to send a data packet.
        sendControlPktNow();
    }
}

void EqdsNIC::sendControlPacket(EqdsBasePacket* pkt, EqdsSrc* src, EqdsSink* sink) {
    assert((src || sink) && !(src && sink));
    
    _control_size += pkt->size();
    CtrlPacket cp = {pkt, src, sink};
    _control.push_back(cp);

    if (EqdsSrc::_debug) {
        cout << "NIC " << this << " request to send control packet of type " << pkt->str()
             << " control queue size " << _control_size << " " << _control.size() << endl;
    }

    if (_busy_ports == _no_of_ports) {
        // all ports are busy
        if (EqdsSrc::_debug) {
            cout << "NIC sendControlPacket " << this << " already sending on all ports\n";
        }
    } else {
        // send now!
        sendControlPktNow();
    }
}

// actually do the send of a queued control packet
void EqdsNIC::sendControlPktNow() {
    assert(!_control.empty());
    assert(_busy_ports != _no_of_ports);
    
    CtrlPacket cp = _control.front();
    _control.pop_front();
    EqdsBasePacket* p = cp.pkt;

    simtime_picosec endtime  = eventlist().now() + (p->size() * 8 * timeFromSec(1.0)) / _linkspeed;
    uint32_t port_to_use = sendOnFreePortNow(endtime, NULL);
    if (EqdsSrc::_debug)
        cout << "NIC " << this << " send control of size " << p->size() << " at "
             << timeAsUs(eventlist().now()) << endl;

    _control_size -= p->size();
    assert(p->route() == NULL);
    const Route* route;
    if (cp.src)
        route = cp.src->getPortRoute(port_to_use);
    else
        route = cp.sink->getPortRoute(port_to_use);
    p->set_route(*route);
    p->sendOn();
}


void EqdsNIC::doNextEvent() {
    // doNextEvent should be called every time a packet will have finished being sent
    uint32_t last_port = _no_of_ports;
    for (uint32_t p = 0; p < _no_of_ports; p++) {
        if (_ports[p].busy && _ports[p].send_end_time == eventlist().now()) {
            last_port = p;
            break;
        }
    }
    assert(last_port != _no_of_ports);
    _busy_ports--;
    _ports[last_port].busy = false;

    if (EqdsSrc::_debug)
        cout << "NIC " << this << " doNextEvent at " << timeAsUs(eventlist().now()) << endl;

    if (_num_queued_srcs > 0 && !_control.empty()) {
        _crt++;

        if (_crt >= (_ratio_control + _ratio_data))
            _crt = 0;

        if (EqdsSrc::_debug) {
            cout << "NIC " << this << " round robin time between srcs " << _num_queued_srcs
                 << " and control " << _control.size() << " " << _crt;
        }

        if (_crt < _ratio_data) {
            // it's time for the next source to send
            EqdsSrc* queued_src = _active_srcs.front();
            const Route* route = queued_src->getPortRoute(findFreePort());
            queued_src->timeToSend(*route);

            if (EqdsSrc::_debug)
                cout << " send data " << endl;

            return;
        } else {
            sendControlPktNow();
            return;
        }
    }

    if (_num_queued_srcs > 0) {
        EqdsSrc* queued_src = _active_srcs.front();
        const Route* route = queued_src->getPortRoute(findFreePort());
        queued_src->timeToSend(*route);

        if (EqdsSrc::_debug)
            cout << "NIC " << this << " send data ONLY " << endl;
    } else if (!_control.empty()) {
        sendControlPktNow();
    }
}



////////////////////////////////////////////////////////////////
//  EQDS SRC PORT
////////////////////////////////////////////////////////////////
EqdsSrcPort::EqdsSrcPort(EqdsSrc& src, uint32_t port_num)
    : _src(src), _port_num(port_num) {
}

void EqdsSrcPort::setRoute(const Route& route) {
    _route = &route;
}

void EqdsSrcPort::receivePacket(Packet& pkt) {
    _src.receivePacket(pkt, _port_num);
}

const string& EqdsSrcPort::nodename() {
    return _src.nodename();
}

////////////////////////////////////////////////////////////////
//  EQDS SRC
////////////////////////////////////////////////////////////////

EqdsSrc::EqdsSrc(TrafficLogger* trafficLogger, EventList& eventList, EqdsNIC& nic, uint32_t no_of_ports, bool rts)
    : EventSource(eventList, "eqdsSrc"), _nic(nic), _flow(trafficLogger) {
    _node_num = _global_node_count++;
    _nodename = "eqdsSrc " + to_string(_node_num);

    _no_of_ports = no_of_ports;
    _ports.resize(no_of_ports);
    for (uint32_t p = 0; p < _no_of_ports; p++) {
        _ports[p] = new EqdsSrcPort(*this, p);
    }
        
    _rtx_timeout_pending = false;
    _rtx_timeout = timeInf;
    _rto_timer_handle = eventlist().nullHandle();

    _base_rtt = _min_rto;

    _flow_logger = NULL;

    _rtt = _min_rto;

    _last_dcr_ts = 0;

    _mdev = 0;
    _rto = _min_rto;
    _logger = NULL;

    _maxwnd = 50 * _mtu;
    _cwnd = _maxwnd;
    _flow_size = 0;
    _done_sending = false;
    _backlog = 0;
    _unsent = 0;
    _pull_target = INIT_PULL;
    _pull = INIT_PULL;
    _state = INITIALIZE_CREDIT;
    _speculating = false;
    _credit_pull = 0;
    _credit_spec = _maxwnd;
    _in_flight = 0;
    _highest_sent = 0;
    _send_blocked_on_nic = false;
    _no_of_paths = _path_entropy_size;
    _path_random = rand() % 0xffff;  // random upper bits of EV
    _path_xor = rand() % _no_of_paths;
    _current_ev_index = 0;
    _max_penalty = 1;
    _last_rts = 0;

    // stats for debugging
    _new_packets_sent = 0;
    _rtx_packets_sent = 0;
    _rts_packets_sent = 0;
    _bounces_received = 0;

    // reset path penalties
    _ev_skip_bitmap.resize(_no_of_paths);
    for (uint32_t i = 0; i < _no_of_paths; i++) {
        _ev_skip_bitmap[i] = 0;
    }

    // by default, end silently
    _end_trigger = 0;

    _dstaddr = UINT32_MAX;
    //_route = NULL;
    _mtu = Packet::data_packet_size();
    _mss = _mtu - _hdr_size;

    _debug_src = EqdsSrc::_debug;

    _fi_count = 0;

    if (_sender_based_cc) {
        switch (_sender_cc_algo) {
            case DCTCP:
                updateCwndOnAck = &EqdsSrc::updateCwndOnAck_DCTCP;
                updateCwndOnNack = &EqdsSrc::updateCwndOnNack_DCTCP;
                break;
            case SMARTT:
                updateCwndOnAck = &EqdsSrc::updateCwndOnAck_SmaRTT;
                updateCwndOnNack = &EqdsSrc::updateCwndOnNack_SmaRTT;
                break;
            default:
                cout << "Unknown CC algo specified " << _sender_cc_algo << endl;
                assert(0);
        }
    }
    //if (_node_num == 639) _debug_src = true; // use this to enable debugging on one flow at a
    // time
}

void EqdsSrc::connectPort(uint32_t port_num,
                          Route& routeout,
                          Route& routeback,
                          EqdsSink& sink,
                          simtime_picosec start_time) {
    _ports[port_num]->setRoute(routeout);
    //_route = &routeout;

    if (port_num == 0) {
        _sink = &sink;
        //_flow.set_id(get_id());  // identify the packet flow with the EQDS source that generated it
        _flow._name = _name;

        if (start_time != TRIGGER_START) {
            eventlist().sourceIsPending(*this, timeFromUs((uint32_t)start_time));
        }
    }
    assert(_sink == &sink);
    _sink->connectPort(port_num, *this, routeback);
}

void EqdsSrc::computeRTT(simtime_picosec send_time) {
    // this code is never called, as per spec (fixed RTO setting).
    // keeping it here just in case we want to experiment at some point with dynamic RTO
    // calculation.

    if (eventlist().now() <= send_time)
        return;
    _raw_rtt = eventlist().now() - send_time;

    // assert(_raw_rtt > 0);

    if (_raw_rtt < _base_rtt)
        _base_rtt = _raw_rtt;

    if (_rtt > 0) {
        simtime_picosec abs;
        if (_raw_rtt > _rtt)
            abs = _raw_rtt - _rtt;
        else
            abs = _rtt - _raw_rtt;

        _mdev = 3 * _mdev / 4 + abs / 4;
        _rtt = 7 * _rtt / 8 + _raw_rtt / 8;
    } else {
        _rtt = _raw_rtt;
        _mdev = _raw_rtt / 2;
    }

    if (_debug_src) {
        cout << "RTT for flow " << _flow.str() << " computed at " << timeAsUs(eventlist().now())
             << " is " << timeAsUs(_rtt) << "us, raw value was " << timeAsUs(_raw_rtt) << endl;
    }
}

simtime_picosec EqdsSrc::computeDynamicRTO(simtime_picosec send_time) {
    // this code is never called, as per spec (fixed RTO setting).
    // keeping it here just in case we want to experiment at some point with dynamic RTO
    // calculation.

    simtime_picosec raw_rtt = eventlist().now() - send_time;

    assert(raw_rtt > 0);
    if (_rtt > 0) {
        simtime_picosec abs;
        if (raw_rtt > _rtt)
            abs = raw_rtt - _rtt;
        else
            abs = _rtt - raw_rtt;

        _mdev = 3 * _mdev / 4 + abs / 4;
        _rtt = 7 * _rtt / 8 + raw_rtt / 8;

        _rto = _rtt + 4 * _mdev;
    } else {
        _rtt = raw_rtt;
        _mdev = raw_rtt / 2;

        _rto = _rtt + 4 * _mdev;
    }
    if (_rto < _min_rto)
        _rto = _min_rto * ((drand() * 0.5) + 0.75);

    if (_rto < _min_rto)
        _rto = _min_rto;

    if (_debug_src) {
        cout << "RTO for flow " << _flow.str() << " computed at " << timeAsUs(_rto)
             << " will be lower bounded to " << timeAsUs(_min_rto) << endl;
    }

    return _rto;
}

void EqdsSrc::receivePacket(Packet& pkt, uint32_t portnum) {
    switch (pkt.type()) {
        case EQDSDATA: {
            _bounces_received++;
            // TBD - this is likely a Back-to-sender packet
            abort();
        }
        case EQDSRTS: {
            abort();
        }
        case EQDSACK: {
            processAck((const EqdsAckPacket&)pkt);
            pkt.free();
            return;
        }
        case EQDSNACK: {
            processNack((const EqdsNackPacket&)pkt);
            pkt.free();
            return;
        }
        case EQDSPULL: {
            processPull((const EqdsPullPacket&)pkt);
            pkt.free();
            return;
        }
        default: {
            abort();
        }
    }
}

mem_b EqdsSrc::handleAckno(EqdsDataPacket::seq_t ackno) {
    auto i = _tx_bitmap.find(ackno);
    if (i == _tx_bitmap.end())
        return 0;
    // mem_b pkt_size = i->second.pkt_size;
    simtime_picosec send_time = i->second.send_time;

    // computeRTO(send_time);
    computeRTT(send_time);

    mem_b pkt_size = i->second.pkt_size;
    _in_flight -= pkt_size;
    assert(_in_flight >= 0);
    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " handleAck " << ackno << " flow " << _flow.str() << endl;
    _tx_bitmap.erase(i);
    _send_times.erase(send_time);

    if (send_time == _rto_send_time) {
        recalculateRTO();
    }

    return pkt_size;
}

mem_b EqdsSrc::handleCumulativeAck(EqdsDataPacket::seq_t cum_ack) {
    mem_b newly_acked = 0;

    // free up anything cumulatively acked
    while (!_rtx_queue.empty()) {
        auto seqno = _rtx_queue.begin()->first;

        if (seqno < cum_ack) {
            _rtx_queue.erase(_rtx_queue.begin());
        } else
            break;
    }

    auto i = _tx_bitmap.begin();
    while (i != _tx_bitmap.end()) {
        auto seqno = i->first;
        // cumulative ack is next expected packet, not yet received
        if (seqno >= cum_ack) {
            // nothing else acked
            break;
        }
        mem_b pkt_size = i->second.pkt_size;
        simtime_picosec send_time = i->second.send_time;

        newly_acked += i->second.pkt_size;

        // computeRTO(send_time);
        computeRTT(send_time);

        _in_flight -= pkt_size;
        assert(_in_flight >= 0);
        if (_debug_src)
            cout << _flow.str() << " " << _nodename << " handleCumAck " << seqno << " flow " << _flow.str() << endl;
        _tx_bitmap.erase(i);
        i = _tx_bitmap.begin();
        _send_times.erase(send_time);
        if (send_time == _rto_send_time) {
            recalculateRTO();
        }
    }
    return newly_acked;
}

void EqdsSrc::handlePull(EqdsBasePacket::pull_quanta pullno) {
    if (pullno > _pull) {
        EqdsBasePacket::pull_quanta extra_credit = pullno - _pull;
        _credit_pull += EqdsBasePacket::unquantize(extra_credit);
        if (_credit_pull > _maxwnd)
            _credit_pull = _maxwnd;
        _pull = pullno;
    }
}

bool EqdsSrc::checkFinished(EqdsDataPacket::seq_t cum_ack) {
    // cum_ack gives the next expected packet
    if (_done_sending) {
        // if (EqdsSrc::_debug) cout << _nodename << " checkFinished done sending " << " cum_acc "
        // << cum_ack << " mss " << _mss << " c*m " << cum_ack * _mss << endl;
        return true;
    }
    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " checkFinished "
             << " cum_acc " << cum_ack << " mss " << _mss << " RTS sent " << _rts_packets_sent
             << " total bytes " << ((int64_t)cum_ack - _rts_packets_sent) * _mss << " flow_size "
             << _flow_size << " done_sending " << _done_sending << endl;

    if ((((mem_b)cum_ack - _rts_packets_sent) * _mss) >= _flow_size) {
        cout << "Flow " << _name << " flowId " << flowId() << " " << _nodename << " finished at "
             << timeAsUs(eventlist().now()) << " total packets " << cum_ack << " RTS "
             << _rts_packets_sent << " total bytes " << ((mem_b)cum_ack - _rts_packets_sent) * _mss
             << endl;
        _state = IDLE;
        if (_end_trigger) {
            _end_trigger->activate();
        }
        if (_flow_logger) {
            _flow_logger->logEvent(_flow, *this, FlowEventLogger::FINISH, _flow_size, cum_ack);
        }
        _done_sending = true;
        return true;
    }
    return false;
}

void EqdsSrc::processAck(const EqdsAckPacket& pkt) {
    _nic.logReceivedCtrl(pkt.size());
    
    auto cum_ack = pkt.cumulative_ack();
    mem_b newly_acked = 0;

    newly_acked += handleCumulativeAck(cum_ack);

    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " processAck cum_ack: " << cum_ack << " flow " << _flow.str() << endl;

    auto ackno = pkt.ref_ack();
    uint64_t bitmap = pkt.bitmap();
    if (_debug_src)
        cout << "    ref_ack: " << ackno << " bitmap: " << bitmap << endl;
    while (bitmap > 0) {
        if (bitmap & 1) {
            if (_debug_src)
                cout << "    Sack " << ackno << " flow " << _flow.str() << endl;

            newly_acked += handleAckno(ackno);
        }
        ackno++;
        bitmap >>= 1;
    }

    if (_sender_based_cc)
        (this->*updateCwndOnAck)(pkt.ecn_echo(), _raw_rtt, newly_acked);
    // auto pullno = pkt.pullno();
    // handlePull(pullno);

    // handle ECN echo
    if (pkt.ecn_echo()) {
        penalizePath(pkt.ev(), 1);
    }

    if (checkFinished(cum_ack)) {
        stopSpeculating();
        return;
    }

    stopSpeculating();
    sendIfPermitted();
}

void EqdsSrc::updateCwndOnAck_DCTCP(bool skip, simtime_picosec rtt, mem_b newly_acked_bytes) {
    cout << timeAsUs(eventlist().now()) << " DCTCP start " << _name << " cwnd " << _cwnd
         << " with params skip " << skip << " acked bytes " << newly_acked_bytes << endl;

    if (skip == false)  // additive increase, 1 PKT /RTT
    {
        _cwnd += newly_acked_bytes * _mtu / _cwnd;

    } else {  // multiplicative decrease, done per mark, more aggressive than DCTCP (less smoothing)
              // but much simpler and more responsive since we don't need to track alpha.
        _cwnd -= newly_acked_bytes / 3;
        _cwnd = max((mem_b)_mtu, _cwnd);
    }
}

void EqdsSrc::updateCwndOnNack_DCTCP(bool skip, simtime_picosec rtt, mem_b nacked_bytes) {
    _cwnd -= nacked_bytes;
    _cwnd = max(_cwnd, (mem_b)_mtu);
}

void EqdsSrc::check_limits_cwnd() {
    // Upper Limit                                                                                    
    if (_cwnd > _maxwnd) {
        _cwnd = _maxwnd;
    }
    // Lower Limit                                                                                    
    if (_cwnd < _mss) {
        _cwnd = _mss;
    }
}

void EqdsSrc::quick_adapt(bool trimmed) {
    if (eventlist().now() >= next_window_end) {
        previous_window_end = next_window_end;
	    saved_acked_bytes = acked_bytes;
        acked_bytes = 0;
        next_window_end = eventlist().now() + (_base_rtt + _target_Qdelay);
        // Enable Fast Drop                                                                           
	    if ((trimmed || need_quick_adapt) && previous_window_end != 0) {

            // Edge case when receiving a packet, a long time after                                   
            // the last one (>> base_rtt). Should not matter in most cases.                           
	        saved_acked_bytes = saved_acked_bytes *
                                ((double)(_base_rtt + _target_Qdelay) /
                                 (eventlist().now() - previous_window_end +
                                  (_base_rtt + _target_Qdelay)));

            // Update window and ignore count                                                         
            _cwnd = max((double)(saved_acked_bytes * 1.0),
                        (double)_mss); // 1.0 is the amount of target_rtt over                        
                                       // base_rtt. Simplified here for this                          
                                       // code share, need to make it dynamic                         
                                       // later on                                                    
            check_limits_cwnd();
            ignore_for = (_in_flight / (double)_mss);

            // Reset counters, update logs.                                                           
            count_received = 0;
	        need_quick_adapt = false;

            // Update x_gain after large incasts. We want to limit its effect if                      
            // we move to much smaller windows. Disabled for now, need to                             
            // re-enable in the future. x_gain = min(initial_x_gain,                                  
            //             (_queue_size / 5.0) / (_mss * ((double)_bdp /                              
            //             _cwnd)));                                                                  
        }
    }
}

void EqdsSrc::updateCwndOnAck_SmaRTT(bool skip, simtime_picosec rtt, mem_b newly_acked_bytes) {
    simtime_picosec delay = rtt - _base_rtt;
    simtime_picosec target_rtt = _base_rtt + _target_Qdelay;

    acked_bytes += _mss;

    bool can_decrease_exp_avg = false;
    exp_avg_ecn = exp_avg_alpha * skip + (1 - exp_avg_alpha) * exp_avg_ecn;

    // ECN Check                                                                                      
    if (use_exp_avg_ecn) {
        if (exp_avg_ecn > exp_avg_ecn_value) {
            can_decrease_exp_avg = true;
        }
    } else {
        can_decrease_exp_avg = true;
    }

    if (count_received >= ignore_for) {
        quick_adapt(false);
    }

    if (count_received < ignore_for) {
        return;
    }

   // fast Increase                                                                                  
    if (delay < timeFromUs(1u) && !skip) {
        _fi_count += newly_acked_bytes;

        if (_fi_count > target_window) {
            _cwnd += 4 * _mtu;
            _cwnd = min(_maxwnd, _cwnd);

            return;
        }
    } else {
        target_window = _cwnd;
        _fi_count = 0;
    }

    if (skip == false && delay >= _target_Qdelay) {
        // Fair Increase                                                                              
        _cwnd += _beta * newly_acked_bytes * _mtu / _cwnd;
        _cwnd = min(_maxwnd, _cwnd);
    } else if (skip == false && delay < _target_Qdelay) {
        // Mult Increase                                                                              
        double delta1 = min(newly_acked_bytes,
                            (mem_b)(_mi * _mtu * (target_rtt - (double)rtt) *
                                    newly_acked_bytes / _cwnd / rtt));
        _cwnd += delta1;
        double delta2 = _beta * newly_acked_bytes * _mtu / _cwnd;
        _cwnd += delta2;
        _cwnd = min(_maxwnd, _cwnd);
    } else if (skip == true && delay >= _target_Qdelay) {
        // Mult Decrease                                                                              
        if (can_decrease_exp_avg) {
            _cwnd -= min(newly_acked_bytes,
                         (mem_b)(_md * newly_acked_bytes *
                                 (rtt - (double)target_rtt) /
                                 rtt)); // the decrease amount is                                     
                                        // additionally capped not to                                 
                                        // exceed the pkt_size it                                     
            _cwnd -= _cwnd * _gamma_g * newly_acked_bytes / _maxwnd;
            if (_cwnd < _mtu)
                _cwnd = _mtu;
        }

    } else if (skip == true and delay < _target_Qdelay) {
        // Fair Decrease                                                                              
        if (can_decrease_exp_avg) {
            _cwnd -= _cwnd * _gamma_g * newly_acked_bytes / _maxwnd;
            if (_cwnd < _mtu)
                _cwnd = _mtu;
        }
    }
}

void EqdsSrc::updateCwndOnNack_SmaRTT(bool skip, simtime_picosec rtt, mem_b nacked_bytes) {
     if (count_received >= ignore_for) {
        _cwnd -= nacked_bytes;
        _cwnd = max(_cwnd, (mem_b)_mtu);
        need_quick_adapt = true;
        quick_adapt(true);
    }
}

void EqdsSrc::processNack(const EqdsNackPacket& pkt) {
    _nic.logReceivedCtrl(pkt.size());

    // auto pullno = pkt.pullno();
    // handlePull(pullno);

    auto nacked_seqno = pkt.ref_ack();
    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " processNack nacked: " << nacked_seqno << " flow " << _flow.str()
             << endl;

    uint16_t ev = pkt.ev();
    // what should we do when we get a NACK with ECN_ECHO set?  Presumably ECE is superfluous?
    // bool ecn_echo = pkt.ecn_echo();

    // move the packet to the RTX queue
    auto i = _tx_bitmap.find(nacked_seqno);
    if (i == _tx_bitmap.end()) {
        if (_debug_src)
            cout << _flow.str() << " " << "Didn't find NACKed packet in _active_packets flow " << _flow.str() << endl;

        // this abort is here because this is unlikely to happen in
        // simulation - when it does, it is usually due to a bug
        // elsewhere.  But if you discover a case where this happens
        // for real, remove the abort and uncomment the return below.
        abort();
        // this can happen when the NACK arrives later than a cumulative ACK covering the NACKed
        // packet. return;
    }

    mem_b pkt_size = i->second.pkt_size;

    assert(pkt_size >= _hdr_size);  // check we're not seeing NACKed RTS packets.
    if (pkt_size == _hdr_size) {
        _stats.rts_nacks++;
    }

    auto seqno = i->first;
    simtime_picosec send_time = i->second.send_time;

    computeRTT(send_time);
    // computeDynamicRTO(send_time);

    if (_sender_based_cc)
        (this->*updateCwndOnNack)(ev,_raw_rtt,pkt_size);

    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " erasing send record, seqno: " << seqno << " flow " << _flow.str()
             << endl;
    _tx_bitmap.erase(i);
    assert(_tx_bitmap.find(seqno) == _tx_bitmap.end());  // xxx remove when working

    _in_flight -= pkt_size;
    assert(_in_flight >= 0);

    _send_times.erase(send_time);

    stopSpeculating();
    queueForRtx(seqno, pkt_size);

    if (send_time == _rto_send_time) {
        recalculateRTO();
    }

    penalizePath(ev, 1);
    sendIfPermitted();
}

void EqdsSrc::processPull(const EqdsPullPacket& pkt) {
    _nic.logReceivedCtrl(pkt.size());

    auto pullno = pkt.pullno();
    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " processPull " << pullno << " flow " << _flow.str() << " SP " << pkt.is_slow_pull() << endl;

    handlePull(pullno);

    stopSpeculating();
    sendIfPermitted();
}

void EqdsSrc::doNextEvent() {
    // a timer event fired.  Can either be a timeout, or the timed start of the flow.
    if (_rtx_timeout_pending) {
        clearRTO();
        assert(_logger == 0);

        if (_logger)
            _logger->logEqds(*this, EqdsLogger::EQDS_TIMEOUT);

        rtxTimerExpired();
    } else {
        if (_debug_src)
            cout << _flow.str() << " " << "Starting flow " << _name << endl;
        startFlow();
    }
}

void EqdsSrc::setFlowsize(uint64_t flow_size_in_bytes) {
    _flow_size = flow_size_in_bytes;
}

void EqdsSrc::startFlow() {
    //_cwnd = _maxwnd;
    _credit_spec = _maxwnd;
    if (_debug_src)
        cout << _flow.str() << " " << "startflow " << _flow._name << " CWND " << _cwnd << " at "
             << timeAsUs(eventlist().now()) << " flow " << _flow.str() << endl;

    cout << "Flow " << _name << " flowId " << flowId() << " " << _nodename << " starting at "
         << timeAsUs(eventlist().now()) << endl;

    if (_flow_logger) {
        _flow_logger->logEvent(_flow, *this, FlowEventLogger::START, _flow_size, 0);
    }
    clearRTO();
    _in_flight = 0;
    _pull_target = INIT_PULL;
    _pull = INIT_PULL;
#ifdef CORRECT_PULL_OFFSET     
    _pull_offset = 0;
#endif    
    _unsent = _flow_size;
    _last_rts = 0;
    // backlog is total amount of data we expect to send, including headers
    _backlog = ceil(((double)_flow_size) / _mss) * _hdr_size + _flow_size;
    _state = SPECULATING;
    _speculating = true;
    _send_blocked_on_nic = false;
    while (_send_blocked_on_nic == false && credit() > 0 && _unsent > 0) {
        if (_debug_src)
            cout << _flow.str() << " " << "requestSending 0 "<< endl;

        const Route *route = _nic.requestSending(*this);
        if (route) {
            // if we're here, there's no NIC queue
            mem_b sent_bytes = sendNewPacket(*route);
            if (sent_bytes > 0) {
                _nic.startSending(*this, sent_bytes, route);
            } else {
                _nic.cantSend(*this);
            }
        } else {
            _send_blocked_on_nic = true;
            return;
        }
    }
}

mem_b EqdsSrc::credit() const {
    return _credit_pull + _credit_spec;
}

void EqdsSrc::stopSpeculating() {
    // we just got an ack, nack or pull.  We need to stop speculating

    _speculating = false;

    if (_state == SPECULATING) {
        _state = COMMITTED;
#ifdef CORRECT_PULL_OFFSET
        _credit_spec -= _pull_offset;
        assert(_credit_spec >= 0);
#endif
    }
}

bool EqdsSrc::spendCredit(mem_b pktsize, bool& speculative) {
    assert(credit() > 0);
    if (_credit_pull > 0) {
        assert(_state == COMMITTED);
        _credit_pull -= pktsize;
        speculative = false;
        return true;
    } else if (_speculating && _credit_spec > 0) {
        assert(_state == SPECULATING);
        _credit_spec -= pktsize;
        speculative = true;
        return true;
    } else {
        assert(_state == COMMITTED);
        // we're not going to be sending right now, but we need to
        // reduce speculative credit so that the pull target can
        // advance
        _credit_spec -= pktsize;
        return false;
    }
}

EqdsBasePacket::pull_quanta EqdsSrc::computePullTarget() {
    mem_b pull_target = _backlog;

    if (_sender_based_cc) {
        if (pull_target > _cwnd + _mtu) {
            pull_target = _cwnd + _mtu;
        }
    }

    if (pull_target > _maxwnd) {
        pull_target = _maxwnd;
    }



    pull_target -= (_credit_pull + _credit_spec);
    pull_target += EqdsBasePacket::unquantize(_pull);
#ifdef CORRECT_PULL_OFFSET    
    if (pull_target < INIT_PULL) {
        if (_speculating) {
            _pull_offset = -pull_target;
        } else {
            abort(); // we want to know if this happens so we can come up with a clean solution
        }
        pull_target = INIT_PULL;
    }
#endif

    // Wnsure pull target keeps moving forward when we send - if not, reduce credit_spec and
    // try again This seems to work well for all-to-all and passes outcast_incast, but we should
    // rewrite it to not need to recurse.
    mem_b old_pull_target = EqdsBasePacket::unquantize(_pull_target);
    if (!_speculating && pull_target - old_pull_target < PULL_QUANTUM && _credit_spec > 0 && (true || _backlog > 0 || !_rtx_queue.empty())) {
        _credit_spec -= _mtu;
        pull_target += _mtu;
    }
    EqdsBasePacket::pull_quanta quant_pull_target = EqdsBasePacket::quantize_ceil(pull_target);

    if (_debug_src)
        cout << timeAsUs(eventlist().now()) << _flow.str() << " " << " " << nodename()
             << " pull_target: " << EqdsBasePacket::unquantize(quant_pull_target) << " beforequant "
             << pull_target << " pull " << EqdsBasePacket::unquantize(_pull) << " diff "
             << EqdsBasePacket::unquantize(quant_pull_target - _pull) << " credit pull "
             << _credit_pull << " spec " << _credit_spec << " speculative " << _speculating
             << " state " << _state << " backlog " << _backlog << " active sources "
             << _nic.activeSources() << endl;
    return quant_pull_target;
}

void EqdsSrc::sendIfPermitted() {
    // send if the NIC, credit and window allow.                                                                                                                  

    if (credit() <= 0) {
        // can send if we have *any* credit, but we don't                                                                                                         
        return;
    }

    //cout << timeAsUs(eventlist().now()) << " " << nodename() << " FOO " << _cwnd << " " << _in_flight << endl;                                                  
    if (_sender_based_cc) {
        if (_cwnd <= _in_flight) {
            return;
        }
    }

    if (_rtx_queue.empty()) {
        if (_backlog == 0) {
            // nothing to retransmit, and no backlog.  Nothing to do here.                                                                                        
            if (_credit_pull > 0) {
                if (_debug_src) {
                    cout << _flow.str() << " " << "we have " << _credit_pull
                         << " bytes of credit, but nothing to use it on"
                         << " flow " << _flow.str() << endl;
                }
            }
            return;
        }

        if (_unsent == 0) {
            return;
        }
    }


    if (_send_blocked_on_nic) {
        // the NIC already knows we want to send                                                                                                                  
        return;
    }

    // we can send if the NIC lets us.                                                                                                                            
    if (_debug_src)
        cout << _flow.str() << " " << "requestSending 1\n";
    const Route* route = _nic.requestSending(*this);
    if (route) {
        mem_b sent_bytes = sendPacket(*route);
        if (sent_bytes > 0) {
            _nic.startSending(*this, sent_bytes, route);
        } else {
            _nic.cantSend(*this);
        }
    } else {
        // we can't send yet, but NIC will call us back when we can                                                                                               
        _send_blocked_on_nic = true;
        return;
    }
}


// if sendPacket got called, we have already asked the NIC for
// permission, and we've already got both credit and cwnd to send, so
// we will likely be sending something (sendNewPacket can return 0 if
// we only had speculative credit we're not allowed to use though)
mem_b EqdsSrc::sendPacket(const Route& route) {
    if (_rtx_queue.empty()) {
        return sendNewPacket(route);
    } else {
        return sendRtxPacket(route);
    }
}

void EqdsSrc::startRTO(simtime_picosec send_time) {
    if (!_rtx_timeout_pending) {
        // timer is not running - start it
        _rtx_timeout_pending = true;
        _rtx_timeout = send_time + _rto;
        _rto_send_time = send_time;

        if (_rtx_timeout < eventlist().now())
            _rtx_timeout = eventlist().now();

        if (_debug)
            cout << "Start timer at " << timeAsUs(eventlist().now()) << " source " << _flow.str()
                 << " expires at " << timeAsUs(_rtx_timeout) << " flow " << _flow.str() << endl;

        _rto_timer_handle = eventlist().sourceIsPendingGetHandle(*this, _rtx_timeout);
        if (_rto_timer_handle == eventlist().nullHandle()) {
            // this happens when _rtx_timeout is past the configured simulation end time.
            _rtx_timeout_pending = false;
            if (_debug)
                cout << "Cancel timer because too late for flow " << _flow.str() << endl;
        }
    } else {
        // timer is already running
        if (send_time + _rto < _rtx_timeout) {
            // RTO needs to expire earlier than it is currently set
            cancelRTO();
            startRTO(send_time);
        }
    }
}

void EqdsSrc::clearRTO() {
    // clear the state
    _rto_timer_handle = eventlist().nullHandle();
    _rtx_timeout_pending = false;

    if (_debug)
        cout << "Clear RTO " << timeAsUs(eventlist().now()) << " source " << _flow.str() << endl;
}

void EqdsSrc::cancelRTO() {
    if (_rtx_timeout_pending) {
        // cancel the timer
        eventlist().cancelPendingSourceByHandle(*this, _rto_timer_handle);
        clearRTO();
    }
}

void EqdsSrc::penalizePath(uint16_t path_id, uint8_t penalty) {
    // _no_of_paths must be a power of 2
    uint16_t mask = _no_of_paths - 1;
    path_id &= mask;  // only take the relevant bits for an index
    _ev_skip_bitmap[path_id] += penalty;
    if (_ev_skip_bitmap[path_id] > _max_penalty) {
        _ev_skip_bitmap[path_id] = _max_penalty;
    }
}

uint16_t EqdsSrc::nextEntropy() {
    if (useReps) {
	    uint64_t allpathssizes = _mss * _no_of_paths;
        if (_mss * _highest_sent < max((uint64_t)_maxwnd, allpathssizes)) {
            _crt_path++;
            if (_crt_path == _no_of_paths) {
                _crt_path = 0;
            }
        } else {
            if (_next_pathid == -1) {
                assert(_no_of_paths > 0);
		        _crt_path = random() % _no_of_paths;
            } else {
                _crt_path = _next_pathid;
            }
        }

        return _crt_path;
    } else {
        // _no_of_paths must be a power of 2
        uint16_t mask = _no_of_paths - 1;
        uint16_t entropy = (_current_ev_index ^ _path_xor) & mask;
        while (_ev_skip_bitmap[entropy] > 0) {
            _ev_skip_bitmap[entropy]--;
            _current_ev_index++;
            if (_current_ev_index == _no_of_paths) {
                _current_ev_index = 0;
                _path_xor = rand() & mask;
            }
            entropy = (_current_ev_index ^ _path_xor) & mask;
        }

        // set things for next time
        _current_ev_index++;
        if (_current_ev_index == _no_of_paths) {
            _current_ev_index = 0;
            _path_xor = rand() & mask;
        }

        entropy |= _path_random ^ (_path_random & mask);  // set upper bits
        return entropy;
    }
}

mem_b EqdsSrc::sendNewPacket(const Route& route) {
    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " sendNewPacket highest_sent " << _highest_sent << " h*m "
             << _highest_sent * _mss << " backlog " << _backlog << " unsent " << _unsent << " flow "
             << _flow.str() << endl;
    assert(_unsent > 0);
    assert(((mem_b)_highest_sent - _rts_packets_sent) * _mss < _flow_size);
    mem_b payload_size = _mss;
    if (_unsent < payload_size) {
        payload_size = _unsent;
    }
    assert(payload_size > 0);
    mem_b full_pkt_size = payload_size + _hdr_size;

    // check we're allowed to send according to state machine
    assert(credit() > 0);
    bool speculative = false;
    bool can_send = spendCredit(full_pkt_size, speculative);
    if (!can_send) {
        // we can't send because we're not in speculative mode and only had speculative credit
        return 0;
    }

    _backlog -= full_pkt_size;
    assert(_backlog >= 0);
    _unsent -= payload_size;
    assert(_backlog >= _unsent);
    _in_flight += full_pkt_size;
    auto ptype = EqdsDataPacket::DATA_PULL;
    if (speculative) {
        ptype = EqdsDataPacket::DATA_SPEC;
    }
    _pull_target = computePullTarget();

    auto* p = EqdsDataPacket::newpkt(_flow, route, _highest_sent, full_pkt_size, ptype,
                                     _pull_target, /*unordered=*/true, _dstaddr);
    uint16_t ev = nextEntropy();
    p->set_pathid(ev);
    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);

    if (_backlog == 0 || _credit_pull < 0)
        p->set_ar(true);

    createSendRecord(_highest_sent, full_pkt_size);
    if (_debug_src)
        cout << _flow.str() << " sending pkt " << _highest_sent << " size " << full_pkt_size
             << " pull target " << _pull_target << " ack request " << p->ar() << " at "
             << timeAsUs(eventlist().now()) << " cwnd " << _cwnd << " in_flight " << _in_flight
             << endl;
    p->sendOn();
    _highest_sent++;
    _new_packets_sent++;
    startRTO(eventlist().now());
    return full_pkt_size;
}

mem_b EqdsSrc::sendRtxPacket(const Route& route) {
    assert(!_rtx_queue.empty());
    auto seq_no = _rtx_queue.begin()->first;
    mem_b full_pkt_size = _rtx_queue.begin()->second;
    bool speculative = false;

    bool can_send = spendCredit(full_pkt_size, speculative); // updates speculative
    assert(!speculative);  // I don't think this can happen, but remove this assert if we decide it can

    if (!can_send) {
        // we can't sent because we've only got speculative credit and we're not in speculating mode
        if (_debug_src)
            cout << _flow.str() << " " << _nodename << " want to RTX, can't send, no credit\n";
        return 0;
    }

    _rtx_queue.erase(_rtx_queue.begin());
    _in_flight += full_pkt_size;
    _pull_target = computePullTarget();
    
    auto* p = EqdsDataPacket::newpkt(_flow, route, seq_no, full_pkt_size, EqdsDataPacket::DATA_RTX,
                                     _pull_target, /*unordered=*/true, _dstaddr);
    uint16_t ev = nextEntropy();
    p->set_pathid(ev);
    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);

    createSendRecord(seq_no, full_pkt_size);

    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " sending rtx pkt " << seq_no << " size " << full_pkt_size
             << " at " << timeAsUs(eventlist().now()) << " cwnd " << _cwnd
             << " in_flight " << _in_flight << " pull_target " << _pull_target << " pull " << _pull << endl;
    p->set_ar(true);
    p->sendOn();
    _rtx_packets_sent++;
    startRTO(eventlist().now());
    return full_pkt_size;
}

void EqdsSrc::sendRTS(bool timeout) {
    if (_last_rts > 0 && eventlist().now() - _last_rts < _rtt) {
        // Don't send more than one RTS per RTT, or we can create an
        // incast of RTS.  Once per RTT is enough to restart things if we lost
        // a whole window.
        return;
    }
    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " sendRTS, flow " << _flow.str() << " at "
             << timeAsUs(eventlist().now()) << " epsn " << _highest_sent << " last RTS " << timeAsUs(_last_rts)
             << " in_flight " << _in_flight << " pull_target " << _pull_target << " pull " << _pull << endl;
    createSendRecord(_highest_sent, _hdr_size);
    auto* p =
        EqdsRtsPacket::newpkt(_flow, NULL, _highest_sent, _pull_target, timeout, _dstaddr);
    p->set_dst(_dstaddr);
    uint16_t ev = nextEntropy();
    p->set_pathid(ev);

    // p->sendOn();
    _nic.sendControlPacket(p, this, NULL);

    _highest_sent++;
    _rts_packets_sent++;
    _last_rts = eventlist().now();
    startRTO(eventlist().now());
}

void EqdsSrc::createSendRecord(EqdsBasePacket::seq_t seqno, mem_b full_pkt_size) {
    // assert(full_pkt_size > 64);
    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " createSendRecord seqno: " << seqno << " size " << full_pkt_size
             << endl;
    assert(_tx_bitmap.find(seqno) == _tx_bitmap.end());
    _tx_bitmap.emplace(seqno, sendRecord(full_pkt_size, eventlist().now()));
    _send_times.emplace(eventlist().now(), seqno);
}

void EqdsSrc::queueForRtx(EqdsBasePacket::seq_t seqno, mem_b pkt_size) {
    assert(_rtx_queue.find(seqno) == _rtx_queue.end());
    _rtx_queue.emplace(seqno, pkt_size);
    if (!_speculating) // don't rtx on speculative credit!
        sendIfPermitted();
}

void EqdsSrc::timeToSend(const Route& route) {
    if (_debug_src)
        cout << "timeToSend"
             << " flow " << _flow.str() << " at " << timeAsUs(eventlist().now()) << endl;

    if (_unsent == 0 && _rtx_queue.empty()) {
        _nic.cantSend(*this);
        return;
    }
    // time_to_send is called back from the EqdsNIC when it's time for
    // this src to send.  To get called back, the src must have
    // previously told the NIC it is ready to send by calling
    // EqdsNIC::requestSending()
    //
    // before returning, EqdsSrc needs to call either
    // EqdsNIC::startSending or EqdsNIC::cantSend from this function
    // to update the NIC as to what happened, so they stay in sync.
    _send_blocked_on_nic = false;

    mem_b full_pkt_size;
    // how much do we want to send?
    if (_rtx_queue.empty()) {
        // we want to send new data
        mem_b payload_size = _mss;
        if (_unsent < payload_size) {
            payload_size = _unsent;
        }
        assert(payload_size > 0);
        full_pkt_size = payload_size + _hdr_size;
    } else {
        // we want to retransmit
        full_pkt_size = _rtx_queue.begin()->second;
    }

    if (_sender_based_cc) {
        if (_cwnd < _in_flight) {
            if (_debug_src)
                cout << _flow.str() << " " << _node_num << "cantSend, limited by sender CWND " << _cwnd << " _in_flight "
                     << _in_flight << "\n";

            _nic.cantSend(*this);
            return;
        }
    }

    // do we have enough credit?
    if (credit() <= 0) {
        if (_debug_src)
            cout << "cantSend"
                 << " flow " << _flow.str() << endl;
        ;
        _nic.cantSend(*this);
        return;
    }

    // OK, we're probably good to send
    mem_b bytes_sent = 0;
    if (_rtx_queue.empty()) {
        bytes_sent = sendNewPacket(route);
    } else {
        bytes_sent = sendRtxPacket(route);
    }

    // let the NIC know we sent, so it can calculate next send time.
    if (bytes_sent > 0) {
        _nic.startSending(*this, full_pkt_size, NULL);
    } else {
        _nic.cantSend(*this);
        return;
    }
    if (_sender_based_cc) {
        if (_cwnd < _in_flight) {
            return;
        }
    }
    // do we have enough credit to send again?
    if (credit() <= 0) {
        return;
    }

    if (_unsent == 0 && _rtx_queue.empty()) {
        // we're done - nothing more to send.
        assert(_backlog == 0);
        return;
    }

    // we're ready to send again.  Let the NIC know.
    assert(!_send_blocked_on_nic);
    if (_debug_src)
        cout << "requestSending2"
             << " flow " << _flow.str() << endl;
    ;
    const Route* newroute = _nic.requestSending(*this);
    // we've just sent - NIC will say no, but will call us back when we can send.
    assert(!newroute);
    _send_blocked_on_nic = true;
}

void EqdsSrc::recalculateRTO() {
    // we're no longer waiting for the packet we set the timer for -
    // figure out what the timer should be now.
    cancelRTO();
    if (_send_times.empty()) {
        // nothing left that we're waiting for
        return;
    }
    auto earliest_send_time = _send_times.begin()->first;
    startRTO(earliest_send_time);
}

void EqdsSrc::rtxTimerExpired() {
    assert(eventlist().now() == _rtx_timeout);
    clearRTO();

    auto first_entry = _send_times.begin();
    assert(first_entry != _send_times.end());
    auto seqno = first_entry->second;

    auto send_record = _tx_bitmap.find(seqno);
    assert(send_record != _tx_bitmap.end());
    mem_b pkt_size = send_record->second.pkt_size;

    // update flightsize?

    _send_times.erase(first_entry);
    if (_debug_src)
        cout << _nodename << " rtx timer expired for " << seqno << " flow " << _flow.str() << endl;
    _tx_bitmap.erase(send_record);
    recalculateRTO();

    if (!_rtx_queue.empty()) {
        // there's already a queue, so clearly we shouldn't just
        // resend right now.  But send an RTS (no more than once per
        // RTT) to cover the case where the receiver doesn't know
        // we're waiting.
        stopSpeculating();  

        queueForRtx(seqno, pkt_size);
        sendRTS(true);

        if (_debug_src)
            cout << "sendRTS 1"
                 << " flow " << _flow.str() << endl;
        ;

        return;
    }

    // there's no queue, so maybe we could just resend now?
    queueForRtx(seqno, pkt_size);

    if (_sender_based_cc) {
        if (_cwnd < pkt_size + _in_flight) {
            // window won't allow us to send yet.
            sendRTS(true);
            return;
        }
    }

    if (_credit_pull <= 0) {
        // we don't have any pulled credit to send (we don't want to
        // immediately send on credit_spec, as that's likely when we
        // lost everything!).  Send an RTS (no more than once per RTT)
        // to cover the case where the receiver doesn't know to send
        // us credit
        if (_debug_src)
            cout << "sendRTS 2"
                 << " flow " << _flow.str() << endl;

        sendRTS(true);
        return;
    }

    // we've got enough pulled credit already to send this, so see if the NIC
    // is ready right now
    if (_debug_src)
        cout << "requestSending 4\n"
             << " flow " << _flow.str() << endl;

    const Route* route = _nic.requestSending(*this);
    if (route) {
        bool bytes_sent = sendRtxPacket(*route);
        if (bytes_sent > 0) {
            _nic.startSending(*this, bytes_sent, route);
        } else {
            _nic.cantSend(*this);
            return;
        }
    }
}

void EqdsSrc::activate() {
    startFlow();
}

void EqdsSrc::setEndTrigger(Trigger& end_trigger) {
    _end_trigger = &end_trigger;
};

////////////////////////////////////////////////////////////////
//  EQDS SINK PORT
////////////////////////////////////////////////////////////////
EqdsSinkPort::EqdsSinkPort(EqdsSink& sink, uint32_t port_num)
    : _sink(sink), _port_num(port_num) {
}

void EqdsSinkPort::setRoute(const Route& route) {
    _route = &route;
}

void EqdsSinkPort::receivePacket(Packet& pkt) {
    _sink.receivePacket(pkt, _port_num);
}

const string& EqdsSinkPort::nodename() {
    return _sink.nodename();
}

////////////////////////////////////////////////////////////////
//  EQDS SINK
////////////////////////////////////////////////////////////////

EqdsSink::EqdsSink(TrafficLogger* trafficLogger, EqdsPullPacer* pullPacer, EqdsNIC& nic, uint32_t no_of_ports)
    : DataReceiver("eqdsSink"),
      _nic(nic),
      _flow(trafficLogger),
      _pullPacer(pullPacer),
      _expected_epsn(0),
      _high_epsn(0),
      _retx_backlog(0),
      _latest_pull(INIT_PULL),
      _highest_pull_target(INIT_PULL),
      _received_bytes(0),
      _accepted_bytes(0),
      _end_trigger(NULL),
      _epsn_rx_bitmap(0),
      _out_of_order_count(0),
      _ack_request(false) {
    
    _nodename = "eqdsSink";  // TBD: would be nice at add nodenum to nodename
    _no_of_ports = no_of_ports;
    _ports.resize(no_of_ports);
    for (uint32_t p = 0; p < _no_of_ports; p++) {
        _ports[p] = new EqdsSinkPort(*this, p);
    }
        
    _stats = {0, 0, 0, 0, 0};
    _in_pull = false;
    _in_slow_pull = false;
}

EqdsSink::EqdsSink(TrafficLogger* trafficLogger,
                   linkspeed_bps linkSpeed,
                   double rate_modifier,
                   uint16_t mtu,
                   EventList& eventList,
                   EqdsNIC& nic,
                   uint32_t no_of_ports)
    : DataReceiver("eqdsSink"),
      _nic(nic),
      _flow(trafficLogger),
      _expected_epsn(0),
      _high_epsn(0),
      _retx_backlog(0),
      _latest_pull(INIT_PULL),
      _highest_pull_target(INIT_PULL),
      _received_bytes(0),
      _accepted_bytes(0),
      _end_trigger(NULL),
      _epsn_rx_bitmap(0),
      _out_of_order_count(0),
      _ack_request(false) {
    
    _pullPacer = new EqdsPullPacer(linkSpeed, rate_modifier, mtu, eventList, no_of_ports);
    _no_of_ports = no_of_ports;
    _ports.resize(no_of_ports);
    for (uint32_t p = 0; p < _no_of_ports; p++) {
        _ports[p] = new EqdsSinkPort(*this, p);
    }
    _stats = {0, 0, 0, 0, 0};
    _in_pull = false;
    _in_slow_pull = false;
}

void EqdsSink::connectPort(uint32_t port_num, EqdsSrc& src, const Route& route) {
    _src = &src;
    _ports[port_num]->setRoute(route);
}

void EqdsSink::handlePullTarget(EqdsBasePacket::seq_t pt) {
    if (_src->debug())
        cout << " EqdsSink " << _nodename << " src " << _src->nodename() << " handlePullTarget pt " << pt << " highest_pt " << _highest_pull_target << endl;
    if (pt > _highest_pull_target) {
        if (_src->debug())
            cout << "    pull target advanced\n";
        _highest_pull_target = pt;

        if (_retx_backlog == 0 && !_in_pull) {
            if (_src->debug())
                cout << "    requesting pull\n";
            _in_pull = true;
            _pullPacer->requestPull(this);
        }
    }
}

/*void EqdsSink::handleReceiveBitmap(){

}*/

void EqdsSink::processData(const EqdsDataPacket& pkt) {
    bool force_ack = false;

    if (_src->debug())
        cout << " EqdsSink " << _nodename << " src " << _src->nodename()
             << " processData: " << pkt.epsn() << " time " << timeAsNs(getSrc()->eventlist().now())
             << " when expected epsn is " << _expected_epsn << " ooo count " << _out_of_order_count
             << " flow " << _src->flow()->str() << endl;

    _accepted_bytes += pkt.size();

    handlePullTarget(pkt.pull_target());

    if (pkt.epsn() > _high_epsn) {
        // highest_received is used to bound the sack bitmap. This is a 64 bit number in simulation,
        // never wraps. In practice need to handle sequence number wrapping.
        _high_epsn = pkt.epsn();
    }

    // should send an ACK; if incoming packet is ECN marked, the ACK will be sent straight away;
    // otherwise ack will be delayed until we have cumulated enough bytes / packets.
    bool ecn = (bool)(pkt.flags() & ECN_CE);

    _pullPacer->updateReceiverCc(ecn, false);

    if (pkt.epsn() < _expected_epsn || _epsn_rx_bitmap[pkt.epsn()]) {
        if (EqdsSrc::_debug)
            cout << _nodename << " src " << _src->nodename() << " duplicate psn " << pkt.epsn()
                 << endl;

        _stats.duplicates++;
        _nic.logReceivedData(pkt.size(), 0);

        // sender is confused and sending us duplicates: ACK straight away.
        // this code is different from the proposed hardware implementation, as it keeps track of
        // the ACK state of OOO packets.
        EqdsAckPacket* ack_packet =
            sack(pkt.path_id(), ecn ? pkt.epsn() : sackBitmapBase(pkt.epsn()), ecn);
        // ack_packet->sendOn();
        _nic.sendControlPacket(ack_packet, NULL, this);

        _accepted_bytes = 0;  // careful about this one.
        return;
    }

    if (_received_bytes == 0) {
        force_ack = true;
    }
    // packet is in window, count the bytes we got.
    // should only count for non RTS and non trimmed packets.
    _received_bytes += pkt.size() - EqdsAckPacket::ACKSIZE;
    _nic.logReceivedData(pkt.size(), pkt.size());

    assert(_received_bytes <= _src->flowsize());
    if (_src->debug() && _received_bytes == _src->flowsize())
        cout << _nodename << " received " << _received_bytes << " at "
             << timeAsUs(EventList::getTheEventList().now()) << endl;

    if (pkt.ar()) {
        // this triggers an immediate ack; also triggers another ack later when the ooo queue drains
        // (_ack_request tracks this state)
        force_ack = true;
        _ack_request = true;
    }

    if (_src->debug())
        cout << _nodename << " src " << _src->nodename()
             << " >>    cumulative ack was: " << _expected_epsn << " flow " << _src->flow()->str()
             << endl;

    if (pkt.epsn() == _expected_epsn) {
        while (_epsn_rx_bitmap[++_expected_epsn]) {
            // clean OOO state, this will wrap at some point.
            _epsn_rx_bitmap[_expected_epsn] = 0;
            _out_of_order_count--;
        }
        if (_src->debug())
            cout << " EqdsSink " << _nodename << " src " << _src->nodename()
                 << " >>    cumulative ack now: " << _expected_epsn << " ooo count "
                 << _out_of_order_count << " flow " << _src->flow()->str() << endl;

        if (_out_of_order_count == 0 && _ack_request) {
            force_ack = true;
            _ack_request = false;
        }
    } else {
        _epsn_rx_bitmap[pkt.epsn()] = 1;
        _out_of_order_count++;
        _stats.out_of_order++;
    }

    if (ecn || shouldSack() || force_ack) {
        EqdsAckPacket* ack_packet =
            sack(pkt.path_id(), (ecn || pkt.ar()) ? pkt.epsn() : sackBitmapBase(pkt.epsn()), ecn);

        if (_src->debug())
            cout << " EqdsSink " << _nodename << " src " << _src->nodename()
                 << " send ack now: " << _expected_epsn << " ooo count " << _out_of_order_count
                 << " flow " << _src->flow()->str() << endl;

        _accepted_bytes = 0;

        // ack_packet->sendOn();
        _nic.sendControlPacket(ack_packet, NULL, this);
    }
}

void EqdsSink::processTrimmed(const EqdsDataPacket& pkt) {
    _nic.logReceivedTrim(pkt.size());

    _stats.trimmed++;
    _pullPacer->updateReceiverCc(false, true);

    if (pkt.epsn() < _expected_epsn || _epsn_rx_bitmap[pkt.epsn()]) {
        if (_src->debug())
            cout << " EqdsSink processTrimmed got a packet we already have: " << pkt.epsn()
                 << " time " << timeAsNs(getSrc()->eventlist().now()) << " flow"
                 << _src->flow()->str() << endl;

        EqdsAckPacket* ack_packet = sack(pkt.path_id(), pkt.epsn(), false);
        //ack_packet->sendOn();
        _nic.sendControlPacket(ack_packet, NULL, this);
        return;
    }

    if (_src->debug())
        cout << " EqdsSink processTrimmed packet " << pkt.epsn() << " time "
             << timeAsNs(getSrc()->eventlist().now()) << " flow" << _src->flow()->str() << endl;

    handlePullTarget(pkt.pull_target());

    bool was_retransmitting = _retx_backlog > 0;

    // prioritize credits to this sender! Unclear by how much we should increase here. Assume MTU
    // for now.
    _retx_backlog += EqdsBasePacket::quantize_ceil(EqdsSrc::_mtu);

    if (_src->debug())
        cout << "RTX_backlog++ trim: " << pkt.epsn() << " from " << getSrc()->nodename()
             << " rtx_backlog " << rtx_backlog() << " at " << timeAsUs(getSrc()->eventlist().now())
             << " flow " << _src->flow()->str() << endl;

    EqdsNackPacket* nack_packet = nack(pkt.path_id(), pkt.epsn());

    // nack_packet->sendOn();
    _nic.sendControlPacket(nack_packet, NULL, this);

    if (!was_retransmitting) {
        // source is now retransmitting, must add it to the list.
        if (_src->debug())
            cout << "PullPacer RequestPull: " << _src->flow()->str() << " at "
                 << timeAsUs(getSrc()->eventlist().now()) << endl;

        _pullPacer->requestRetransmit(this);
    }
}

void EqdsSink::processRts(const EqdsRtsPacket& pkt) {
    assert(pkt.ar());
    if (_src->debug())
        cout << " EqdsSink " << _nodename << " src " << _src->nodename()
             << " processRts: " << pkt.epsn() << " time " << timeAsNs(getSrc()->eventlist().now()) << endl;

    handlePullTarget(pkt.pull_target());

    // what happens if this is not an actual retransmit, i.e. the host decides with the ACK that it
    // is
    bool was_retransmitting = _retx_backlog > 0;
    _retx_backlog += pkt.retx_backlog();

    if (_src->debug())
        cout << "RTX_backlog++ RTS: " << _src->flow()->str() << " rtx_backlog " << rtx_backlog()
             << " at " << timeAsUs(getSrc()->eventlist().now()) << endl;

    if (!was_retransmitting) {
        if (!_in_pull) {
            // got an RTS but didn't even know that the source was backlogged. This means we lost
            // all data packets in current window. Must add to standard Pull list, to ensure that
            // after RTX phase passes,  the remaining packets are pulled normally
            _in_pull = true;
            _pullPacer->requestPull(this);
        }

        if (_src->debug())
            cout << "PullPacer RequestRetransmit: " << _src->flow()->str() << " at "
                 << timeAsUs(getSrc()->eventlist().now()) << endl;

        _pullPacer->requestRetransmit(this);
    }

    bool ecn = (bool)(pkt.flags() & ECN_CE);

    if (pkt.epsn() < _expected_epsn || _epsn_rx_bitmap[pkt.epsn()]) {
        if (_src->debug())
            cout << _nodename << " src " << _src->nodename() << " duplicate psn " << pkt.epsn()
                 << endl;

        _stats.duplicates++;

        // sender is confused and sending us duplicates: ACK straight away.
        // this code is different from the proposed hardware implementation, as it keeps track of
        // the ACK state of OOO packets.
        EqdsAckPacket* ack_packet = sack(pkt.path_id(), pkt.epsn(), ecn);
        // ack_packet->sendOn();
        _nic.sendControlPacket(ack_packet, NULL, this);

        _accepted_bytes = 0;  // careful about this one.
        return;
    }

    // packet is in window, count the bytes we got.
    // should only count for non RTS and non trimmed packets.
    _received_bytes += pkt.size() - EqdsAckPacket::ACKSIZE;

    if (pkt.epsn() == _expected_epsn) {
        while (_epsn_rx_bitmap[++_expected_epsn]) {
            // clean OOO state, this will wrap at some point.
            _epsn_rx_bitmap[_expected_epsn] = 0;
            _out_of_order_count--;
        }
        if (_src->debug())
            cout << " EqdsSink " << _nodename << " src " << _src->nodename()
                 << " >>    cumulative ack now: " << _expected_epsn << " ooo count "
                 << _out_of_order_count << " flow " << _src->flow()->str() << endl;

        if (_out_of_order_count == 0 && _ack_request) {
            _ack_request = false;
        }
    } else {
        _epsn_rx_bitmap[pkt.epsn()] = 1;
        _out_of_order_count++;
        _stats.out_of_order++;
    }

    EqdsAckPacket* ack_packet =
        sack(pkt.path_id(), (ecn || pkt.ar()) ? pkt.epsn() : sackBitmapBase(pkt.epsn()), ecn);

    if (_src->debug())
        cout << " EqdsSink " << _nodename << " src " << _src->nodename()
             << " send ack now: " << _expected_epsn << " ooo count " << _out_of_order_count
             << " flow " << _src->flow()->str() << endl;

    _accepted_bytes = 0;

    // ack_packet->sendOn();
    _nic.sendControlPacket(ack_packet, NULL, this);
}

void EqdsSink::receivePacket(Packet& pkt, uint32_t port_num) {
    _stats.received++;
    _stats.bytes_received += pkt.size();  // should this include just the payload?

    switch (pkt.type()) {
        case EQDSDATA:
            if (pkt.header_only())
                processTrimmed((const EqdsDataPacket&)pkt);
            else
                processData((const EqdsDataPacket&)pkt);

            pkt.free();
            break;
        case EQDSRTS:
            processRts((const EqdsRtsPacket&)pkt);
            pkt.free();
            break;
        default:
            abort();
    }
}

uint16_t EqdsSink::nextEntropy() {
    int spraymask = (1 << TGT_EV_SIZE) - 1;
    int fixedmask = ~spraymask;
    int idx = _entropy & spraymask;
    int fixed_entropy = _entropy & fixedmask;
    int ev = idx++ & spraymask;

    _entropy = fixed_entropy | ev;  // save for next pkt

    return ev;
}

EqdsPullPacket* EqdsSink::pull() {
    // called when pull pacer is ready to give another credit to this connection.
    // TODO: need to credit in multiple of MTU here.

    if (_retx_backlog > 0) {
        if (_retx_backlog > EqdsSink::_credit_per_pull)
            _retx_backlog -= EqdsSink::_credit_per_pull;
        else
            _retx_backlog = 0;

        if (EqdsSrc::_debug)
            cout << "RTX_backlog--: " << getSrc()->nodename() << " rtx_backlog " << rtx_backlog()
                 << " at " << timeAsUs(getSrc()->eventlist().now()) << " flow "
                 << _src->flow()->str() << endl;
    }

    _latest_pull += EqdsSink::_credit_per_pull;

    EqdsPullPacket* pkt = NULL;
    pkt = EqdsPullPacket::newpkt(_flow, NULL, _latest_pull, nextEntropy(), _srcaddr);

    return pkt;
}

bool EqdsSink::shouldSack() {
    return _accepted_bytes >= _bytes_unacked_threshold;
}

EqdsBasePacket::seq_t EqdsSink::sackBitmapBase(EqdsBasePacket::seq_t epsn) {
    return max((int64_t)epsn - 63, (int64_t)(_expected_epsn + 1));
}

EqdsBasePacket::seq_t EqdsSink::sackBitmapBaseIdeal() {
    uint8_t lowest_value = UINT8_MAX;
    // initialize to a safe default to avoid maybe-uninitialized warning
    EqdsBasePacket::seq_t lowest_position = _expected_epsn + 1;

    // find the lowest non-zero value in the sack bitmap; that is the candidate for the base, since
    // it is the oldest packet that we are yet to sack. on sack bitmap construction that covers a
    // given seqno, the value is incremented.
    for (EqdsBasePacket::seq_t crt = _expected_epsn; crt <= _high_epsn; crt++)
        if (_epsn_rx_bitmap[crt] && _epsn_rx_bitmap[crt] < lowest_value) {
            lowest_value = _epsn_rx_bitmap[crt];
            lowest_position = crt;
        }

    // clamp window end; avoid underflow if _high_epsn < 64
    if (lowest_position + 64 > _high_epsn) {
        if (_high_epsn > 64)
            lowest_position = _high_epsn - 64;
        else
            lowest_position = _expected_epsn + 1;
    }

    if (lowest_position <= _expected_epsn)
        lowest_position = _expected_epsn + 1;

    return lowest_position;
}

uint64_t EqdsSink::buildSackBitmap(EqdsBasePacket::seq_t ref_epsn) {
    // take the next 64 entries from ref_epsn and create a SACK bitmap with them
    if (_src->debug())
        cout << " EqdsSink: building sack for ref_epsn " << ref_epsn << endl;
    uint64_t bitmap = (uint64_t)(_epsn_rx_bitmap[ref_epsn] != 0) << 63;

    for (int i = 1; i < 64; i++) {
        bitmap = bitmap >> 1 | (uint64_t)(_epsn_rx_bitmap[ref_epsn + i] != 0) << 63;
        if (_src->debug() && (_epsn_rx_bitmap[ref_epsn + i] != 0))
            cout << "     Sack: " << ref_epsn + i << endl;

        if (_epsn_rx_bitmap[ref_epsn + i]) {
            // remember that we sacked this packet
            if (_epsn_rx_bitmap[ref_epsn + i] < UINT8_MAX)
                _epsn_rx_bitmap[ref_epsn + i]++;
        }
    }
    if (_src->debug())
        cout << "       bitmap is: " << bitmap << endl;
    return bitmap;
}

EqdsAckPacket* EqdsSink::sack(uint16_t path_id, EqdsBasePacket::seq_t seqno, bool ce) {
    uint64_t bitmap = buildSackBitmap(seqno);
    EqdsAckPacket* pkt =
        EqdsAckPacket::newpkt(_flow, NULL, _expected_epsn, seqno, path_id, ce, _srcaddr);
    pkt->set_bitmap(bitmap);
    return pkt;
}

EqdsNackPacket* EqdsSink::nack(uint16_t path_id, EqdsBasePacket::seq_t seqno) {
    EqdsNackPacket* pkt = EqdsNackPacket::newpkt(_flow, NULL, seqno, path_id, _srcaddr);
    return pkt;
}

void EqdsSink::setEndTrigger(Trigger& end_trigger) {
    _end_trigger = &end_trigger;
};

static unsigned pktByteTimes(unsigned size) {
    // IPG (96 bit times) + preamble + SFD + ether header + FCS = 38B
    return max(size, 46u) + 38;
}

uint32_t EqdsSink::reorder_buffer_size() {
    uint32_t count = 0;
    // it's not very efficient to count each time, but if we only do
    // this occasionally when the sink logger runs, it should be OK.
    for (uint32_t i = 0; i < eqdsMaxInFlightPkts; i++) {
        if (_epsn_rx_bitmap[i])
            count++;
    }
    return count;
}

////////////////////////////////////////////////////////////////
//  EQDS PACER
////////////////////////////////////////////////////////////////

// pull rate modifier should generally be something like 0.99 so we pull at just less than line rate
EqdsPullPacer::EqdsPullPacer(linkspeed_bps linkSpeed,
                             double pull_rate_modifier,
                             uint16_t mtu,
                             EventList& eventList,
                             uint32_t no_of_ports)
    : EventSource(eventList, "eqdsPull"),
      _pktTime(pull_rate_modifier * 8 * pktByteTimes(mtu) * 1e12 / (linkSpeed * no_of_ports)) {
    _active = false;
}

void EqdsPullPacer::doNextEvent() {
    if (_rtx_senders.empty() && _active_senders.empty() && _idle_senders.empty()) {
        _active = false;
        return;
    }

    if (skipPull()) {
        // if oversubscribed CC says to skip, we'll come back in one pkt time...
        eventlist().sourceIsPendingRel(*this, _pktTime);
        return;
    }

    EqdsSink* sink = NULL;
    EqdsPullPacket* pullPkt;

    if (!_rtx_senders.empty()) {
        sink = _rtx_senders.front();
        _rtx_senders.pop_front();

        pullPkt = sink->pull();
        if (EqdsSrc::_debug)
            cout << "PullPacer: RTX: " << sink->getSrc()->nodename() << " rtx_backlog "
                 << sink->rtx_backlog() << " at " << timeAsUs(eventlist().now()) << endl;
        // TODO if more pulls are needed, enqueue again
        if (sink->rtx_backlog() > 0)
            _rtx_senders.push_back(sink);
        else if (sink->backlog() > 0 && !sink->inPullQueue()) {
            _active_senders.push_back(sink);
            sink->addToPullQueue();
        }
    } else if (!_active_senders.empty()) {
        sink = _active_senders.front();

        assert(sink->inPullQueue());

        _active_senders.pop_front();
        pullPkt = sink->pull();

        // TODO if more pulls are needed, enqueue again
        if (EqdsSrc::_debug)
            cout << "PullPacer: Active: " << sink->getSrc()->nodename() << " backlog "
                 << sink->backlog() << " at " << timeAsUs(eventlist().now()) << endl;
        if (sink->backlog() > 0)
            _active_senders.push_back(sink);
        else {  // this sink has had its demand satisfied, move it to idle senders list.
            _idle_senders.push_back(sink);
            sink->removeFromPullQueue();
            sink->addToSlowPullQueue();
        }
    } else {  // no active senders, we must have at least one idle sender
        sink = _idle_senders.front();
        _idle_senders.pop_front();

        if (!sink->inSlowPullQueue())
            sink->addToSlowPullQueue();

        if (EqdsSrc::_debug)
            cout << "PullPacer: Idle: " << sink->getSrc()->nodename() << " at "
                 << timeAsUs(eventlist().now()) << " backlog " << sink->backlog() << " "
                 << sink->slowCredit() << " max "
                 << EqdsBasePacket::quantize_floor(sink->getMaxCwnd()) << endl;
        pullPkt = sink->pull();
        pullPkt->set_slow_pull(true);

        if (sink->backlog() == 0 &&
            sink->slowCredit() < EqdsBasePacket::quantize_floor(sink->getMaxCwnd())) {
            // only send upto 1BDP worth of speculative credit.
            // backlog will be negative once this source starts receiving speculative credit.
            _idle_senders.push_back(sink);
        } else
            sink->removeFromSlowPullQueue();
    }

    pullPkt->flow().logTraffic(*pullPkt, *this, TrafficLogger::PKT_SEND);

    // pullPkt->sendOn();
    sink->getNIC()->sendControlPacket(pullPkt, NULL, sink);
    _active = true;

    eventlist().sourceIsPendingRel(*this, _pktTime);
}

bool EqdsPullPacer::isActive(EqdsSink* sink) {
    for (auto i = _active_senders.begin(); i != _active_senders.end(); i++) {
        if (*i == sink)
            return true;
    }
    return false;
}

bool EqdsPullPacer::isRetransmitting(EqdsSink* sink) {
    for (auto i = _rtx_senders.begin(); i != _rtx_senders.end(); i++) {
        if (*i == sink)
            return true;
    }
    return false;
}

bool EqdsPullPacer::isIdle(EqdsSink* sink) {
    for (auto i = _idle_senders.begin(); i != _idle_senders.end(); i++) {
        if (*i == sink)
            return true;
    }
    return false;
}

void EqdsPullPacer::requestPull(EqdsSink* sink) {
    if (isActive(sink)) {
        abort();
    }
    assert(sink->inPullQueue());

    _active_senders.push_back(sink);
    // TODO ack timer

    if (!_active) {
        eventlist().sourceIsPendingRel(*this, 0);
        _active = true;
    }
}

void EqdsPullPacer::requestRetransmit(EqdsSink* sink) {
    assert(!isRetransmitting(sink));

    /*
    if (!sink->inPullQueue()){
        sink->addToPullQueue();
        requestPull(sink);
    }
    */

    _rtx_senders.push_back(sink);

    if (!_active) {
        eventlist().sourceIsPendingRel(*this, 0);
        _active = true;
    }
}

#define NOMINAL_RTT timeFromUs(10.0)  // XXXX shouldn't be a constant
void EqdsPullPacer::updateReceiverCc(bool ecn, bool trim) {
    if (!_oversubscribed_cc)
        return;
    // the goal here is to keep track of the total number of packets
    // received and the number of packets received with ECN set in the
    // last RTT on a rolling basis

    // remove any obsolete state
    updateCcState();

    simtime_picosec now = eventlist().now();
    _receipt_records.push_back({.arrival_time = now, .ecn = ecn || trim});
    if (ecn || trim) {
        _receipt_ecn_count++;
    }
}

void EqdsPullPacer::updateCcState() {
    simtime_picosec now = eventlist().now();
    while (_receipt_records.size() > 0 &&
           _receipt_records.front().arrival_time + NOMINAL_RTT < now) {
        // this is old state - discard it
        bool ecn = _receipt_records.front().ecn;
        _receipt_records.pop_front();
        if (ecn) {
            _receipt_ecn_count--;
        }
    }
}

bool EqdsPullPacer::skipPull() {
    if (!_oversubscribed_cc)
        return false;

    // remove any obsolete state
    updateCcState();

    // should we skip sending a pull right now?
    if (_receipt_records.size() == 0 || _receipt_ecn_count == 0)
        return false;

    // we allow 10% ECN before we back off sending pulls
    double ecn_fraction = (double)_receipt_ecn_count / _receipt_records.size();
    if (ecn_fraction < 0.1) {
        return false;
    }
    ecn_fraction -= 0.1;
    if (drand() < ecn_fraction) {
        return true;
    }
    return false;
}
