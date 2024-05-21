// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef _SLIMFLY_SWITCH
#define _SLIMFLY_SWITCH

#include "callback_pipe.h"
#include "switch.h"
#include <unordered_map>
#include "slimfly_topology.h"
#include "fat_tree_switch.h"

class SlimflyTopology;

// Slimfly parameters
//  p       = number of hosts per router.
//  q       = prime power to generate connections.
//  q_base  = base of q (must be a prime).
//  q_exp   = exponent of q (> 0; and for q_base = 2: > 1).
//  xi      = group generator.
//  k       = router radix.
//  Ns      = number of switches.
//  Nh      = number of hosts.
//
//  q = 4w + d     with w € N and d € {-1, 0, 1}.
//
//  k   = ((3q - d) / 2) + p.
//  Ns  = 2q^2.
//  Nh  = p * 2q^2.

/* #define HOST_TOR(src) (src / _p)
#define HOST_GROUP(src) (src / (_a * _p)) */

/* class FlowletInfo {
  public:
    uint32_t _egress;
    simtime_picosec _last;

    FlowletInfo(uint32_t egress, simtime_picosec lasttime) {
        _egress = egress;
        _last = lasttime;
    };
}; */

class SlimflySwitch : public Switch {
  public:
    enum switch_type { NONE = 0, GENERAL = 1};

    enum routing_strategy {
        NIX = 0,
        MINIMAL = 1,
        VALIANTS = 2
    };

    enum sticky_choices { PER_PACKET = 0, PER_FLOWLET = 1 };

    SlimflySwitch(EventList &eventlist, string s, switch_type t, uint32_t id,
                  simtime_picosec switch_delay, SlimflyTopology *st);
    SlimflySwitch(EventList &eventlist, string s, switch_type t, uint32_t id,
                  simtime_picosec switch_delay, SlimflyTopology *st, uint32_t strat);

    virtual void addHostPort(int addr, uint32_t flowid, PacketSink *transport);

    virtual void permute_paths(vector<FibEntry *> *uproutes);

    virtual void receivePacket(Packet &pkt);
    virtual Route *getNextHop(Packet &pkt, BaseQueue *ingress_port);

    static void set_strategy(routing_strategy s) {
        assert(_sf_strategy == NIX);
        _sf_strategy = s;
    }
    static void set_ar_fraction(uint16_t f) {
        assert(f >= 1);
        _ar_fraction = f;
    }

    static void set_precision_ts(int ts) { precision_ts = ts; }

    static routing_strategy _sf_strategy;
    static uint16_t _ar_fraction;
    static uint16_t _ar_sticky;
    static simtime_picosec _sticky_delta;
    static double _ecn_threshold_fraction;
    static int precision_ts;

    uint32_t modulo(int x, int y);

  private:
    switch_type _type;
    Pipe *_pipe;
    SlimflyTopology *_st;
    vector<pair<simtime_picosec, uint64_t>> _list_sent;

    unordered_map<uint32_t, FlowletInfo *> _flowlet_maps;
    uint32_t _crt_route;
    uint32_t _hash_salt;
    simtime_picosec _last_choice;

    unordered_map<Packet *, bool> _packets;
};

#endif