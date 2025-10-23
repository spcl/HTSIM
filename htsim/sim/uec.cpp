// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "uec.h"
#include <math.h>
#include <cstdint>
#include "circular_buffer.h"
#include "uec_logger.h"
#include "pciemodel.h"

using namespace std;

// Static stuff
flowid_t UecSrc::_debug_flowid = UINT32_MAX;
// _path_entropy_size is the number of paths we spray across.  If you don't set it, it will default
// to all paths.
int UecSrc::_global_node_count = 0;
bool UecSrc::_shown = false;
mem_b UecSrc::_configured_maxwnd = 0;

/* _min_rto can be tuned using setMinRTO. Don't change it here.  */
simtime_picosec UecSrc::_min_rto = timeFromUs((uint32_t)DEFAULT_UEC_RTO_MIN);

mem_b UecSink::_bytes_unacked_threshold = 16384;
int UecSink::TGT_EV_SIZE = 7;

bool UecSink::_model_pcie = false;

/* this default will be overridden from packet size*/
uint16_t UecSrc::_hdr_size = 64;
uint16_t UecSrc::_mss = 4096;
uint16_t UecSrc::_mtu = _mss + _hdr_size;

// send 4 packets of credit per pull, as per default in UEC spec
uint16_t UecSink::_mtus_per_pull = 4;

// units of UEC_PULL_QUANTA bytes (typically 256) - note round down to mss rather than mtu
UecBasePacket::pull_quanta UecSink::_credit_per_pull = (UecSrc::_mss * UecSink::_mtus_per_pull) >> UEC_PULL_SHIFT;

bool UecSrc::_debug = false;

bool UecSrc::_sender_based_cc = false;
bool UecSrc::_receiver_based_cc = false;
bool UecSink::_oversubscribed_cc = false; // can only be enabled when receiver_based_cc is set to true

UecSrc::Sender_CC UecSrc::_sender_cc_algo = UecSrc::NSCC;

/* 
    The following variable values are not default values, there are initializer values. The actual
    default values are set in initNsccParams/initRcccParams.
*/
linkspeed_bps UecSrc::_reference_network_linkspeed = 0; // set by initNsccParams
simtime_picosec UecSrc::_reference_network_rtt = timeFromUs(12u); 
mem_b UecSrc::_reference_network_bdp = 0; // set by initNsccParams
linkspeed_bps UecSrc::_network_linkspeed = 0; // set by initNsccParams
simtime_picosec UecSrc::_network_rtt = 0; // set by initNsccParams
mem_b UecSrc::_network_bdp = 0; // set by initNsccParams
bool UecSrc::_network_trimming_enabled = false; // set by initNsccParams
double UecSrc::_scaling_factor_a = 1; //for 400Gbps. cf. spec must be set to BDP/(100Gbps*12us)
double UecSrc::_scaling_factor_b = 0; // Needs to be inialized in initNscc
uint32_t UecSrc::_qa_scaling = 1; //quick adapt scaling - how much of the achieved bytes should we use as new CWND?
double UecSrc::_gamma = 0.8; //used for aggressive decrease
double UecSrc::_alpha = UecSrc::_scaling_factor_a * 1000 * 4000 / timeFromUs(6u);
double UecSrc::_fi = 1; //fair_increase constant
double UecSrc::_fi_scale = .25 * UecSrc::_scaling_factor_a;
mem_b UecSrc::_min_cwnd = 0;

double UecSrc::_delay_alpha = 0.0125;//0.125;

simtime_picosec UecSrc::_adjust_period_threshold = timeFromUs(12u);
simtime_picosec UecSrc::_target_Qdelay = timeFromUs(6u);
uint32_t UecSrc::_adjust_bytes_threshold = (simtime_picosec)32000*_target_Qdelay/timeFromUs(12u);
double UecSrc::_qa_threshold = 4 * UecSrc::_target_Qdelay; 

double UecSrc::_eta = 0;
bool UecSrc::_disable_quick_adapt = false;
uint8_t UecSrc::_qa_gate = 0;
bool UecSrc::update_base_rtt_on_nack = true;

/* SLEEK parameters */
bool UecSrc::_enable_sleek = false;
int UecSrc::probe_first_trial_time = 3;
int UecSrc::probe_retry_time = 5;
float UecSrc::loss_retx_factor = 1.5;
int UecSrc::min_retx_config = 5;
/* End SLEEK parameters */

void UecSrc::initNsccParams(simtime_picosec network_rtt,
                            linkspeed_bps linkspeed,
                            simtime_picosec target_Qdelay,
                            int8_t qa_gate,
                            bool trimming_enabled){
    _sender_based_cc = true;
                            
    _reference_network_linkspeed = speedFromGbps(100);
    _reference_network_rtt = timeFromUs(12u); 
    _reference_network_bdp = timeAsSec(_reference_network_rtt)*(_reference_network_linkspeed/8);

    _network_linkspeed = linkspeed;
    _network_rtt = network_rtt; 
    _network_bdp = timeAsSec(_network_rtt)*(_network_linkspeed/8);
    _network_trimming_enabled = trimming_enabled;

    _min_cwnd = _mtu;

    if (target_Qdelay > 0) {
        _target_Qdelay = target_Qdelay;
    } else {
        if (_network_trimming_enabled) {
            _target_Qdelay = _network_rtt * 0.75;
        } else {
            _target_Qdelay = _network_rtt;
        }
    }

    if (qa_gate < 0) {
        _qa_gate = 3;
    } else {
        _qa_gate = qa_gate;
    }
    _qa_threshold = 4 * _target_Qdelay; 

    _scaling_factor_a = (double)_network_bdp/(double)_reference_network_bdp;
    _scaling_factor_b = (double)_target_Qdelay/(double)_reference_network_rtt; // no unit

    _alpha = 4.0*_mss*_scaling_factor_a*_scaling_factor_b/_target_Qdelay; // bytes/picosec
    _fi = 5*_mss*_scaling_factor_a;
    _eta = 0.15*_mss*_scaling_factor_a;

    _qa_scaling = 1; //quick adapt scaling - how much of the achieved bytes should we use as new CWND?
    _gamma = 0.8; //used for aggressive decrease
    _fi_scale = .25*_scaling_factor_a;

    _delay_alpha = 0.0125;

    _adjust_period_threshold = _network_rtt;
    _adjust_bytes_threshold = 8 * _mtu;

    cout << "Initializing static NSCC parameters:"
        << " _reference_network_linkspeed=" << _reference_network_linkspeed
        << " _reference_network_rtt=" << _reference_network_rtt
        << " _reference_network_bdp=" << _reference_network_bdp
        << " _target_Qdelay=" << _target_Qdelay
        << " _network_linkspeed=" << _network_linkspeed
        << " _network_rtt=" << _network_rtt
        << " _network_bdp=" << _network_bdp
        << " _qa_gate=2^" << (uint32_t)_qa_gate
        << " _qa_threshold=" << _qa_threshold
        << " _scaling_factor_a=" << _scaling_factor_a
        << " _scaling_factor_b=" << _scaling_factor_b
        << " _alpha=" << _alpha
        << " _fi=" << _fi
        << " _eta=" << _eta
        << " _qa_scaling=" << _qa_scaling
        << " _gamma=" << _gamma
        << " _fi_scale=" << _fi_scale
        << " _delay_alpha=" << _delay_alpha 
        << " _adjust_period_threshold=" << _adjust_period_threshold
        << " _adjust_bytes_threshold=" << _adjust_bytes_threshold
        << endl;
}

void UecSrc::initNscc(mem_b cwnd, simtime_picosec peer_rtt) {
    _base_rtt = peer_rtt;
    _base_bdp = timeAsSec(_base_rtt)*(_nic.linkspeed()/8);
    _bdp = _base_bdp;

    setMaxWnd(1.5*_bdp);
    setConfiguredMaxWnd(1.5*_bdp);

    if (cwnd == 0) {
        _cwnd = _maxwnd;
    } else {
        _cwnd = cwnd;
    }

    /* cout << "Initialize per-instance NSCC parameters:"
        << " flowid " << _flow.flow_id()
        << " _base_rtt=" << _base_rtt
        << " _base_bdp=" << _base_bdp
        << " _bdp=" << _bdp
        << " _min_cwnd=" << _min_cwnd
        << " _maxwnd=" << _maxwnd
        << " _cwnd=" << _cwnd
        << endl; */
}

void UecSrc::initRccc(mem_b cwnd, simtime_picosec peer_rtt) {
    _receiver_based_cc = true;
    _base_rtt = peer_rtt;
    _base_bdp = timeAsSec(_base_rtt)*(_nic.linkspeed()/8);
    _bdp = _base_bdp;

    setMaxWnd(1.5*_bdp);
    setConfiguredMaxWnd(1.5*_bdp);

    if (cwnd == 0) {
        _cwnd = _maxwnd;
    } else {
        _cwnd = cwnd;
    }

    cout << "Initialize per-instance RCCC parameters:"
        << " flowid " << _flow.flow_id()
        << " _base_rtt=" << _base_rtt
        << " _base_bdp=" << _base_bdp
        << " _bdp=" << _bdp
        << " _maxwnd=" << _maxwnd
        << " _cwnd=" << _cwnd
        << endl;
}



#define INIT_PULL 100000000  // needs to be large enough we don't map
                            // negative pull targets (where
                            // credit_spec > backlog) to less than
                            // zero and suffer underflow.  Real
                            // implementations will properly handle
                            // modular wrapping.

/*
scaling_factor_a = current_BDP/100Gbps*net_base_rtt //12us scaling_factor_b = 12/target_Qdelay
beta = 5*scaling_factor_a
gamma = 0.15* scaling_factor_a
alpha = 4.0* scaling_factor_a*scaling_factor_b/base_rtt gamma_g = 0.8
*/

////////////////////////////////////////////////////////////////
//  UEC NIC
////////////////////////////////////////////////////////////////

UecNIC::UecNIC(id_t src_num, EventList& eventList, linkspeed_bps linkspeed, uint32_t ports)
    : EventSource(eventList, "uecNIC"), NIC(src_num)  {
    _nodename = "uecNIC" + to_string(src_num);
    _control_size = 0;
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
    _ratio_data = 1;
    _ratio_control = 10;
    _crt = 0;
}

// srcs call request_sending to see if they can send now.  If the
// answer is no, they'll be called back when it's time to send.
const Route* UecNIC::requestSending(UecSrc& src) {
    if (UecSrc::_debug) {
        cout << src.nodename() << " requestSending at "
             << timeAsUs(EventList::getTheEventList().now()) << endl;
    }
    if (_busy_ports == _no_of_ports) {
        // we're already sending on all ports
        /*
        if (_active_srcs.empty() && _control.empty()) {
            // need to schedule the callback
            eventlist().sourceIsPending(*this, _send_end_time);
        }
        */
        _active_srcs.push_back(&src);
        return NULL;
    }
    assert(/*_active_srcs.empty() &&*/ _control.empty());
    uint32_t portnum = findFreePort();
    return src.getPortRoute(portnum);
}

uint32_t UecNIC::findFreePort() {
    assert(_busy_ports < _no_of_ports);
    do {
        _rr_port = (_rr_port + 1) % _no_of_ports;

    } while (_ports[_rr_port].busy);
    return _rr_port;
}

uint32_t UecNIC::sendOnFreePortNow(simtime_picosec endtime, const Route* rt) {
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
void UecNIC::startSending(UecSrc& src, mem_b pkt_size, const Route* rt) {
    if (UecSrc::_debug) {
        cout << src.nodename() << " startSending at "
             << timeAsUs(EventList::getTheEventList().now()) << endl;
    }

    
    if (!_active_srcs.empty()) {
        UecSrc* queued_src = _active_srcs.front();
        _active_srcs.pop_front();
        assert(queued_src == &src);
    }

    simtime_picosec endtime = eventlist().now() + (pkt_size * 8 * timeFromSec(1.0)) / _linkspeed;
    sendOnFreePortNow(endtime, rt);
}

// srcs call cantSend when they previously requested to send, and now its their turn, they can't for
// some reason.
void UecNIC::cantSend(UecSrc& src) {
    if (UecSrc::_debug) {
        cout << src.nodename() << " cantSend at " << timeAsUs(EventList::getTheEventList().now())
             << endl;
    }

    if (_active_srcs.empty() && _control.empty()) {
        // it was an immediate send, so nothing to do if we can't send after all
        return;
    }
    if (!_active_srcs.empty()) {

        UecSrc* queued_src = _active_srcs.front();
        _active_srcs.pop_front();

        assert(queued_src == &src);
        assert(_busy_ports < _no_of_ports);

        if (!_active_srcs.empty()) {
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

void UecNIC::sendControlPacket(UecBasePacket* pkt, UecSrc* src, UecSink* sink) {
    assert((src || sink) && !(src && sink));
    
    _control_size += pkt->size();
    CtrlPacket cp = {pkt, src, sink};
    _control.push_back(cp);

    if (UecSrc::_debug) {
        cout << "NIC " << this << " request to send control packet of type " << pkt->str()
             << " control queue size " << _control_size << " " << _control.size() << endl;
    }

    if (_busy_ports == _no_of_ports) {
        // all ports are busy
        if (UecSrc::_debug) {
            cout << "NIC sendControlPacket " << this << " already sending on all ports\n";
        }
    } else {
        // send now!
        sendControlPktNow();
    }
}

// actually do the send of a queued control packet
void UecNIC::sendControlPktNow() {
    assert(!_control.empty());
    assert(_busy_ports != _no_of_ports);
    
    CtrlPacket cp = _control.front();
    _control.pop_front();
    UecBasePacket* p = cp.pkt;

    simtime_picosec endtime  = eventlist().now() + (p->size() * 8 * timeFromSec(1.0)) / _linkspeed;
    uint32_t port_to_use = sendOnFreePortNow(endtime, NULL);
    if (UecSrc::_debug)
        cout << "NIC " << this << " send control of size " << p->size() << " at "
             << timeAsUs(eventlist().now()) << endl;

    _control_size -= p->size();
    // At the NIC, only control packets or data packets with a payload size of zero are permitted to be transmitted at a higher priority.
    assert(p->route() == NULL || (p->type() == UECDATA && p->size() == UecBasePacket::ACKSIZE));
    const Route* route;
    if (cp.src)
        route = cp.src->getPortRoute(port_to_use);
    else
        route = cp.sink->getPortRoute(port_to_use);
    p->set_route(*route);
    p->sendOn();
}


void UecNIC::doNextEvent() {
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

    if (UecSrc::_debug)
        cout << "NIC " << this << " doNextEvent at " << timeAsUs(eventlist().now()) << endl;

    if (!_active_srcs.empty() && !_control.empty()) {
        _crt++;

        if (_crt >= (_ratio_control + _ratio_data))
            _crt = 0;

        if (UecSrc::_debug) {
            cout << "NIC " << this << " round robin time between srcs " << _active_srcs.size()
                 << " and control " << _control.size() << " " << _crt;
        }

        if (_crt < _ratio_data) {
            // it's time for the next source to send
            UecSrc* queued_src = _active_srcs.front();
            const Route* route = queued_src->getPortRoute(findFreePort());
            queued_src->timeToSend(*route);

            if (UecSrc::_debug)
                cout << " send data " << endl;

            return;
        } else {
            sendControlPktNow();
            return;
        }
    }

    if (!_active_srcs.empty()) {
        UecSrc* queued_src = _active_srcs.front();
        const Route* route = queued_src->getPortRoute(findFreePort());
        queued_src->timeToSend(*route);

        if (UecSrc::_debug)
            cout << "NIC " << this << " send data ONLY " << endl;
    } else if (!_control.empty()) {
        sendControlPktNow();
    }
}



////////////////////////////////////////////////////////////////
//  UEC SRC PORT
////////////////////////////////////////////////////////////////
UecSrcPort::UecSrcPort(UecSrc& src, uint32_t port_num)
    : _src(src), _port_num(port_num) {
}

void UecSrcPort::setRoute(const Route& route) {
    _route = &route;
}

void UecSrcPort::receivePacket(Packet& pkt) {
    _src.receivePacket(pkt, _port_num);
}

const string& UecSrcPort::nodename() {
    return _src.nodename();
}

////////////////////////////////////////////////////////////////
//  UEC SRC
////////////////////////////////////////////////////////////////

UecSrc::UecSrc(TrafficLogger* trafficLogger, 
               EventList& eventList,
			   unique_ptr<UecMultipath> mp, 
               UecNIC& nic, 
               uint32_t no_of_ports, 
               bool rts)
        : EventSource(eventList, "uecSrc"), 
          _mp(move(mp)),
          _nic(nic), 
          _msg_tracker(),
          _last_event_time(),
          _flow(trafficLogger)
          {
    assert(_mp != nullptr);
    
    _mp->set_debug_tag(_flow.str());
    
    _node_num = _global_node_count++;
    _nodename = "uecSrc " + to_string(_node_num);

    _no_of_ports = no_of_ports;
    _ports.resize(no_of_ports);
    for (uint32_t p = 0; p < _no_of_ports; p++) {
        _ports[p] = new UecSrcPort(*this, p);
    }

    _rtx_timeout_pending = false;
    _rtx_timeout = timeInf;
    _rto_timer_handle = eventlist().nullHandle();

    _probe_timer_handle = eventlist().nullHandle();
    _probe_timer_when = 0;
    _probe_seqno = 0; 
    _probe_send_time = 0; 

    _flow_logger = NULL;

    _logger = NULL;

    _maxwnd = 50 * _mtu;
    _cwnd = _maxwnd;
    _flow_size = 0;
    _done_sending = false;
    _backlog = 0;
    _rtx_backlog = 0;
    _pull_target = INIT_PULL;
    _pull = INIT_PULL;
    _credit = _maxwnd;
    _speculating = true;
    _in_flight = 0;
    _highest_sent = 0;
    _send_blocked_on_nic = false;
    _inc_bytes = 0;

    //must be at least two, to allow us to encode assumed_bad state.
    _last_rts = 0;

    // stats for debugging
    _stats = {};

    // by default, end silently
    _end_trigger = 0;

    _dstaddr = UINT32_MAX;
    //_route = NULL;
    _mtu = Packet::data_packet_size();
    _mss = _mtu - _hdr_size;

    _debug_src = UecSrc::_debug;
    _bdp = 0;
    _base_rtt = 0;

    _bytes_ignored = 0;
    _bytes_to_ignore = 0;
    _trigger_qa = false;
    _achieved_bytes = 0;
    _qa_endtime = 0;
    _fi_count = 0;

    if (_sender_based_cc) {
        switch (_sender_cc_algo) {
            case DCTCP:
                updateCwndOnAck = &UecSrc::updateCwndOnAck_DCTCP;
                updateCwndOnNack = &UecSrc::updateCwndOnNack_DCTCP;
                break;
            case NSCC:
                updateCwndOnAck = &UecSrc::updateCwndOnAck_NSCC;
                updateCwndOnNack = &UecSrc::updateCwndOnNack_NSCC;
                break;
            case CONSTANT:
                updateCwndOnAck = &UecSrc::dontUpdateCwndOnAck;
                updateCwndOnNack = &UecSrc::dontUpdateCwndOnNack;
                break;
            default:
                cout << "Unknown CC algo specified " << _sender_cc_algo << endl;
                assert(0);
        }
    }
    //if (_node_num == 2) _debug_src = true; // use this to enable debugging on one flow at a
    // time
    _received_bytes = 0;
    _recvd_bytes = 0;

    _highest_recv_seqno = 0;
    _highest_rtx_sent = 0;

    _nscc_overall_stats = {};
    _nscc_fulfill_stats = {};
}

void UecSrc::delFromSendTimes(simtime_picosec time, UecDataPacket::seq_t seq_no) {
    //cout << eventlist().now() << " flowid " << _flow.flow_id() << " _send_times.erase " << time << " for " << seq_no << endl;
    auto snd_seq_range = _send_times.equal_range(time);
    auto snd_it = snd_seq_range.first;
    while (snd_it != snd_seq_range.second) {
        if (snd_it->second == seq_no) {
            _send_times.erase(snd_it);
            break;
        } else {
            ++snd_it;
        }
    }
}

void UecSrc::connectPort(uint32_t port_num,
                          Route& routeout,
                          Route& routeback,
                          UecSink& sink,
                          simtime_picosec start_time) {
    _ports[port_num]->setRoute(routeout);
    //_route = &routeout;

    if (port_num == 0) {
        _sink = &sink;
        //_flow.set_id(get_id());  // identify the packet flow with the UEC source that generated it
        _flow._name = _name;

        if (start_time != TRIGGER_START || true) {
            eventlist().sourceIsPending(*this, start_time);
        }
    }
    assert(_sink == &sink);
    _sink->connectPort(port_num, *this, routeback);
}

void UecSrc::receivePacket(Packet& pkt, uint32_t portnum) {
    switch (pkt.type()) {
        case UECDATA: {
            _stats.bounces_received++;
            // TBD - this is likely a Back-to-sender packet
            cout << "UecSrc::receivePacket receive UECDATA packets\n";

            abort();
        }
        case UECRTS: {
            cout << "UecSrc::receivePacket receive UECRTS packets\n";

            abort();
        }
        case UECACK: {
            processAck((const UecAckPacket&)pkt);
            pkt.free();
            return;
        }
        case UECNACK: {
            processNack((const UecNackPacket&)pkt);
            pkt.free();
            return;
        }
        case UECPULL: {
            processPull((const UecPullPacket&)pkt);
            pkt.free();
            return;
        }
        default: {
            cout << "UecSrc::receivePacket receive default\n";

            abort();
        }
    }
}

mem_b UecSrc::handleAckno(UecDataPacket::seq_t ackno) {
    auto i = _tx_bitmap.find(ackno);
    if (i == _tx_bitmap.end()) {
        // The ackno is either in tx_bitmap or in rtx_queue
        // or in neither, but never in both.
        // Hence, if it's not in _tx_bitmap, check if it's 
        // in _rtx_queue and remove and correct. 
        // If ackno is in neither, there is nothing else
        // to do here.
        auto rtx_i = _rtx_queue.find(ackno);
        if (rtx_i != _rtx_queue.end()) {
            // packet was in RTX queue
            mem_b pkt_size = rtx_i->second;
            _rtx_queue.erase(rtx_i);
            _rtx_backlog -= pkt_size;
            _in_flight += pkt_size; // don't double count - we decremented when we marked for rtx
            if (_debug_src) {
                cout << "found pkt " << ackno << " in rtx queue\n";
            }

            if (_msg_tracker.has_value()) {
                _msg_tracker.value()->addSAck(ackno);
            }
        }
        return 0;
    } else {
        // If ackno is in tx_bitmap, it means we have recentely 
        // send out an packet, either for the first time or
        // an rtx packet. Since the current ack tells us that
        // it has been received already, we can remove it from 
        // _tx_bitmap.
        simtime_picosec send_time = i->second.send_time;

        mem_b pkt_size = i->second.pkt_size;
        
        if (_debug_src)
            cout << _flow.str() << " " << _nodename << " handleAck " << ackno << " flow " << _flow.str() << endl;
        if(_flow.flow_id() == _debug_flowid ) {
              cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " handleAck ackno " << ackno
                   << endl;
        } 

        if (_msg_tracker.has_value()) {
            _msg_tracker.value()->addSAck(ackno);
        }

        _tx_bitmap.erase(i);
        // _send_times.erase(send_time);
        delFromSendTimes(send_time, ackno);

        if (send_time == _rto_send_time) {
            recalculateRTO();
        }

        return pkt_size;
    }


    abort(); // dead code below
    /*
    
    // mem_b pkt_size = i->second.pkt_size;
    simtime_picosec send_time = i->second.send_time;

    mem_b pkt_size = i->second.pkt_size;
    
    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " handleAck " << ackno << " flow " << _flow.str() << endl;
    if(_flow.flow_id() == _debug_flowid ){
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " handleAck ackno " << ackno
             << endl;
    }    
    _tx_bitmap.erase(i);
    // _send_times.erase(send_time);
    delFromSendTimes(send_time, ackno);

    if (send_time == _rto_send_time) {
        recalculateRTO();
    }

    return pkt_size;
    */
}

mem_b UecSrc::handleCumulativeAck(UecDataPacket::seq_t cum_ack) {
    mem_b newly_acked = 0;

    // free up anything cumulatively acked
    while (!_rtx_queue.empty()) {
        auto seqno = _rtx_queue.begin()->first;

        if (seqno < cum_ack) {
            mem_b pkt_size = _rtx_queue.begin()->second;
            _rtx_queue.erase(_rtx_queue.begin());
            _rtx_backlog -= pkt_size;
            _in_flight += pkt_size; // don't double count - we decremented when we marked for rtx
        } else {
            break;
        }
    }

    if (_msg_tracker.has_value()) {
        _msg_tracker.value()->addCumAck(cum_ack);
    }

    auto i = _tx_bitmap.begin();
    while (i != _tx_bitmap.end()) {
        auto seqno = i->first;
        // cumulative ack is next expected packet, not yet received
        if (seqno >= cum_ack) {
            // nothing else acked
            break;
        }
        simtime_picosec send_time = i->second.send_time;

        newly_acked += i->second.pkt_size;

        if (_debug_src)
            cout << _flow.str() << " " << _nodename << " handleCumAck " << seqno << " flow " << _flow.str() << endl;
        if(_flow.flow_id() == _debug_flowid ){
            cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " handleCumulativeAck seqno " << seqno
                << endl;
        }  
        _tx_bitmap.erase(i);
        i = _tx_bitmap.begin();
        // _send_times.erase(send_time);
        delFromSendTimes(send_time, seqno);
        if (send_time == _rto_send_time) {
            recalculateRTO();
        }
        //we can safely remove the number of retranmission times if we receive the packets' ACK
        auto rtx_time = _rtx_times.find(seqno);
        if (rtx_time != _rtx_times.end()){
            _rtx_times.erase(rtx_time);
        }
    }
    return newly_acked;
}

void UecSrc::handlePull(UecBasePacket::pull_quanta pullno) {
    if (pullno > _pull) {
        UecBasePacket::pull_quanta extra_credit = pullno - _pull;
        _credit += UecBasePacket::unquantize(extra_credit);
        if (_credit > _configured_maxwnd)
            _credit = _configured_maxwnd;
        _pull = pullno;
    }
    if(_flow.flow_id() == _debug_flowid){
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " credit " << _credit << endl; 
    }
}

bool UecSrc::checkFinished(UecDataPacket::seq_t cum_ack) {

    if (_done_sending) {
        // assert(_backlog == 0);
        // assert(_rtx_queue.empty());
        // if (_pdc.has_value()) {
        //     assert(_pdc->checkFinished());
        // }
    } else { 
        if (_msg_tracker.has_value()) {
            if (_msg_tracker.value()->checkFinished()) {
                /* cout << "Flow " << _name << " flowId " << flowId() << " " << _nodename 
                    << " finished at " << timeAsUs(eventlist().now()) 
                    << " total messages " << _msg_tracker.value()->getMsgCompleted()
                    << " total packets " << cum_ack 
                    << " RTS " << _stats.rts_pkts_sent 
                    << " total bytes " << ((mem_b)cum_ack - _stats.rts_pkts_sent) * _mss
                    << " in_flight now " << _in_flight 
                    << " fair_inc " << _nscc_overall_stats.inc_fair_bytes
                    << " prop_inc " << _nscc_overall_stats.inc_prop_bytes
                    << " fast_inc " << _nscc_overall_stats.inc_fast_bytes 
                    << " eta_inc " << _nscc_overall_stats.inc_eta_bytes 
                    << " multi_dec -" << _nscc_overall_stats.dec_multi_bytes 
                    << " quick_dec -" << _nscc_overall_stats.dec_quick_bytes 
                    << " nack_dec -" << _nscc_overall_stats.dec_nack_bytes 
                    << endl; */
                cancelRTO();
                _done_sending = true;

                // ATLAHS 
                EventOver *flow_over = new EventOver(from, to, _flow_size, tag, eventlist().now(), AtlahsEventType::SEND_EVENT_OVER);
                flow_over->node = lgs_node;
                flow_over->start_time_event = _flow_start_time;
                if (_atlahs_api) {
                    if (_atlahs_api->print_stats_flows) {
                        _atlahs_api->flowInfos.push_back(FlowInfo(timeAsUs(_flow_start_time), timeAsUs(eventlist().now()), timeAsUs(eventlist().now() - _flow_start_time), _flow_size, 1, _cwnd));
                    }
                    _atlahs_api->EventFinished(*flow_over);
                }
            }
        } else {
            if ((((int64_t)cum_ack - _stats.rts_pkts_sent) * _mss) >= (int64_t)_flow_size) {
                /* cout << "Flow " << _name << " flowId " << flowId() << " " << _nodename 
                    << " finished at " << timeAsUs(eventlist().now()) 
                    << " total messages " << 1 
                    << " total packets " << cum_ack 
                    << " RTS " << _stats.rts_pkts_sent 
                    << " total bytes " << ((mem_b)cum_ack - _stats.rts_pkts_sent) * _mss
                    << " in_flight now " << _in_flight 
                    << " fair_inc " << _nscc_overall_stats.inc_fair_bytes
                    << " prop_inc " << _nscc_overall_stats.inc_prop_bytes
                    << " fast_inc " << _nscc_overall_stats.inc_fast_bytes 
                    << " eta_inc " << _nscc_overall_stats.inc_eta_bytes 
                    << " multi_dec -" << _nscc_overall_stats.dec_multi_bytes 
                    << " quick_dec -" << _nscc_overall_stats.dec_quick_bytes 
                    << " nack_dec -" << _nscc_overall_stats.dec_nack_bytes 
                    << endl; */
                _speculating = false;
                if (_end_trigger) {
                    _end_trigger->activate();
                }
                if (_flow_logger) {
                    _flow_logger->logEvent(_flow, *this, FlowEventLogger::FINISH, _flow_size, cum_ack);
                }
                cancelRTO();
                // ATLAHS 
                EventOver *flow_over = new EventOver(from, to, _flow_size, tag, eventlist().now(), AtlahsEventType::SEND_EVENT_OVER);
                flow_over->node = lgs_node;
                flow_over->start_time_event = _flow_start_time;
                if (_atlahs_api) {
                    if (_atlahs_api->print_stats_flows) {
                        _atlahs_api->flowInfos.push_back(FlowInfo(timeAsUs(_flow_start_time), timeAsUs(eventlist().now()), timeAsUs(eventlist().now() - _flow_start_time), _flow_size, 1, _cwnd));
                    }
                    _atlahs_api->EventFinished(*flow_over);
                }
                _done_sending = true;
            }
        }
    }

    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " checkFinished "
             << " cum_acc " << cum_ack << " mss " << _mss << " RTS sent " << _stats.rts_pkts_sent
             << " total bytes " << ((int64_t)cum_ack - _stats.rts_pkts_sent) * _mss 
             << " flow_size " << _flow_size 
             << " backlog " << _backlog
             << " rtx_queue " << _rtx_queue.size()
             << " done_sending " << _done_sending << endl;

    return _done_sending;
}

bool UecSrc::isTotallyFinished() {
    if (_msg_tracker.has_value()) {
        return _msg_tracker.value()->isTotallyFinished();
    } else {
        return _done_sending;
    }
}

bool UecSrc::validateSendTs(UecBasePacket::seq_t acked_psn, bool rtx_echo) {
    auto rtx_time = _rtx_times.find(acked_psn);
    if(rtx_time == _rtx_times.end())
        return false;

    if ((rtx_time->second == 0 && rtx_echo == false) 
     || (rtx_time->second == 1 && rtx_echo == true)) {
        return true;
    } else {
        return false;
    }
}

void UecSrc::processAck(const UecAckPacket& pkt) {
    _nic.logReceivedCtrl(pkt.size());
    
    auto cum_ack = pkt.cumulative_ack();
    bool rtx_echo = pkt.rtx_echo();
    //handle flight_size based on recvd_bytes in packet.
    uint64_t newly_recvd_bytes = 0;

    if (pkt.recvd_bytes() > _recvd_bytes){
        newly_recvd_bytes = pkt.recvd_bytes() - _recvd_bytes;
        _recvd_bytes = pkt.recvd_bytes();

        _achieved_bytes += newly_recvd_bytes;
        _received_bytes += newly_recvd_bytes;
        _bytes_ignored += newly_recvd_bytes;
    }

    if (_debug_src) {
        cout << "processAck " << cum_ack << " ref_epsn " << pkt.acked_psn() << " recvd_bytes " << _recvd_bytes << " newly_recvd_bytes " << newly_recvd_bytes << endl;
    }
    _stats.acks_received++;

    //decrease flightsize.
    _in_flight -= newly_recvd_bytes;
    // We cannot run this next line's check here since 
    // _in_flight could be corrected (increased) in either
    // handleCumulativeAck or handleAckno.
    // assert(_in_flight >= 0);

    if (_sender_based_cc && pkt.rcv_wnd_pen() < 255) {
            sint64_t window_decrease = newly_recvd_bytes - newly_recvd_bytes * pkt.rcv_wnd_pen() / 255;
            _cwnd = max(_cwnd-window_decrease, (mem_b)_mtu);
    }

    //compute RTT sample
    auto acked_psn = pkt.acked_psn();
    auto i = _tx_bitmap.find(acked_psn);
    auto rtx_time = _rtx_times.find(acked_psn);
    uint32_t ooo = pkt.ooo();

    mem_b pkt_size;
    simtime_picosec delay;
    simtime_picosec raw_rtt = 0;
    simtime_picosec send_time = 0;

    if (i != _tx_bitmap.end() && validateSendTs(acked_psn, pkt.rtx_echo()) && (!pkt.is_probe_ack()) ) {
    //a timestamp is valid if 
    //1. the received ack is new packet and no retransmission at local record;
    //or 2. the received ack is a retransmitted packet and local record shows this packet only gets retransmitted once. 
        if(_flow.flow_id() == _debug_flowid ){
            cout <<  timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() 
                << " rtx_times "<< rtx_time->second
                << " rtx_echo " << rtx_echo 
                << " packet_type " << pkt.is_probe_ack()
                << " _probe_psn " << _probe_seqno
                << " psn " << pkt.acked_psn()
                << endl;
        }
        //auto seqno = i->first;
        send_time = i->second.send_time;
        pkt_size = i->second.pkt_size;
        raw_rtt = eventlist().now() - send_time;

        if (!pkt.is_rts()) {
            update_base_rtt(raw_rtt);
        }
        
        if (raw_rtt >= _base_rtt) {
            update_delay(raw_rtt, true, pkt.ecn_echo());
            delay = raw_rtt - _base_rtt; 
        } else {
            delay = get_avg_delay();
        }
    } else {
        // this can happen when the ACK arrives later than a cumulative ACK covering the NACKed
        // packet.
        if (UecSrc::_debug)
            cout << "Can't find send record for seqno " << acked_psn << endl;
        if (pkt.is_probe_ack()){
            if (_probe_seqno == pkt.acked_psn()){
                _raw_rtt = eventlist().now() - _probe_send_time ;
                if (_raw_rtt < _base_rtt){
                    delay = 0;
                    _raw_rtt = _base_rtt;
                }else{
                    delay = _raw_rtt - _base_rtt;
                    update_delay(_raw_rtt, true, pkt.ecn_echo());
                }
                if(_flow.flow_id() == _debug_flowid ){
                    cout <<  timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " _probe_seqno " << _probe_seqno
                        << " delay " << timeAsUs(delay) 
                        << endl;
                }
            }else{
                delay = get_avg_delay();
            }
            pkt_size = 0;
        }else{
            pkt_size = _mtu;
            delay = get_avg_delay();
        }
    }

    handleCumulativeAck(cum_ack);

    if (_debug_src)
        cout << "At " << timeAsUs(eventlist().now()) << " " << _flow.str() << " " << _nodename << " processAck cum_ack: " << cum_ack << " flow " << _flow.str() << endl;

    auto ackno = pkt.ref_ack();

    uint64_t bitmap = pkt.bitmap();

    if (_debug_src)
        cout << "    ref_ack: " << ackno << " bitmap: " << bitmap << endl;

    while (bitmap > 0) {
        if (bitmap & 1) {
            if (_debug_src)
                cout << "    Sack " << ackno << " flow " << _flow.str() << endl;

            handleAckno(ackno);
            if (_highest_recv_seqno < ackno){
                _highest_recv_seqno = ackno;
            }
        }
        ackno++;
        bitmap >>= 1;
    }

    // We ran both potential _in_flight correcting functions
    // now check if we are in the negative.
    //assert(_in_flight >= 0);


    _mp->processEv(pkt.ev(), pkt.ecn_echo() ? UecMultipath::PATH_ECN : UecMultipath::PATH_GOOD);

    if(_flow.flow_id() == _debug_flowid ){
        cout <<  timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " track_avg_rtt " << timeAsUs(get_avg_delay())
            << " rtt " << timeAsUs(raw_rtt) << " skip " << pkt.ecn_echo()  << " ev " << pkt.ev()
            << " cum_ack " << cum_ack
            << " bitmap_base " << pkt.ref_ack()
            << " ooo " << ooo
            << " cwnd " << _cwnd/get_avg_pktsize()
            << " _achieved_bytes " << _achieved_bytes
            << " acked_psn " << acked_psn
            << " sending_time " << timeAsUs(send_time)
            << endl;
    }
    if (_sender_based_cc){
        /*if (pkt.ecn_echo()){
            (this->*updateCwndOnAck)(pkt.ecn_echo(), delay, pkt_size);
            (this->*updateCwndOnAck)(false, delay, newly_recvd_bytes - pkt_size);
        }
        else */
        (this->*updateCwndOnAck)(pkt.ecn_echo(), delay, newly_recvd_bytes);
    }

    if (_debug_src) {
        cout << "At " << timeAsUs(eventlist().now()) << " " << _flow.str() << " " << _nodename << " processAck: " << cum_ack << " flow " << _flow.str() << " cwnd " << _cwnd << " flightsize " << _in_flight << " delay " << timeAsUs(delay) << " newlyrecvd " << newly_recvd_bytes << " skip " << pkt.ecn_echo() << " raw rtt " << raw_rtt << endl;
    }

    if (_sender_based_cc && _enable_sleek) {
        //probe packets
        if (_probe_timer_when != 0){
            if (_probe_timer_handle->second != this){
                if(_flow.flow_id() == _debug_flowid ){
                    cout <<  timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " an assert soon"<< endl;
                }                
            }
            eventlist().cancelPendingSourceByHandle(*this, _probe_timer_handle);
            _probe_timer_when = 0;
            _probe_timer_handle = eventlist().nullHandle();
        }
        if (cum_ack < _highest_sent || _backlog > 0){
            if (_backlog == 0){
                _probe_timer_when = eventlist().now() + (_base_rtt+_target_Qdelay);            
            }else{
                _probe_timer_when = eventlist().now() + probe_first_trial_time*_base_rtt;
            }
            _probe_timer_handle = eventlist().sourceIsPendingGetHandle(*this, _probe_timer_when);
        }
        if(pkt.is_probe_ack() && delay < _target_Qdelay){
            _loss_recovery_mode = true;
            _recovery_seqno = _highest_sent;
            _highest_rtx_sent = cum_ack;
            if(_flow.flow_id() == _debug_flowid ){
                cout <<  timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " enter_loss_probe " << " _avg_delay " << timeAsUs(_avg_delay)<< endl;
            }
        }
        runSleek(ooo, cum_ack);
    }

    stopSpeculating();

    if (checkFinished(cum_ack)) {
        return;
    }

    sendIfPermitted();
}

void UecSrc::updateCwndOnAck_DCTCP(bool skip, simtime_picosec rtt, mem_b newly_acked_bytes) {
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

void UecSrc::updateCwndOnNack_DCTCP(bool skip, mem_b nacked_bytes, bool last_hop) {
    _cwnd -= nacked_bytes;
    _cwnd = max(_cwnd, (mem_b)_mtu);
}

bool UecSrc::can_send_RCCC() {
    assert(_receiver_based_cc);
    return credit() > 0;
}

bool UecSrc::can_send_NSCC(mem_b pkt_size) {
    assert(_sender_based_cc);
    return (pkt_size > 0) 
    	   && (((!_loss_recovery_mode && _cwnd >= _in_flight + pkt_size) 
                || (_loss_recovery_mode && (!_rtx_queue.empty() || _cwnd >= _in_flight + pkt_size))));
}

void UecSrc::set_cwnd_bounds() {
    if (_cwnd < _min_cwnd)
        _cwnd = _min_cwnd;

    if (_cwnd > _maxwnd)
        _cwnd = _maxwnd;
}

bool UecSrc::quick_adapt(bool is_loss, bool skip, simtime_picosec delay) {
    bool qa_done_or_ignore = false;

    if (_disable_quick_adapt) {
        return false;
    }

    if (_debug_src){
        cout << "At " << timeAsUs(eventlist().now()) << " " << _flow.str() << " quickadapt called is loss "<< is_loss << " delay " << delay 
             << " qa_endtime " << timeAsUs(_qa_endtime) << " trigger qa " << _trigger_qa << endl;
    }

    if (_bytes_ignored < _bytes_to_ignore && skip) {
        qa_done_or_ignore = true;
    } else if (eventlist().now() > _qa_endtime){
        if (_qa_endtime != 0 
                && (_trigger_qa || is_loss || (delay > _qa_threshold)) 
                && _achieved_bytes < (_maxwnd >> _qa_gate)) {

            if (_debug_src) {
                cout << "At " << timeAsUs(eventlist().now()) << " " << _flow.str() << " running quickadapt, CWND is " << _cwnd << " setting it to " << _achieved_bytes <<  endl;
            }

            if (_cwnd < _achieved_bytes){
                if (_debug_src) {
                    cout << "This shouldn't happen: QUICK ADAPT MIGHT INCREASE THE CWND" << endl;
                }
            } 
            
            mem_b before = _cwnd;
            _cwnd = max(_achieved_bytes, _min_cwnd); //* _qa_scaling;
            _nscc_overall_stats.dec_quick_bytes += before - _cwnd;
            _nscc_fulfill_stats.dec_quick_bytes += before - _cwnd;

            if (_flow.flow_id() == _debug_flowid) {
                cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id()
                     << " quick_adapt  _nscc_cwnd " << _cwnd << " is_loss " << is_loss 
                     << endl;
            }

            _bytes_to_ignore = _in_flight;
            _bytes_ignored = 0;
            _trigger_qa = false;
            qa_done_or_ignore = true;
        }
        _achieved_bytes = 0;
        _qa_endtime = eventlist().now() + _base_rtt + _target_Qdelay;
    }

    if (qa_done_or_ignore) {
        _inc_bytes = 0;
        _received_bytes = 0;
    }

    return qa_done_or_ignore;
}

void UecSrc::fair_increase(uint32_t newly_acked_bytes){
    mem_b before = _inc_bytes;
    _inc_bytes += _fi * newly_acked_bytes; //increase by 16Million!
    _nscc_fulfill_stats.inc_fair_bytes += _inc_bytes - before;
}

void UecSrc::proportional_increase(uint32_t newly_acked_bytes,simtime_picosec delay){
    fast_increase(newly_acked_bytes, delay);
    if (_increase)
        return;
    
    //make sure targetQdelay > delay;
    assert(_target_Qdelay > delay);

    mem_b before = _inc_bytes;
    _inc_bytes += _alpha * newly_acked_bytes * (_target_Qdelay - delay);
    _nscc_fulfill_stats.inc_prop_bytes += _inc_bytes - before;
}

void UecSrc::fast_increase(uint32_t newly_acked_bytes,simtime_picosec delay){
    if (delay < timeFromUs(1u)){
        _fi_count += newly_acked_bytes;
        if (_fi_count > _cwnd || _increase){
            mem_b before = _cwnd;
            _cwnd += newly_acked_bytes * _fi_scale;
            _nscc_overall_stats.inc_fast_bytes += _cwnd - before;
            _nscc_fulfill_stats.inc_fast_bytes += _cwnd - before;

            _increase = true;
            return;
        }
    }
    else  {
        _fi_count = 0;
    }
    _increase = false;
}

void UecSrc::multiplicative_decrease() {
    _increase = false;
    _fi_count = 0;
    simtime_picosec avg_delay = get_avg_delay();
    if (avg_delay > _target_Qdelay){
        if (eventlist().now() - _last_dec_time > _base_rtt){
            mem_b before = _cwnd;
            _cwnd *= max(1-_gamma*(avg_delay-_target_Qdelay)/avg_delay, 0.5);/*_max_md_jump instead of 1*/
            _cwnd = max(_cwnd, _min_cwnd);
            _nscc_overall_stats.dec_multi_bytes += before - _cwnd;
            _nscc_fulfill_stats.dec_multi_bytes += before - _cwnd;

            _last_dec_time = eventlist().now();
        }
    }
}

void UecSrc::fulfill_adjustment(){
    assert(_bdp > 0);

    _cwnd += _inc_bytes / _cwnd;

    _nscc_fulfill_stats.inc_fair_bytes /= _cwnd;
    _nscc_fulfill_stats.inc_prop_bytes /= _cwnd;

    _nscc_overall_stats.inc_fair_bytes += _nscc_fulfill_stats.inc_fair_bytes;
    _nscc_overall_stats.inc_prop_bytes += _nscc_fulfill_stats.inc_prop_bytes;

    if ((eventlist().now() - _last_adjust_time) >= _adjust_period_threshold) {
        _cwnd += _eta;
        _nscc_overall_stats.inc_eta_bytes += _eta;
        _nscc_fulfill_stats.inc_eta_bytes += _eta;
        _last_adjust_time = eventlist().now();
    }

    if (_debug_src) {
        cout << timeAsUs(eventlist().now())
             << " flowid " << _flow.flow_id()
             << " Running fulfill adjustment cwnd " << _cwnd 
             << " inc " << _nscc_fulfill_stats.inc_fair_bytes + _nscc_fulfill_stats.inc_prop_bytes 
             << " fair_inc " << _nscc_fulfill_stats.inc_fair_bytes
             << " prop_inc " << _nscc_fulfill_stats.inc_prop_bytes
             << " fast_inc " << _nscc_fulfill_stats.inc_fast_bytes 
             << " eta_inc " << _nscc_fulfill_stats.inc_eta_bytes 
             << " multi_dec -" << _nscc_fulfill_stats.dec_multi_bytes 
             << " quick_dec -" << _nscc_fulfill_stats.dec_quick_bytes 
             << " nack_dec -" << _nscc_fulfill_stats.dec_nack_bytes 
             << " avg-delay " << _avg_delay 
             << endl;
    }

    _inc_bytes = 0;
    _received_bytes = 0;

    _nscc_fulfill_stats = {};
}

void UecSrc::mark_packet_for_retransmission(UecBasePacket::seq_t psn, uint16_t pktsize){
    _in_flight -= pktsize;
    //assert (_in_flight>=0);
    _cwnd = max(_cwnd - pktsize, (mem_b)_mtu);
    if(_flow.flow_id() == _debug_flowid)
        cout <<timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<< " mark_packet_for_retransmission  _cwnd " << _cwnd << endl;    
    //_rtx_count ++;
}

void UecSrc::dontUpdateCwndOnAck(bool skip, simtime_picosec delay, mem_b newly_acked_bytes) {
}


void UecSrc::updateCwndOnAck_NSCC(bool skip, simtime_picosec delay, mem_b newly_acked_bytes) {
    // bool can_decrease = _exp_avg_ecn > _ecn_thresh;

    if (quick_adapt(false, skip, delay))
        return;

    if (!skip && delay >= _target_Qdelay) {
        fair_increase(newly_acked_bytes);
        if (_flow.flow_id() == _debug_flowid || UecSrc::_debug) {
            cout << timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<< " " << _flow.str() << " fair_increase _nscc_cwnd " << _cwnd 
                << " newly_acked_bytes " << newly_acked_bytes 
                << " fi " << _fi << endl;
        }
    } else if (!skip && delay < _target_Qdelay) {
        proportional_increase(newly_acked_bytes,delay);
        if (_flow.flow_id() == _debug_flowid || UecSrc::_debug) {
            cout << timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<< " " << _flow.str() << " proportional_increase _nscc_cwnd " << _cwnd << endl;
        }
    } else if (skip && delay >= _target_Qdelay) {    
        multiplicative_decrease();
        if (_flow.flow_id() == _debug_flowid || UecSrc::_debug) {
            cout << timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<< " " << _flow.str() << " multiplicative_decrease _nscc_cwnd " << _cwnd << endl;
        }
    } else if (skip && delay < _target_Qdelay) {
        // NOOP, just switch path
    }

    // Check here, fulfill_adjustment requires valid cwnd.
    set_cwnd_bounds();

    // if ( _received_bytes > _adjust_bytes_threshold || eventlist().now() - _last_adjust_time > _adjust_period_threshold ) {
    if ( _received_bytes > _adjust_bytes_threshold || eventlist().now() - _last_adjust_time > _adjust_period_threshold ) {
        if (_flow.flow_id() == _debug_flowid || UecSrc::_debug) {
            cout << timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<<  " " << _flow.str() << " fulfill_adjustmentx _nscc_cwnd " << _cwnd
                << " inc_bytes " << _inc_bytes
                << endl;
        }
        fulfill_adjustment();
        if (_flow.flow_id() == _debug_flowid || UecSrc::_debug) {
            cout << timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<< " " << _flow.str() << " fulfill_adjustment _nscc_cwnd " << _cwnd << endl;
        }
    }

    set_cwnd_bounds();

    if (_flow.flow_id() == _debug_flowid)
        cout << timeAsUs(eventlist().now()) <<" flowid " << _flow.flow_id()<< " final _nscc_cwnd " << _cwnd << " _basertt " << timeAsUs(_base_rtt)<< endl;
}

void UecSrc::updateCwndOnNack_NSCC(bool skip, mem_b nacked_bytes, bool last_hop) {
    bool adjust_cwnd = true;

    _bytes_ignored += nacked_bytes;
    _nscc_overall_stats.dec_nack_bytes += nacked_bytes;
    _nscc_fulfill_stats.dec_nack_bytes += nacked_bytes;

    // We use _network_rtt as an estimate for the trimming threshold
    // TODO: we might need to check if the NACK was generated by trimming,
    //       and handle the case if it was not at some point.
    update_delay(_base_rtt + _network_rtt, true, true);

    if (_flow.flow_id() == _debug_flowid)
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id()
             << " onnack  _nscc_cwnd " << _cwnd << endl;

    _trigger_qa = true;
    if (quick_adapt(true, true, 0)) {
        adjust_cwnd = false;
    }

    if (adjust_cwnd && (!_receiver_based_cc || !last_hop)) {
        _cwnd -= nacked_bytes;
        set_cwnd_bounds();
    }
}

void UecSrc::dontUpdateCwndOnNack(bool skip, mem_b nacked_bytes, bool last_hop) {
}

void UecSrc::update_base_rtt(simtime_picosec raw_rtt){
    if (_base_rtt > raw_rtt) {
        _base_rtt = raw_rtt;
        _bdp = timeAsUs(raw_rtt) * _nic.linkspeed() / 8000000; 
        _maxwnd = 1.5 * _bdp;
        
        if (UecSrc::_debug)
            cout << "Reinit BDP and MAXWND to "  << _bdp << " " << _maxwnd << " in pkts " << _maxwnd/_mtu << endl;
        if (_bdp == 0)
            cout <<timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " _bdp " << _bdp << " " << _maxwnd << " in pkts " << _maxwnd/_mtu << " raw_rtt " << timeAsUs(_raw_rtt) << endl;
    }
}

void UecSrc::update_delay(simtime_picosec raw_rtt, bool update_avg, bool skip){
    simtime_picosec delay = raw_rtt - _base_rtt;
    if(update_avg){

        if(skip == false && delay > _target_Qdelay){
            _avg_delay = _delay_alpha * _base_rtt*0.25 + (1-_delay_alpha) * _avg_delay;
        }else{
            if (delay > 5*_base_rtt)
            {
                double r = 0.0125;
                _avg_delay = r * delay + (1 - r) * _avg_delay;
            }
            else
            {
                _avg_delay = _delay_alpha * delay + (1 - _delay_alpha) * _avg_delay;
            }
        }
    }
    if (_debug_src) {
        cout << "Update delay with sample " << timeAsUs(delay) << " avg is " << timeAsUs(_avg_delay) << " base rtt is " << _base_rtt << endl;
    }
}

simtime_picosec UecSrc::get_avg_delay(){
    return _avg_delay;
}

uint16_t UecSrc::get_avg_pktsize(){
    return _mss;  // does not include header
}

void UecSrc::runSleek(uint32_t ooo, UecBasePacket::seq_t cum_ack) {
    mem_b avg_size = get_avg_pktsize();
    mem_b threshold = min((mem_b)(loss_retx_factor*_cwnd), _maxwnd);
    threshold = max(threshold, min_retx_config*avg_size);

    if(_flow.flow_id() == _debug_flowid || _debug_src ){
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " rtx_threshold " << threshold/avg_size
            << " ooo " << ooo
            << " _highest_rtx_sent " << _highest_rtx_sent
            << " cwnd_in_pkts " << _cwnd/avg_size
            << " cum_ack " << cum_ack
            << " _probe_timer_when "  << timeAsUs(_probe_timer_when)
            << " highest_sent " << _highest_sent
            << " _backlog " << _backlog
            << endl;
    }

    if (cum_ack >= _recovery_seqno && _loss_recovery_mode) {
        _loss_recovery_mode = false;
        if (_flow.flow_id() == _debug_flowid || _debug_src){
            cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " exit_loss " <<endl;
        }
    }

    if (ooo < threshold/avg_size && !_loss_recovery_mode)
        return;

    if (!_loss_recovery_mode && _rtx_queue.empty() ) {
        _loss_recovery_mode = true;
        _recovery_seqno = _highest_sent ;
        if (_flow.flow_id() == _debug_flowid || _debug_src ){
            cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " enter_loss " << " _highest_sent " << _highest_sent <<endl;
        }
    }

    // move the packet to the RTX queue
    for (UecBasePacket::seq_t rtx_seqno = cum_ack; 
          rtx_seqno < _recovery_seqno && rtx_seqno < (cum_ack + _cwnd/get_avg_pktsize()); 
          rtx_seqno ++ ) {
        if (rtx_seqno < _highest_rtx_sent)
            continue;

        auto i = _tx_bitmap.find(rtx_seqno);
        if (i == _tx_bitmap.end()) {
            // this means this packet seqno has been acked.
            continue;
        }

        if (_rtx_queue.find(rtx_seqno) != _rtx_queue.end()) {
            continue;
        }

        if (_flow.flow_id() == _debug_flowid ) {
            cout <<  timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " rtx_seqno " << rtx_seqno
                << " _highest_recv_seqno "<< _highest_recv_seqno
                << " recovery_seqno " << _recovery_seqno
                << endl;
        }       

        _stats._sleek_counter++;

        mem_b pkt_size = i->second.pkt_size;
        assert(pkt_size >= _hdr_size); // check we're not seeing NACKed RTS packets.
        auto seqno = i->first;
        simtime_picosec send_time = i->second.send_time;
        _tx_bitmap.erase(i);
        assert(_tx_bitmap.find(seqno) == _tx_bitmap.end()); // xxx remove when working

        _in_flight -= pkt_size;

        // _send_times.erase(send_time);
        delFromSendTimes(send_time, rtx_seqno);
        _highest_rtx_sent = seqno+1;
        queueForRtx(seqno, pkt_size);

        if (send_time == _rto_send_time)
        {
            if(_flow.flow_id() == _debug_flowid ){
                cout <<  timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " rtx_seqno " << rtx_seqno
                    << " send_time "<< timeAsUs(send_time)
                    << " _rto_send_time " << timeAsUs(_rto_send_time)
                    << " recalculateRTO"
                    << endl;
            }     
            recalculateRTO();
        }
        // penalizePath(ev, 1);
    }
    sendIfPermitted();
}

void UecSrc::processNack(const UecNackPacket& pkt) {
    _nic.logReceivedCtrl(pkt.size());
    _stats.nacks_received++;

    // auto pullno = pkt.pullno();
    // handlePull(pullno);

    auto nacked_seqno = pkt.ref_ack();
    if (_debug_src) {
        cout << _flow.str() << " " << _nodename << " processNack nacked: " << nacked_seqno << " flow " << _flow.str()
             << endl;
    }

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
        //abort();
        // this can happen when the NACK arrives later than a cumulative ACK covering the NACKed
        // packet. 
        return;
    }

    mem_b pkt_size = i->second.pkt_size;

    assert(pkt_size >= _hdr_size);  // check we're not seeing NACKed RTS packets.
    if (pkt_size == _hdr_size) {
        _stats.rts_nacks++;
    }

    auto seqno = i->first;
    simtime_picosec send_time = i->second.send_time;
    simtime_picosec raw_rtt = eventlist().now() - send_time;

    if (update_base_rtt_on_nack) {
        update_base_rtt(raw_rtt);
    }
    
    if(raw_rtt >= _base_rtt) {
        update_delay(raw_rtt, false, true);
    }

    if(_flow.flow_id() == _debug_flowid){
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " ev " << ev 
            << " seqno " << seqno
            << " trimming " << endl;
    }
    if (_sender_based_cc){
        (this->*updateCwndOnNack)(ev, pkt_size,pkt.last_hop());
    }

    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " erasing send record, seqno: " << seqno << " flow " << _flow.str()
             << endl;
    _tx_bitmap.erase(i);
    assert(_tx_bitmap.find(seqno) == _tx_bitmap.end());  // xxx remove when working

    _in_flight -= pkt_size;
    //assert(_in_flight >= 0);

    // _send_times.erase(send_time);
    delFromSendTimes(send_time, seqno);

    stopSpeculating();
    queueForRtx(seqno, pkt_size);

    if (send_time == _rto_send_time) {
        recalculateRTO();
    }

    if (pkt.last_hop())
        _mp->processEv(ev, pkt.ecn_echo() ? UecMultipath::PATH_ECN : UecMultipath::PATH_GOOD);
    else
        _mp->processEv(ev, UecMultipath::PATH_NACK);

    sendIfPermitted();
}

void UecSrc::processPull(const UecPullPacket& pkt) {
    _nic.logReceivedCtrl(pkt.size());
    _stats.pulls_received++;

    auto pullno = pkt.pullno();
    if (_debug_src)
        cout << timeAsUs(eventlist().now()) << " flow " << _flow.str() << " " << _nodename << " processPull " << pullno << " flow " << _flow.str() << " SP " << pkt.is_slow_pull() << endl;
    if (_flow.flow_id() == _debug_flowid){
        cout << timeAsUs(eventlist().now())<< " flowid " << _flow.flow_id() << " processPull " << pullno  << " SP " << pkt.is_slow_pull() << endl;
    }
    stopSpeculating();
    handlePull(pullno);
    sendIfPermitted();
}

void UecSrc::doNextEvent() {
    if (_rtx_timeout_pending && eventlist().now() == _rtx_timeout) {
        clearRTO();
        assert(_logger == 0);

        if (_logger)
            _logger->logUec(*this, UecLogger::UEC_TIMEOUT);

        rtxTimerExpired();
    } else if(_highest_sent == 0) {
        if (_debug_src)
            cout << _flow.str() << " " << "Starting flow " << _name << endl;
        startConnection();
    }

    if (_sender_based_cc && _enable_sleek) {
        if (_probe_timer_when != 0 && _probe_timer_when == eventlist().now()){
            if ( _flow.flow_id() == _debug_flowid || _debug_src ) {
                cout << timeAsUs(eventlist().now())<< " doNextEvent probe " <<  _rtx_timeout_pending << " flowid " << _flow.flow_id() << endl;
            }
            sendProbe();
        }
    }
}

bool UecSrc::hasStarted() {
    return _last_event_time.has_value();
}

bool UecSrc::isActivelySending() {
    bool is_sending = false;
    /*
        Cases to consider:
        1. if we are blocked by the NIC, we are active
        2. if we still have packets in the backlog or there are packets in the
          rtx queue, we can be sure that this connection is still being serviced.
        3. if we are done sending, it's still possible that we exactly hit the 
          cwnd/credit limit on the last packet, then we need to check if we are
          blocked by CC. 
        4. if there nothing to be sent, but the connection is not done yet,
          we must have a timeout running. If that is not the case, something
          is wrong (I think), better restart the connection
    */
    if (_send_blocked_on_nic) {
        // 1. 
        is_sending = true;
    } else if (!(_backlog == 0 && _rtx_queue.empty())) {
        // 2.
        is_sending = true;
    } else if (!_done_sending) {
        // 3.
        // Nothing to send, but the connection is not fully acked yet.
        // Make sure there is still a timeout around
        assert(_rtx_timeout_pending);
        is_sending = false;
    } else {
        // 4.
        // Nothing to send, everything has been send
        assert(_rtx_timeout_pending==false);
        is_sending = false;
    }
    
    return is_sending;
}

void UecSrc::setFlowsize(uint64_t flow_size_in_bytes) {
    assert(!_msg_tracker.has_value());
    _flow_size = flow_size_in_bytes;
}

void UecSrc::addToBacklog(mem_b size) {
    _backlog += size;
    _flow_size += size;
    if (_done_sending) {
        _done_sending = false;
    }
}

void UecSrc::startConnection() {
    //_cwnd = _maxwnd;
    _credit = _configured_maxwnd;

    if (_debug_src)
        cout << _flow.str() << " " << "startflow " << _flow._name << " CWND " << _cwnd << " at "
             << timeAsUs(eventlist().now()) << " flow " << _flow.str() << endl;

    if (_last_event_time.has_value() and _last_event_time.value() == eventlist().now()) {
        cout << "Flow " << _name << " flowId " << flowId() << " " << _nodename << " duplicate call to starting at "
            << timeAsUs(eventlist().now()) << endl;
        abort();
    } 

    assert(!hasStarted());
    _last_event_time.emplace(eventlist().now());
    _flow_start_time = eventlist().now();

    /* cout << "Flow " << _name << " flowId " << flowId() << " " << _nodename << " starting at "
         << timeAsUs(eventlist().now()) << endl; */


    if (_flow_logger) {
        _flow_logger->logEvent(_flow, *this, FlowEventLogger::START, _flow_size, 0);
    }

    clearRTO();
    _in_flight = 0;
    _pull_target = INIT_PULL;
    _pull = INIT_PULL;
    _last_rts = 0;
    // backlog is total amount of data we expect to send, including headers
    if (!_msg_tracker.has_value()) {
        _backlog = ceil(((double)_flow_size) / _mss) * _hdr_size + _flow_size;
    } else {
        // In this case, _backlog will be populated directly from the PDC
    }

    _rtx_backlog = 0;
    _send_blocked_on_nic = false;

    while (_send_blocked_on_nic == false && isSendPermitted()) {
        if (_debug_src) {
            cout << _flow.str() << " " << "requestSending 0 "<< endl;
        }

        const Route *route = _nic.requestSending(*this);
        if (_flow.flow_id() == _debug_flowid){
            cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " requestSending " << _nic.activeSources()
                << endl;
        }
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
        }
    }
    // No packet might be sent here, this can happen e.g., with outcast workloads.
}

bool UecSrc::isSendPermitted() {
    if (_rtx_queue.empty() && _backlog == 0) {
        return false;
    }

    if (_receiver_based_cc && !can_send_RCCC()) {
        // can send if we have *any* credit, but we don't                                                                                                         
        return false;
    }

    mem_b next_packet_size = getNextPacketSize();        
    if (_sender_based_cc && !can_send_NSCC(next_packet_size)) {
        return false;
    }

    return true;
}

void UecSrc::continueConnection() {
    if (_debug_src)
        cout << "Flow " << _name << " flowId " << flowId() << " " << _nodename << " continue at "
            << timeAsUs(eventlist().now()) << endl;

    assert(_msg_tracker.has_value());
    assert(hasStarted());
    assert(_backlog > 0);
    assert(_rtx_backlog == 0);
    assert(_send_blocked_on_nic == false);

    _last_event_time.emplace(eventlist().now());

    if (isSendPermitted()) {
        uint32_t pkts_sent = 0;
        while (_send_blocked_on_nic == false && isSendPermitted()) {
            if (_debug_src) {
                cout << _flow.str() << " " << "requestSending 0 "<< endl;
            }

            const Route *route = _nic.requestSending(*this);
            if (_flow.flow_id() == _debug_flowid){
                cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " requestSending " << _nic.activeSources()
                    << endl;
            }
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
            }

            pkts_sent += 1;
        }
        assert(pkts_sent > 0);
    } else {
        // We are blocked by CC, make sure that there is a timeout in place
        assert(_rtx_timeout_pending);
    }
}

mem_b UecSrc::credit() const {
    return _credit;
}

void UecSrc::spendCredit(mem_b pktsize) {
    if (_receiver_based_cc){
        assert(_credit > 0);
        _credit -= pktsize;
    }
}

void UecSrc::stopSpeculating() {
    // this doesn't really do a lot, except prevent us retransmitting
    // on an RTO before we've heard back from the receiver
    if (_speculating) {
        _speculating = false;
            if (_credit > 0)
            _credit = 0;

        if (_flow.flow_id() == _debug_flowid){
            cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " stopSpeculating _credit " << _credit << endl;
        }
    }
}

UecBasePacket::pull_quanta UecSrc::computePullTarget() {
    if (!_receiver_based_cc)
        return 0;

    mem_b pull_target = _backlog + _rtx_backlog;
    //mem_b pull_target = _backlog;

    if (_sender_based_cc) {
        if (pull_target > _cwnd + _mtu) {
            pull_target = _cwnd + _mtu;
        }
    }

    if (pull_target > _configured_maxwnd) {
        pull_target = _configured_maxwnd;
    }

    pull_target -= _credit;

    if (_speculating && pull_target < _mtu && _backlog >0)//always request at least an MTU of credit if we have a backlog, regardless of how much credit we have already have. Saves our bacon for short transfers 
        pull_target = _mtu;
        
    if(_flow.flow_id() == _debug_flowid){
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " _credit " << _credit 
            << " pull_target " << _pull_target << endl;
    }

    if (_nic.activeSources()>1)
        pull_target /= _nic.activeSources();

    pull_target += UecBasePacket::unquantize(_pull);

    UecBasePacket::pull_quanta quant_pull_target = UecBasePacket::quantize_ceil(pull_target);

    if (_debug_src) {
        cout << timeAsUs(eventlist().now()) << " " << _flow.str() << " " << " " << nodename()
             << " pull_target: " << UecBasePacket::unquantize(quant_pull_target) << " beforequant "
             << pull_target << " pull " << UecBasePacket::unquantize(_pull) << " diff "
             << UecBasePacket::unquantize(quant_pull_target - _pull) << " credit "
             << _credit
             << " backlog " << _backlog << " rtx_backlog " << _rtx_backlog << " active sources "
             << _nic.activeSources() << " cwnd " << _cwnd << " maxwnd " << _maxwnd
             << endl;
    }
    return quant_pull_target;
}

mem_b UecSrc::getNextPacketSize(){
    if (_rtx_queue.empty()) {
        if(_backlog == 0){
            return 0;
        }
        // This assertion does not hold when we have multiple messages
        // assert(((mem_b)_highest_sent - _stats.rts_pkts_sent) * _mss < _flow_size);
        mem_b full_pkt_size = _mtu;
        if (_backlog < _mtu) {
            full_pkt_size = _backlog;
        }        
        return full_pkt_size;
    } else {
        assert(!_rtx_queue.empty());
        mem_b full_pkt_size = _rtx_queue.begin()->second;
        return full_pkt_size;
    }
}

void UecSrc::sendIfPermitted() {
    // send if the NIC, credit and window allow.           

    if (_receiver_based_cc && credit() <= 0) {
        // can send if we have *any* credit, but we don't                                                                                                         
        return;
    }

    //cout << timeAsUs(eventlist().now()) << " " << nodename() << " FOO " << _cwnd << " " << _in_flight << endl;                                                  
    mem_b next_packet_size = getNextPacketSize();        
    if (_sender_based_cc) {
        if (!can_send_NSCC(next_packet_size)) {
            return;
        }
    }

    if (_rtx_queue.empty()) {
        if (_backlog == 0) {
            return;
        }
    }
    if (_flow.flow_id() == _debug_flowid)
    {
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() <<" sendIfPermitted requestSending _send_blocked_on_nic "<< _send_blocked_on_nic
            << " activesenders " << _nic.activeSources() << endl;
    }
    if (_send_blocked_on_nic) {
        // the NIC already knows we want to send       
        if (_flow.flow_id() == _debug_flowid){
            for(auto it = _nic._active_srcs.begin(); it != _nic._active_srcs.end(); ++it) {
                UecSrc* queued_src = *it; 
                cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() <<" sendIfPermitted block" << queued_src->flow()->flow_id() << " _nic " << _nic._src_id << endl;;
            } 
        }
        return;
    }

    // we can send if the NIC lets us.                                                                                                                            
    if (_debug_src)
        cout << _flow.str() << " " << "requestSending 1\n";
    if (_flow.flow_id() == _debug_flowid)
    {
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() <<" sendIfPermitted requestSending " << endl;
    }
    const Route* route = _nic.requestSending(*this);
    if (route) {
        mem_b sent_bytes = sendPacket(*route);
        if (sent_bytes > 0) {
            _nic.startSending(*this, sent_bytes, route);
            sendIfPermitted();
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
mem_b UecSrc::sendPacket(const Route& route) {
    if (_rtx_queue.empty()) {
        return sendNewPacket(route);
    } else {
        return sendRtxPacket(route);
    }
}

void UecSrc::startRTO(simtime_picosec send_time) {
    if (!_rtx_timeout_pending) {
        // timer is not running - start it
        _rtx_timeout_pending = true;
        _rtx_timeout = send_time + _min_rto;
        _rto_send_time = send_time;

        if (_rtx_timeout < eventlist().now())
            _rtx_timeout = eventlist().now();

        if (_debug_src)
            cout << "Start timer at " << timeAsUs(eventlist().now()) << " source " << _flow.str()
                 << " expires at " << timeAsUs(_rtx_timeout) << " flow " << _flow.str() << endl;

        _rto_timer_handle = eventlist().sourceIsPendingGetHandle(*this, _rtx_timeout);
        if (_rto_timer_handle == eventlist().nullHandle()) {
            // this happens when _rtx_timeout is past the configured simulation end time.
            _rtx_timeout_pending = false;
            if (_debug_src)
                cout << "Cancel timer because too late for flow " << _flow.str() << endl;
        }
    } else {
        // timer is already running
        if (send_time + _min_rto < _rtx_timeout) {
            // RTO needs to expire earlier than it is currently set
            cancelRTO();
            startRTO(send_time);
        }
    }
}

void UecSrc::clearRTO() {
    // clear the state
    _rto_timer_handle = eventlist().nullHandle();
    _rtx_timeout_pending = false;

    if (_debug_src)
        cout << "Clear RTO " << timeAsUs(eventlist().now()) << " would have expired at " << _rtx_timeout << " source " << _flow.str() << endl;
}

void UecSrc::cancelRTO() {
    if (_rtx_timeout_pending) {
        // cancel the timer
        eventlist().cancelPendingSourceByHandle(*this, _rto_timer_handle);
        clearRTO();
    }
}

mem_b UecSrc::sendNewPacket(const Route& route) {
    if (_debug_src)
        cout << timeAsUs(eventlist().now()) << " " << _flow.str() << " " << _nodename
             << " sendNewPacket highest_sent " << _highest_sent << " h*m "
             << _highest_sent * _mss << " backlog " << _backlog << " flow "
             << _flow.str() << endl;
    assert(_backlog > 0);
    // This assertion does not hold when we have multiple messages
    // assert(((mem_b)_highest_sent - _stats.rts_pkts_sent) * _mss < _flow_size);

    mem_b full_pkt_size = 0;
    
    if (_msg_tracker.has_value()) {
        full_pkt_size = _msg_tracker.value()->getNextPacket(_highest_sent);
    } else {
        full_pkt_size = _mtu;
        if (_backlog < _mtu) {
            full_pkt_size = _backlog;
        }
    }
    assert(full_pkt_size <= _mtu);

    // check we're allowed to send according to state machine
    if (_receiver_based_cc)
        assert(credit() > 0);
        
    spendCredit(full_pkt_size);

    _backlog -= full_pkt_size;
    assert(_backlog >= 0);
    _in_flight += full_pkt_size;
    auto ptype = UecDataPacket::DATA_PULL;
    if (_speculating) {
        ptype = UecDataPacket::DATA_SPEC;
    }
    _pull_target = computePullTarget();

    auto* p = UecDataPacket::newpkt(_flow, route, _highest_sent, full_pkt_size, ptype,
                                     _pull_target, _dstaddr);

    uint16_t ev = _mp->nextEntropy(_highest_sent, (uint64_t)_cwnd/_mss);
    p->set_pathid(ev);
    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);

    if (_backlog == 0 || (_receiver_based_cc && _credit <= 0) || ( _sender_based_cc &&  (_in_flight + full_pkt_size) >= _cwnd )) 
        p->set_ar(true);
    
    createSendRecord(_highest_sent, full_pkt_size);
    if (_debug_src)
        cout << timeAsUs(eventlist().now()) << " " << _flow.str() << " sending pkt " << _highest_sent
             << " size " << full_pkt_size << " pull target " << _pull_target << " ack request " << p->ar()
             << " cwnd " << _cwnd << " ev " << ev << " in_flight " << _in_flight << endl;
    if (_flow.flow_id() == _debug_flowid)
    {
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() <<" sending pkt " << _highest_sent
             << " size " << full_pkt_size << " cwnd " << _cwnd << " ev " << ev 
             << " in_flight " << _in_flight << " pull_target " << _pull_target << " pull " << _pull 
             << " ar " << p->ar()
             << endl;
    }
    p->sendOn();
    _highest_sent++;
    _stats.new_pkts_sent++;
    startRTO(eventlist().now());

    assert(full_pkt_size > 0);

    return full_pkt_size;
}

mem_b UecSrc::sendRtxPacket(const Route& route) {
    assert(!_rtx_queue.empty());
    auto seq_no = _rtx_queue.begin()->first;
    mem_b full_pkt_size = _rtx_queue.begin()->second;
    spendCredit(full_pkt_size);

    _rtx_queue.erase(_rtx_queue.begin());
    _rtx_backlog -= full_pkt_size;
    assert(_rtx_backlog >= 0);
    _in_flight += full_pkt_size;
    _pull_target = computePullTarget();
    
    auto* p = UecDataPacket::newpkt(_flow, route, seq_no, full_pkt_size, UecDataPacket::DATA_RTX,
                                     _pull_target, _dstaddr);

    uint16_t ev = _mp->nextEntropy(_highest_sent, (uint64_t)_cwnd/_mss);
    p->set_pathid(ev);
    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);

    createSendRecord(seq_no, full_pkt_size);

    if (_debug_src)
        cout << timeAsUs(eventlist().now()) << " " << _flow.str() << " " << _nodename << " sending rtx pkt " << seq_no
             << " size " << full_pkt_size << " cwnd " << _cwnd
             << " in_flight " << _in_flight << " pull_target " << _pull_target << " pull " << _pull << endl;
    if (_flow.flow_id() == _debug_flowid)
    {
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() <<" sending rtx pkt " << seq_no
             << " size " << full_pkt_size << " cwnd " << _cwnd <<" ev " << ev << " rtx_times " << _rtx_times[seq_no]
             << " in_flight " << _in_flight << " pull_target " << _pull_target << " pull " << _pull << endl;
    }
    p->set_ar(true);
    p->sendOn();
    _stats.rtx_pkts_sent++;
    startRTO(eventlist().now());
    return full_pkt_size;
}

void UecSrc::sendProbe() {
    if (_flow.flow_id() == _debug_flowid) {
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " sendProbe "
             << endl;
    }
    _probe_seqno++;
    auto* p = UecDataPacket::newpkt(_flow, NULL, _probe_seqno, _hdr_size,
                                    UecBasePacket::DATA_PROBE, 0, _dstaddr);
    p->set_dst(_dstaddr);
    uint16_t ev = _mp->nextEntropy(_highest_sent, (uint64_t)_cwnd/_mss);
    p->set_pathid(ev);
    // p->sendOn();
    _nic.sendControlPacket(p, this, NULL);

    _probe_send_time = eventlist().now();
    _probe_timer_when = eventlist().now() + probe_retry_time * _base_rtt;
    _probe_timer_handle = eventlist().sourceIsPendingGetHandle(*this, _probe_timer_when);
}

void UecSrc::sendRTS() {
    if (_last_rts > 0 && eventlist().now() - _last_rts < _network_rtt) {
        // Don't send more than one RTS per RTT, or we can create an
        // incast of RTS.  Once per RTT is enough to restart things if we lost
        // a whole window.
        return;
    }

    if (_msg_tracker.has_value()) {
        _msg_tracker.value()->notifyCtrlSeqno(_highest_sent);
    }

    if (_debug_src)
        cout << timeAsUs(eventlist().now()) << " " << _flow.str() << " " << _nodename << " sendRTS, flow " << _flow.str()
             << " epsn " << _highest_sent << " last RTS " << timeAsUs(_last_rts)
             << " in_flight " << _in_flight << " pull_target " << _pull_target << " pull " << _pull << endl;
    createSendRecord(_highest_sent, _hdr_size);
    auto* p =
        UecRtsPacket::newpkt(_flow, NULL, _highest_sent, _pull_target, _dstaddr);

    uint16_t ev = _mp->nextEntropy(_highest_sent, (uint64_t)_cwnd/_mss);
    p->set_pathid(ev);

    // p->sendOn();
    _nic.sendControlPacket(p, this, NULL);

    _highest_sent++;
    _stats.rts_pkts_sent++;
    _last_rts = eventlist().now();
    startRTO(eventlist().now());
}

void UecSrc::createSendRecord(UecBasePacket::seq_t seqno, mem_b full_pkt_size) {
    if (_debug_src)
        cout << _flow.str() << " " << _nodename << " createSendRecord seqno: " << seqno << " size " << full_pkt_size
             << endl;

    assert(_tx_bitmap.find(seqno) == _tx_bitmap.end());

    _tx_bitmap.emplace(seqno, sendRecord(full_pkt_size, eventlist().now()));
    _send_times.emplace(eventlist().now(), seqno);

    if (_rtx_times.find(seqno) == _rtx_times.end()) {
        _rtx_times.emplace(seqno, 0);
    } else {
        _rtx_times[seqno] += 1;
    }
}

void UecSrc::queueForRtx(UecBasePacket::seq_t seqno, mem_b pkt_size) {
    assert(_rtx_queue.find(seqno) == _rtx_queue.end());
    _rtx_queue.emplace(seqno, pkt_size);
    _rtx_backlog += pkt_size;
    if (!_speculating || !_receiver_based_cc)
        sendIfPermitted();
}

void UecSrc::timeToSend(const Route& route) {
    if (_debug_src)
        cout << "timeToSend"
             << " flow " << _flow.str() << " at " << timeAsUs(eventlist().now()) << endl;

    // time_to_send is called back from the UecNIC when it's time for
    // this src to send.  To get called back, the src must have
    // previously told the NIC it is ready to send by calling
    // UecNIC::requestSending()

    // before returning, UecSrc needs to call either
    // UecNIC::startSending or UecNIC::cantSend from this function
    // to update the NIC as to what happened, so they stay in sync.
    // This also true when the flow is complete, let's make sure
    // we are in sync either way.
    _send_blocked_on_nic = false;

    if (_backlog == 0 && _rtx_queue.empty()) {
        _nic.cantSend(*this);
        return;
    }

    mem_b next_packet_size = getNextPacketSize();
    if (_sender_based_cc && !can_send_NSCC(next_packet_size)) {
        if (_debug_src)
            cout << _flow.str() << " " << _node_num << " cantSend, limited by sender CWND " << _cwnd << " _in_flight "
                    << _in_flight << "\n";

        _nic.cantSend(*this);
        return;
    }

    if (_flow.flow_id() == _debug_flowid ){
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() << " _receiver_based_cc " << _receiver_based_cc << " credit " << credit()
            << endl;
    }
    // do we have enough credit if we're using receiver CC?
    if (_receiver_based_cc && !can_send_RCCC()) {
        if (_debug_src)
            cout << "cantSend"
                << " flow " << _flow.str() << endl;
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
        _nic.startSending(*this, bytes_sent, NULL);
    } else {
        _nic.cantSend(*this);
        return;
    }
    
    if (!isSendPermitted()) {
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

void UecSrc::recalculateRTO() {
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

void UecSrc::rtxTimerExpired() {
    assert(eventlist().now() == _rtx_timeout);
    clearRTO();

    auto first_entry = _send_times.begin();
    assert(first_entry != _send_times.end());
    auto seqno = first_entry->second;

    auto send_record = _tx_bitmap.find(seqno);
    assert(send_record != _tx_bitmap.end());
    mem_b pkt_size = send_record->second.pkt_size;

    // Trigger multipathing feedback for timeout. Unless we save EVs on the sender per packet, we will 
    // not be able to recover the original timed-out ev.
    _mp->processEv(UecMultipath::UNKNOWN_EV, UecMultipath::PATH_TIMEOUT);

    // update flightsize?

    //_send_times.erase(first_entry);
    delFromSendTimes(send_record->second.send_time,seqno);

    //cout << _nodename << " rtx timer expired for seqno " << seqno << " flow " << _flow.str() << " packet sent at " << timeAsUs(send_record->second.send_time) << " now time is " << timeAsUs(eventlist().now()) << endl;

    if (_debug_src)
        cout << _nodename << " rtx timer expired for seqno " << seqno << " flow " << _flow.str() << " packet sent at " << timeAsUs(send_record->second.send_time) << " now time is " << timeAsUs(eventlist().now()) << endl;
    
    if (_flow.flow_id() == UecSrc::_debug_flowid ) {
        cout << timeAsUs(eventlist().now()) << " flowid " << _flow.flow_id() 
            <<" rtx timer expired for seqno " << seqno << " packet sent at " 
            << timeAsUs(send_record->second.send_time) << " now time is " << timeAsUs(eventlist().now()) 
            << " _loss_recovery_mode " << _loss_recovery_mode
            << endl;
    }

    //Yanfang: this is a hack, we remove timestamp for these seqno, 
    //I would expect that that the fast loss recovery will retransmit this packet, when the send_times record the sending timestamp for this packet
    if (_sender_based_cc && _enable_sleek) {
        if (_loss_recovery_mode) {
            if (_rtx_times[seqno] < 1) {
                recalculateRTO();
            } else {
                _highest_rtx_sent = seqno;
            }
            return; 
        }
    }

    _tx_bitmap.erase(send_record);
    recalculateRTO();

    if (_sender_based_cc)
        mark_packet_for_retransmission(seqno, pkt_size);

    if (!_rtx_queue.empty()) {
        // there's already a queue, so clearly we shouldn't just
        // resend right now.  But send an RTS (no more than once per
        // RTT) to cover the case where the receiver doesn't know
        // we're waiting.
        stopSpeculating();  

        queueForRtx(seqno, pkt_size);

        if (_receiver_based_cc) {
            if (_debug_src)
                cout << "sendRTS 1"
                     << " flow " << _flow.str() << endl;
            sendRTS();
        }
        return;
    }

    // there's no queue, so maybe we could just resend now?
    queueForRtx(seqno, pkt_size);

    if (_sender_based_cc) {
        if (_cwnd < pkt_size + _in_flight) {
            // window won't allow us to send yet.
            if (_debug_src)
                cout << "sendRTS 3"
                     << " flow " << _flow.str() << endl;
            sendRTS();  
            return;
        }
    }

    if (_receiver_based_cc && _credit <= 0) {
        // we don't have any credit to send.  Send an RTS (no more than once per RTT)
        // to cover the case where the receiver doesn't know to send
        // us credit
        if (_debug_src)
            cout << "sendRTS 2"
                 << " flow " << _flow.str() << endl;

        sendRTS();
        return;
    }

    // we've got enough pulled credit or window already to send this, so see if the NIC
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

void UecSrc::activate() {
    cout << _flow.str() << " activate" << endl;
    startConnection();
}

void UecSrc::setEndTrigger(Trigger& end_trigger) {
    _end_trigger = &end_trigger;
};

////////////////////////////////////////////////////////////////
//  UEC SINK PORT
////////////////////////////////////////////////////////////////
UecSinkPort::UecSinkPort(UecSink& sink, uint32_t port_num)
    : _sink(sink), _port_num(port_num) {
}

void UecSinkPort::setRoute(const Route& route) {
    _route = &route;
}

void UecSinkPort::receivePacket(Packet& pkt) {
    _sink.receivePacket(pkt, _port_num);
}

const string& UecSinkPort::nodename() {
    return _sink.nodename();
}

////////////////////////////////////////////////////////////////
//  UEC SINK
////////////////////////////////////////////////////////////////

UecSink::UecSink(TrafficLogger* trafficLogger, UecPullPacer* pullPacer, UecNIC& nic, uint32_t no_of_ports)
    : DataReceiver("uecSink"),
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
      _recvd_bytes(0),
      _rcv_cwnd_pen(255),
      _end_trigger(NULL),
      _epsn_rx_bitmap(0),
      _out_of_order_count(0),
      _ack_request(false),
      _entropy(0)  {
    
    _nodename = "uecSink";  // TBD: would be nice at add nodenum to nodename
    _no_of_ports = no_of_ports;
    _ports.resize(no_of_ports);
    for (uint32_t p = 0; p < _no_of_ports; p++) {
        _ports[p] = new UecSinkPort(*this, p);
    }
        
    _stats = {0, 0, 0, 0, 0, 0, 0, 0};
    _in_pull = false;
    _in_slow_pull = false;

    _pcie = NULL;
    _receiver_cc = NULL;
}

UecSink::UecSink(TrafficLogger* trafficLogger,
                   linkspeed_bps linkSpeed,
                   double rate_modifier,
                   uint16_t mtu,
                   EventList& eventList,
                   UecNIC& nic,
                   uint32_t no_of_ports)
    : DataReceiver("uecSink"),
      _nic(nic),
      _flow(trafficLogger),
      _expected_epsn(0),
      _high_epsn(0),
      _retx_backlog(0),
      _latest_pull(INIT_PULL),
      _highest_pull_target(INIT_PULL),
      _received_bytes(0),
      _accepted_bytes(0),
      _recvd_bytes(0),
      _rcv_cwnd_pen(255),
      _end_trigger(NULL),
      _epsn_rx_bitmap(0),
      _out_of_order_count(0),
      _ack_request(false),
      _entropy(0) {
    
    if (UecSrc::_receiver_based_cc)
        _pullPacer = new UecPullPacer(linkSpeed, rate_modifier, mtu, eventList, no_of_ports);
    else    
        _pullPacer = NULL;

    _no_of_ports = no_of_ports;
    _ports.resize(no_of_ports);
    for (uint32_t p = 0; p < _no_of_ports; p++) {
        _ports[p] = new UecSinkPort(*this, p);
    }
    _stats = {0, 0, 0, 0, 0,0,0};
    _in_pull = false;
    _in_slow_pull = false;

    _pcie = NULL;
    _receiver_cc = NULL;
}

void UecSink::connectPort(uint32_t port_num, UecSrc& src, const Route& route) {
    _src = &src;
    _ports[port_num]->setRoute(route);
}

void UecSink::handlePullTarget(UecBasePacket::seq_t pt) {
    if (!UecSrc::_receiver_based_cc)
        return;

    if (_src->debug())
        cout << " UecSink " << _nodename << " src " << _src->nodename() << " handlePullTarget pt " << pt << " highest_pt " << _highest_pull_target << endl;
    if (_src->flow()->flow_id() == UecSrc::_debug_flowid ){
        cout << timeAsUs(_src->eventlist().now()) << " flowid " << _src->flow()->flow_id()  << " handlePullTarget pt " << pt << " highest_pt " << _highest_pull_target << endl;

    }
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

void UecSink::processData(UecDataPacket& pkt) {
    bool force_ack = false;
    if (pkt.packet_type() == UecBasePacket::DATA_PROBE){
        UecAckPacket* ack_packet =
            sack(pkt.path_id(), sackBitmapBase(pkt.epsn()), pkt.epsn(), (bool)(pkt.flags() & ECN_CE), pkt.retransmitted());
        ack_packet->set_probe_ack(true);
        _nic.sendControlPacket(ack_packet, NULL, this);   
        return;     
    }
    //PCIeModel processing

    if (_model_pcie){
        if (!_pcie->addBacklog(pkt.size())){
            //will drop this packet!
            cout << "PCIE trim" << endl;
            //should trim this packet.
            pkt.strip_payload();
            processTrimmed(pkt);
            return;
        }
    }

    //ensure we never overflow receive bitmap.
    if (pkt.epsn() > _expected_epsn + uecMaxInFlightPkts * UecSrc::_mtu){
        abort();
    }

    if (_src->debug())
        cout << " UecSink " << _nodename << " src " << _src->nodename()
             << " processData: " << pkt.epsn() << " time " << timeAsNs(getSrc()->eventlist().now())
             << " when expected epsn is " << _expected_epsn << " size " << pkt.size() << " ooo count " << _out_of_order_count
             << " flow " << _src->flow()->str() << endl;

    _accepted_bytes += pkt.size();

    if (_src->msg_tracker().has_value()) {
        _src->msg_tracker().value()->addRecvd(pkt.epsn());
    }

    handlePullTarget(pkt.pull_target());

    if (_src->flow()->flow_id() == UecSrc::_debug_flowid)
    {
        cout << timeAsUs(_src->eventlist().now()) << " flowid " << _src->flow()->flow_id()
             << " recv " << pkt.epsn() << endl;
    }
    if (pkt.epsn() > _high_epsn) {
        // highest_received is used to bound the sack bitmap. This is a 64 bit number in simulation,
        // never wraps. In practice need to handle sequence number wrapping.
        _high_epsn = pkt.epsn();
    }

    // should send an ACK; if incoming packet is ECN marked, the ACK will be sent straight away;
    // otherwise ack will be delayed until we have cumulated enough bytes / packets.
    bool ecn = (bool)(pkt.flags() & ECN_CE);

    if (ecn){
        _stats.ecn_received++;
        _stats.ecn_bytes_received += pkt.size();

        if (_oversubscribed_cc)
            _receiver_cc->ecn_received(pkt.size());
    }

    if (pkt.epsn() < _expected_epsn || _epsn_rx_bitmap[pkt.epsn()]) {
        if (UecSrc::_debug)
            cout << _nodename << " src " << _src->nodename() << " duplicate psn " << pkt.epsn()
                 << endl;

        _stats.duplicates++;
        _nic.logReceivedData(pkt.size(), 0);

        // if (_src->flow()->flow_id() == UecSrc::_debug_flowid){   
            cout << timeAsUs(_src->eventlist().now()) << " flowid " << _src->flow()->flow_id()  
                << " Spurious " << pkt.epsn() <<endl;
        // }
        // sender is confused and sending us duplicates: ACK straight away.
        // this code is different from the proposed hardware implementation, as it keeps track of
        // the ACK state of OOO packets.
        UecAckPacket* ack_packet =
            sack(pkt.path_id(), ecn ? pkt.epsn() : sackBitmapBase(pkt.epsn()), pkt.epsn(), ecn, pkt.retransmitted());
        _nic.sendControlPacket(ack_packet, NULL, this);

        _accepted_bytes = 0;  // careful about this one.
        return;
    }

    if (_received_bytes == 0) {
        force_ack = true;
    }
    // packet is in window, count the bytes we got.
    // should only count for non RTS and non trimmed packets.
    _received_bytes += pkt.size() - UecAckPacket::ACKSIZE;
    _nic.logReceivedData(pkt.size(), pkt.size());

    _recvd_bytes += pkt.size();
    if (_src->debug()) {
        cout << _nodename << " recvd_bytes: " << _recvd_bytes << endl;
    }

    if (_src->debug() && _received_bytes >= _src->flowsize())
        cout << _nodename << " received " << _received_bytes << " at "
             << timeAsUs(EventList::getTheEventList().now()) << endl;
    assert(_received_bytes <= _src->flowsize());

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
            cout << " UecSink " << _nodename << " src " << _src->nodename()
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
    if (_src->flow()->flow_id() == UecSrc::_debug_flowid) {
        cout << timeAsUs(_src->eventlist().now()) << " flowid " << _src->flow()->flow_id()
             << " checkSack: " << pkt.epsn() << " ooo_count "
             << _out_of_order_count << " ecn " << ecn << " shouldSack " << shouldSack()
             << " forceack " << force_ack << endl;
    }
    if (ecn || shouldSack() || force_ack) {
        UecAckPacket* ack_packet =
            sack(pkt.path_id(), (ecn || pkt.ar()) ? pkt.epsn() : sackBitmapBase(pkt.epsn()), pkt.epsn(), ecn, pkt.retransmitted());

        if (_src->debug()) {
            cout << " UecSink " << _nodename << " src " << _src->nodename()
                 << " sendAckNow: " << _expected_epsn << " ref_epsn " << pkt.epsn()
                 << " ooo_count " << _out_of_order_count
                 << " recvd_bytes " << _recvd_bytes << " flow " << _src->flow()->str() 
                 << " ecn " << ecn << " shouldSack " << shouldSack() << " forceack " << force_ack << endl;
        }

        if (_src->flow()->flow_id() == UecSrc::_debug_flowid)
        {
            cout << timeAsUs(_src->eventlist().now()) << " flowid " << _src->flow()->flow_id()
                 << " sendAckNow: " << _expected_epsn << " ref_epsn " << pkt.epsn()
                 << " ooo_count " << _out_of_order_count
                 << " recvd_bytes " << _recvd_bytes << " flow " << _src->flow()->str() 
                 << " ecn " << ecn << " shouldSack " << shouldSack() << " forceack " << force_ack << endl;
        }
        _accepted_bytes = 0;

        // ack_packet->sendOn();
        _nic.sendControlPacket(ack_packet, NULL, this);
    }
}

void UecSink::processTrimmed(const UecDataPacket& pkt) {
    _nic.logReceivedTrim(pkt.size());

    _stats.trimmed++;
    
    /*Currently, the trimming support in htsim does not change (or support) DSCP code points. However, upon trim, it
    does save the TTL of the packet at the trim point. To detect last hop trims, we compare the received TTL to the 
    one saved in the trim packet. The "-2” part comes from the fact that every hop in htsim is composed of a queue 
    (which models bandwidth + buffer) and a pipe (which models propagation latency).*/
    bool is_last_hop = (pkt.nexthop() - pkt.trim_hop() - 2) == 0;
    
    if (_oversubscribed_cc){
        _receiver_cc->trimmed_received(is_last_hop);
    }

    if (pkt.epsn() < _expected_epsn || _epsn_rx_bitmap[pkt.epsn()]) {
        if (_src->debug())
            cout << " UecSink processTrimmed got a packet we already have: " << pkt.epsn()
                 << " time " << timeAsNs(getSrc()->eventlist().now()) << " flow"
                 << _src->flow()->str() << endl;

        UecAckPacket* ack_packet = sack(pkt.path_id(), sackBitmapBase(pkt.epsn()), pkt.epsn(), false, pkt.retransmitted());
        //ack_packet->sendOn();
        _nic.sendControlPacket(ack_packet, NULL, this);
        return;
    }

    if (_src->debug())
        cout << " UecSink processTrimmed packet " << pkt.epsn() << " time "
             << timeAsNs(getSrc()->eventlist().now()) << " flow" << _src->flow()->str() << endl;

    handlePullTarget(pkt.pull_target());

    if (_src->debug())
        cout << "RTX_backlog++ trim: " << pkt.epsn() << " from " << getSrc()->nodename()
             << " rtx_backlog " << rtx_backlog() << " at " << timeAsUs(getSrc()->eventlist().now())
             << " flow " << _src->flow()->str() << endl;

    UecNackPacket* nack_packet = nack(pkt.path_id(), pkt.epsn(), is_last_hop, (bool)(pkt.flags() & ECN_CE));

    // nack_packet->sendOn();
    _nic.sendControlPacket(nack_packet, NULL, this);

    if (UecSrc::_receiver_based_cc && !_in_pull) {
        // source is now retransmitting, must add it to the list.
        if (_src->debug())
            cout << "PullPacer RequestPull: " << _src->flow()->str() << " at "
                 << timeAsUs(getSrc()->eventlist().now()) << endl;

        _in_pull = true;
        _pullPacer->requestPull(this);
    }
}

void UecSink::processRts(const UecRtsPacket& pkt) {
    assert(pkt.ar());
    if (_src->debug())
        cout << " UecSink " << _nodename << " src " << _src->nodename()
             << " processRts: " << pkt.epsn() << " time " << timeAsNs(getSrc()->eventlist().now()) << endl;
    
    if (_src->msg_tracker().has_value()) {
        _src->msg_tracker().value()->addRecvd(pkt.epsn());
    }

    handlePullTarget(pkt.pull_target());

    // what happens if this is not an actual retransmit, i.e. the host decides with the ACK that it
    // is

    if (_src->debug())
        cout << "RTX_backlog++ RTS: " << _src->flow()->str() << " rtx_backlog " << rtx_backlog()
             << " at " << timeAsUs(getSrc()->eventlist().now()) << endl;

    if (UecSrc::_receiver_based_cc && !_in_pull) {
        _in_pull = true;
        _pullPacer->requestPull(this);
    }

    bool ecn = (bool)(pkt.flags() & ECN_CE);
    assert(!ecn); // not expecting ECN set on control packets

    if (pkt.epsn() < _expected_epsn || _epsn_rx_bitmap[pkt.epsn()]) {
        if (_src->debug())
            cout << _nodename << " src " << _src->nodename() << " duplicate RTS psn " << pkt.epsn()
                 << endl;

        _stats.duplicates++;

        // sender is confused and sending us duplicates: ACK straight away.
        // this code is different from the proposed hardware implementation, as it keeps track of
        // the ACK state of OOO packets.
        UecAckPacket* ack_packet = sack(pkt.path_id(), sackBitmapBase(pkt.epsn()), pkt.epsn(), ecn, pkt.retransmitted());
        ack_packet->set_is_rts(true);
        _nic.sendControlPacket(ack_packet, NULL, this);

        _accepted_bytes = 0;  // careful about this one.
        return;
    }


    if (pkt.epsn() == _expected_epsn) {
        while (_epsn_rx_bitmap[++_expected_epsn]) {
            // clean OOO state, this will wrap at some point.
            _epsn_rx_bitmap[_expected_epsn] = 0;
            _out_of_order_count--;
        }
        if (_src->debug())
            cout << " UecSink " << _nodename << " src " << _src->nodename()
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

    UecAckPacket* ack_packet =
        sack(pkt.path_id(), (ecn || pkt.ar()) ? pkt.epsn() : sackBitmapBase(pkt.epsn()), pkt.epsn(), ecn, pkt.retransmitted());
    ack_packet->set_is_rts(true);
    if (_src->debug())
        cout << " UecSink " << _nodename << " src " << _src->nodename()
             << " send ack now: " << _expected_epsn << " ooo count " << _out_of_order_count
             << " flow " << _src->flow()->str() << endl;

    _nic.sendControlPacket(ack_packet, NULL, this);
}

void UecSink::receivePacket(Packet& pkt, uint32_t port_num) {
    _stats.received++;
    _stats.bytes_received += pkt.size();  // should this include just the payload?

    if (_oversubscribed_cc)
        _receiver_cc->data_received(pkt.size());

    switch (pkt.type()) {
        case UECDATA:
            if (pkt.header_only()){
                processTrimmed((const UecDataPacket&)pkt);
                // cout << "UecSink::receivePacket receive trimmed packet\n";
                // assert(false);
            }else
                processData((UecDataPacket&)pkt);

            pkt.free();
            break;
        case UECRTS:
            processRts((const UecRtsPacket&)pkt);
            pkt.free();
            break;
        default:
            cout << "UecSink::receivePacket receive weird packets\n";
            abort();
    }
}

uint16_t UecSink::nextEntropy() {
    int spraymask = (1 << TGT_EV_SIZE) - 1;
    int fixedmask = ~spraymask;
    int idx = _entropy & spraymask;
    int fixed_entropy = _entropy & fixedmask;
    int ev = ++idx & spraymask;

    _entropy = fixed_entropy | ev;  // save for next pkt

    return ev;
}

UecPullPacket* UecSink::pull(UecBasePacket::pull_quanta& extra_credit) {
    // called when pull pacer is ready to give another credit to this connection.
    // TODO: need to credit in multiple of MTU here.

    if (_retx_backlog > 0) {
        if (_retx_backlog > UecSink::_credit_per_pull)
            _retx_backlog -= UecSink::_credit_per_pull;
        else
            _retx_backlog = 0;

        if (UecSrc::_debug)
            cout << "RTX_backlog--: " << getSrc()->nodename() << " rtx_backlog " << rtx_backlog()
                 << " at " << timeAsUs(getSrc()->eventlist().now()) << " flow "
                 << _src->flow()->str() << endl;
    }

    if (extra_credit == 0) {
        // only send as much credit as the sender asked for
        auto prev_pull = _latest_pull;
	_latest_pull += UecSink::_credit_per_pull;
        if (_latest_pull > _highest_pull_target) {
            // don't go above pull_target, but also don't go backwards
            _latest_pull = max(_highest_pull_target, prev_pull);
	}
        extra_credit = _latest_pull - prev_pull;
    } else {
        // it's a slow pull, ignore pull target and just grant what we're told
        _latest_pull += extra_credit;
    }

    UecPullPacket* pkt = NULL;
    pkt = UecPullPacket::newpkt(_flow, NULL, _latest_pull, false, _srcaddr);
    pkt->set_pathid(nextEntropy());

    return pkt;
}

bool UecSink::shouldSack() {
    return _accepted_bytes >= _bytes_unacked_threshold;
}

UecBasePacket::seq_t UecSink::sackBitmapBase(UecBasePacket::seq_t epsn) {
    return max((int64_t)epsn - 63, (int64_t)(_expected_epsn + 1));
}

UecBasePacket::seq_t UecSink::sackBitmapBaseIdeal() {
    uint8_t lowest_value = UINT8_MAX;
    UecBasePacket::seq_t lowest_position = 0;

    // find the lowest non-zero value in the sack bitmap; that is the candidate for the base, since
    // it is the oldest packet that we are yet to sack. on sack bitmap construction that covers a
    // given seqno, the value is incremented.
    for (UecBasePacket::seq_t crt = _expected_epsn; crt <= _high_epsn; crt++)
        if (_epsn_rx_bitmap[crt] && _epsn_rx_bitmap[crt] < lowest_value) {
            lowest_value = _epsn_rx_bitmap[crt];
            lowest_position = crt;
        }

    if (lowest_position + 64 > _high_epsn)
        lowest_position = _high_epsn - 64;

    if (lowest_position <= _expected_epsn)
        lowest_position = _expected_epsn + 1;

    return lowest_position;
}

uint64_t UecSink::buildSackBitmap(UecBasePacket::seq_t ref_epsn) {
    // take the next 64 entries from ref_epsn and create a SACK bitmap with them
    if (_src->debug())
        cout << " UecSink: building sack for ref_epsn " << ref_epsn << endl;
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

UecAckPacket* UecSink::sack(uint16_t path_id, UecBasePacket::seq_t seqno, UecBasePacket::seq_t acked_psn, bool ce, bool rtx_echo) {
    uint64_t bitmap = buildSackBitmap(seqno);
    UecAckPacket* pkt =
        UecAckPacket::newpkt(_flow, NULL, _expected_epsn, seqno, acked_psn, path_id, ce, _recvd_bytes,_rcv_cwnd_pen,_srcaddr);
    pkt->set_bitmap(bitmap);
    pkt->set_ooo(_out_of_order_count);
    pkt->set_rtx_echo(rtx_echo);
    pkt->set_probe_ack(false);
    return pkt;
}

UecNackPacket* UecSink::nack(uint16_t path_id, UecBasePacket::seq_t seqno,bool last_hop, bool ecn_echo) {
    UecNackPacket* pkt = UecNackPacket::newpkt(_flow, NULL, seqno, path_id,  _recvd_bytes,_rcv_cwnd_pen,_srcaddr);
    pkt->set_last_hop(last_hop);
    pkt->set_ecn_echo(ecn_echo);
    return pkt;
}

void UecSink::setEndTrigger(Trigger& end_trigger) {
    _end_trigger = &end_trigger;
};


/*static unsigned pktByteTimes(unsigned size) {
    // IPG (96 bit times) + preamble + SFD + ether header + FCS = 38B
    return max(size, 46u) + 38;
}*/

uint32_t UecSink::reorder_buffer_size() {
    uint32_t count = 0;
    // it's not very efficient to count each time, but if we only do
    // this occasionally when the sink logger runs, it should be OK.
    for (uint32_t i = 0; i < uecMaxInFlightPkts; i++) {
        if (_epsn_rx_bitmap[i])
            count++;
    }
    return count;
}

////////////////////////////////////////////////////////////////
//  UEC PACER
////////////////////////////////////////////////////////////////

// pull rate modifier should generally be something like 0.99 so we pull at just less than line rate
UecPullPacer::UecPullPacer(linkspeed_bps linkSpeed,
                             double pull_rate_modifier,
                             uint16_t bytes_credit_per_pull,
                             EventList& eventList,
                             uint32_t no_of_ports)
    : EventSource(eventList, "uecPull"),
      _time_per_quanta((8 * UEC_PULL_QUANTUM * 1e12 / (linkSpeed * no_of_ports))/pull_rate_modifier)
{
    _active = false;
    _actual_time_per_quanta = _time_per_quanta;
    _bytes_credit_per_pull = bytes_credit_per_pull;
    _linkspeed = linkSpeed;
    _rates[PCIE] = 1;
    _rates[OVERSUBSCRIBED_CC] = 1;
}

void UecPullPacer::doNextEvent() {
    if (_active_senders.empty() && _idle_senders.empty()) {
        _active = false;
        return;
    }

    UecSink* sink = NULL;
    UecPullPacket* pullPkt;
    UecBasePacket::pull_quanta extra_credit = 0;

    if (!_active_senders.empty()) {
        sink = _active_senders.front();

        assert(sink->inPullQueue());

        _active_senders.pop_front();
        pullPkt = sink->pull(extra_credit);

        // TODO if more pulls are needed, enqueue again
        if (UecSrc::_debug)
            cout << "PullPacer: Active: " << sink->getSrc()->flow()->str() << " backlog "
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

        if (UecSrc::_debug)
            cout << "PullPacer: Idle: " << sink->getSrc()->flow()->str() << " at "
                 << timeAsUs(eventlist().now()) << " backlog " << sink->backlog() << " "
                 << sink->slowCredit() << " max "
                 << UecBasePacket::quantize_floor(sink->getConfiguredMaxWnd()) << endl;
        extra_credit = UecSink::_credit_per_pull;
        pullPkt = sink->pull(extra_credit);
        pullPkt->set_slow_pull(true);

        if (sink->backlog() == 0 &&
            sink->slowCredit() < UecBasePacket::quantize_floor(sink->getConfiguredMaxWnd())) {
            // only send upto 1BDP worth of speculative credit.
            // backlog will be negative once this source starts receiving speculative credit.
            _idle_senders.push_back(sink);
        } else {
            sink->removeFromSlowPullQueue();
        }
    }
    

    pullPkt->flow().logTraffic(*pullPkt, *this, TrafficLogger::PKT_SEND);

    // pullPkt->sendOn();
    sink->getNIC()->sendControlPacket(pullPkt, NULL, sink);
    _active = true;

    if (extra_credit == 0) {
        // we need some time between pulls, even if we're not sending more credit;
        extra_credit = 1024 >> UEC_PULL_SHIFT;
    }
    simtime_picosec pkt_time = _actual_time_per_quanta * extra_credit;
    assert(pkt_time > 0);
    eventlist().sourceIsPendingRel(*this, pkt_time);
}

void UecPullPacer::updatePullRate(reason r, double relative_rate){
    _rates[r] = relative_rate;

    _actual_time_per_quanta = _time_per_quanta / min(_rates[PCIE],_rates[OVERSUBSCRIBED_CC]);

    if (UecSrc::_debug)
        cout << "Interpacket delay " << timeAsUs(_actual_time_per_quanta * UecSink::_credit_per_pull) << endl;
}

bool UecPullPacer::isActive(UecSink* sink) {
    for (auto i = _active_senders.begin(); i != _active_senders.end(); i++) {
        if (*i == sink)
            return true;
    }
    return false;
}

bool UecPullPacer::isIdle(UecSink* sink) {
    for (auto i = _idle_senders.begin(); i != _idle_senders.end(); i++) {
        if (*i == sink)
            return true;
    }
    return false;
}

void UecPullPacer::requestPull(UecSink* sink) {
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


