// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#include <math.h>
#include <iostream>
#include <algorithm>
#include "dcqcn.h"
#include "queue.h"
#include <stdio.h>
#include "switch.h"
#include "trigger.h"
using namespace std;

////////////////////////////////////////////////////////////////
//  DCQCN SOURCE
////////////////////////////////////////////////////////////////

/* keep track of RTOs.  Generally, we shouldn't see RTOs if
   return-to-sender is enabled.  Otherwise we'll see them with very
   large incasts. */
simtime_picosec DCQCNSink::_cnp_interval = timeFromUs(50.0);
simtime_picosec DCQCNSrc::_cc_update_period = timeFromUs(55.0);

double DCQCNSrc::_alpha = 1;
double DCQCNSrc::_g = .00390625; // g = 1/256
uint64_t DCQCNSrc::_B = 10000000; // number of bytes to go until we fire the byte counter

uint32_t DCQCNSrc::_F = 5;
linkspeed_bps DCQCNSrc::_RAI = 0;
linkspeed_bps DCQCNSrc::_RHAI = 0;

DCQCNSrc::DCQCNSrc(RoceLogger* logger, TrafficLogger* pktlogger, EventList &eventlist, linkspeed_bps rate)
    : RoceSrc(logger,pktlogger,eventlist,rate)
{
    _cnps_received = 0;

    //linkspeed
    _link = rate;
    //current transmit rate
    _RC = rate;
    //target transmit rate
    _RT = rate;

    _RAI = rate / 20;
    _RHAI = rate / 10;

    _last_cc_update = 0;
    _last_alpha_update = 0;

    _T = 0;
    _BC = 0;
    _byte_counter = 0;
    _old_highest_sent = 0;
}

void DCQCNSrc::processCNP(const CNPPacket& cnp){
    _RT = _RC;
    _RC = _RC * (1-_alpha/2);
    _alpha = (1-_g)*_alpha + _g;

    //_ai_state = increase_state::fast_recovery;

    _T = 0;
    _BC = 0;
    _byte_counter = 0;
    _old_highest_sent = _highest_sent;

    cout << "At " << timeAsUs(eventlist().now()) << " " << _RC << " CNP received, reduced rate; target rate is " << _RT << " node " << nodename() << " alpha is " << _alpha;

    _pacing_rate = _RC;
    update_spacing();
    cout << " packet spacing is " << timeAsUs(_packet_spacing) << endl;

    _last_cc_update = eventlist().now();
    _last_alpha_update = eventlist().now();

    eventlist().sourceIsPendingRel(*this, _cc_update_period);
}

void DCQCNSrc::increaseRate(){
    if (_RC>= _link) {
        //no need to increase, already at line rate!
        return;
    }

    if (max(_T,_BC) <= _F){
        //fast recovery
        _RC = (_RT + _RC) / 2;

    } else if (min(_T,_BC) > _F){
        //hyper increase
        _RT = _RT + (min(_T,_BC)-_F)* _RHAI;
        _RC = (_RT + _RC) / 2;

    } else {
        //active increase 
        _RT += _RAI;
        _RC = (_RT + _RC) / 2;

    }

    if (_RC > _link){
        _RC = _link;
    }

    _pacing_rate = _RC;
    update_spacing();

    cout << "At " << timeAsUs(eventlist().now()) << " " << _RC << " increase target rate is " << _RT << " node " << nodename() << " packet spacing is " << timeAsUs(_packet_spacing) << " _T is " << _T << " _BC is " << _BC << " byte counter is " << _byte_counter << " alpha is " << _alpha << endl;
}

void DCQCNSrc::doNextEvent(){
    bool reschedule = false;

    RoceSrc::doNextEvent();

    cout << "Adding " << (_highest_sent - _old_highest_sent) << " to sent bytes " << _byte_counter << " " << _highest_sent << endl;

    _byte_counter += (_highest_sent - _old_highest_sent);
    _old_highest_sent = _highest_sent;

    if (_byte_counter >= _B){
        _byte_counter = 0;
        _BC++;

        increaseRate();
        reschedule = true;
    }

    if (eventlist().now()-_last_alpha_update >= _cc_update_period){
        _alpha = (1-_g)*_alpha;
        reschedule = true;
    }    


    if (eventlist().now()-_last_cc_update >= _cc_update_period){
        _last_cc_update = eventlist().now();
        _T ++;

        increaseRate();
        reschedule = true;
    }

    if (reschedule)
        eventlist().sourceIsPendingRel(*this, _cc_update_period);
}

void DCQCNSrc::receivePacket(Packet& pkt) 
{
    if (!_flow_started){
        assert(pkt.type()==ETH_PAUSE);
        return; 
    }

    if (_stop_time && eventlist().now() >= _stop_time) {
        // stop sending new data, but allow us to finish any retransmissions
        _flow_size = _highest_sent+_mss;
        _stop_time = 0;
    }

    if (_done)
        return;

    switch (pkt.type()) {
    case ETH_PAUSE:    
        processPause((const EthPausePacket&)pkt);
        pkt.free();
        return;
    case ROCENACK: 
        _nacks_received++;
        processNack((const RoceNack&)pkt);
        pkt.free();
        return;
    case ROCEACK:
        _acks_received++;
        processAck((const RoceAck&)pkt);
        pkt.free();
        return;
    case CNP:
        _cnps_received++;
        processCNP((const CNPPacket&)pkt);
        pkt.free();
        return;

    default:
        abort();
    }
}

////////////////////////////////////////////////////////////////
//  DCQCN SINK
////////////////////////////////////////////////////////////////

/* Only use this constructor when there is only one for to this receiver */
DCQCNSink::DCQCNSink(EventList &eventlist)
    : RoceSink(), EventSource(eventlist, "DCQCN Sink")
{
    _last_cnp_sent_time = UINT64_MAX;
    _marked_packets_since_last_cnp = 0;
    _packets_since_last_cnp = 0;
}

// Receive a packet.
// Note: _cumulative_ack is the last byte we've ACKed.
// seqno is the first byte of the new packet.
void DCQCNSink::receivePacket(Packet& pkt) {
    bool ecn_marked = ((pkt.flags() & ECN_CE) != 0);
    RoceSink::receivePacket(pkt);

    if (ecn_marked){
        //generate CNPs here.
        if (_last_cnp_sent_time == UINT64_MAX || eventlist().now() - _last_cnp_sent_time >= _cnp_interval){
            send_cnp();
            eventlist().sourceIsPendingRel(*this,_cnp_interval);
        }               
        else {
            _marked_packets_since_last_cnp++;
        }
    }
    _packets_since_last_cnp++;
}

void DCQCNSink::doNextEvent(){
    if (eventlist().now() - _last_cnp_sent_time >= _cnp_interval && _marked_packets_since_last_cnp >0){
        send_cnp();
        eventlist().sourceIsPendingRel(*this,_cnp_interval);
    }
}

void DCQCNSink::send_cnp() {
    CNPPacket *cnp = 0;
    cnp = CNPPacket::newpkt(_src->_flow, *_route, _cumulative_ack,_srcaddr);
    cnp->set_pathid(0);

    cnp->sendOn();

    _last_cnp_sent_time = eventlist().now();
    _packets_since_last_cnp = 0;
    _marked_packets_since_last_cnp = 0;
}




