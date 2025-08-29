// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        


#ifndef DCQCN_H
#define DCQCN_H

/*
 * A DCQCN source and sink
 */

#include <list>
#include <map>
//#include "util.h"
#include "math.h"
#include "config.h"
#include "network.h"
#include "rocepacket.h"
#include "cnppacket.h"
#include "queue.h"
#include "roce.h"
#include "eventlist.h"
#include "eth_pause_packet.h"
#include "trigger.h"
#include "ecn.h"
#define timeInf 0

class DCQCNSink;
class Switch;

class DCQCNSrc : public RoceSrc {
    friend class DCQCNSink;
public:
    DCQCNSrc(RoceLogger* logger, TrafficLogger* pktlogger, EventList &eventlist, linkspeed_bps rate);

    virtual void receivePacket(Packet& pkt);
    virtual void processCNP(const CNPPacket& cnp);    
    virtual void increaseRate();
    virtual void doNextEvent();

    // should really be private, but loggers want to see:
    uint32_t _cnps_received;    

    static simtime_picosec _cc_update_period;
    static double _alpha, _g;
    static uint32_t _F;
    static linkspeed_bps _RAI, _RHAI;
    static uint64_t _B;

private:
    simtime_picosec _last_cc_update, _last_alpha_update;
    linkspeed_bps _RC, _RT, _link;
    
    enum increase_state {invalid = 0, fast_recovery=1,active_increase=2};
    //increase_state _ai_state;
    uint16_t _T,_BC;
    uint64_t _byte_counter, _old_highest_sent;

};

class DCQCNSink : public RoceSink, public EventSource {
    friend class DCQCNSrc;
public:
    DCQCNSink(EventList &eventlist);
    virtual void doNextEvent();

    virtual void receivePacket(Packet& pkt);  
    static simtime_picosec _cnp_interval;

    inline id_t get_id() const {return RoceSink::get_id();}

private:
    simtime_picosec _last_cnp_sent_time;
 
    uint32_t _marked_packets_since_last_cnp;
    uint32_t _packets_since_last_cnp;

    // Mechanism
    void send_cnp();
};

#endif

