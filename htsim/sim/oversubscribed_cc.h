// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef OVERSUBSCRIBED_CC_H
#define OVERSUBSCRIBED_CC_H

#include <memory>
#include <tuple>
#include <list>

#include "eventlist.h"
#include "trigger.h"
#include "uecpacket.h"
#include "circular_buffer.h"
#include "modular_vector.h"

class UecPullPacer;

class OversubscribedCC : public EventSource {
public:
    OversubscribedCC(EventList& eventList, UecPullPacer * pacer);

    void doNextEvent();
    void doCongestionControl();

    simtime_picosec nextInterval(){
        //target feedback delay is 1.5 baseRTTs; randonmize around this value to avoid unwanted synchronization between different sources.
        return (simtime_picosec)((0.75+drand()/2)*1.5*_base_rtt);
    }

    inline void ecn_received(mem_b size) {_ecn++;_ecn_bytes += size;}
    inline void data_received(mem_b size) {_received++;_received_bytes += size;}
    inline void trimmed_received(bool last_hop) {
        if (last_hop)
            _trimmed_last_hop++;
        else
            _trimmed_other++;
    }

    static double _target_congestion;
    static double _Ai, _Md, _alpha;
    static simtime_picosec _base_rtt;    
    static double _min_rate;

    inline static void setOversubscriptionRatio(double r) {
        _min_rate = 0.9/r;
        if (_min_rate < 0.01)
            _min_rate = 0.01;
        cout << "Setting min_rate to " << _min_rate * 100 << "% of linerate" << endl;
    }

private:
    double _rate;//total credit rate as dictated by observed congestion, computed dynamically.
    double _g;//marked packets average.
    uint32_t _increase_count;

    UecPullPacer* _pullPacer = NULL;

    uint64_t _received_bytes, _ecn_bytes;
    uint64_t _received, _old_received;
    uint64_t _trimmed_last_hop, _old_trimmed_last_hop;
    uint64_t _trimmed_other, _old_trimmed_other;
    uint64_t _ecn, _old_ecn;
};

#endif