// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef _BCUBE_SWITCH
#define _BCUBE_SWITCH

#include "callback_pipe.h"
#include "switch.h"
#include <unordered_map>
#include "b-cube_topology.h"
#include "fat_tree_switch.h"

class BCubeTopology;

// B-Cube parameters
//  n = number of hosts per router.
//  k = number of levels in the B-Cube.

class BCubeSwitch : public Switch {
  public:
    enum switch_type { NONE = 0, GENERAL = 1};

    enum routing_strategy {
        NIX = 0,
        MINIMAL = 1
    };

    enum sticky_choices { PER_PACKET = 0, PER_FLOWLET = 1 };

    BCubeSwitch(EventList &eventlist, string s, switch_type t, uint32_t id,
                  simtime_picosec switch_delay, BCubeTopology *bt);
    BCubeSwitch(EventList &eventlist, string s, switch_type t, uint32_t id,
                  simtime_picosec switch_delay, BCubeTopology *bt, uint32_t strat);

    virtual void addHostPort(int addr, uint32_t flowid, PacketSink *transport);

    virtual void permute_paths(vector<FibEntry *> *uproutes);

    virtual void receivePacket(Packet &pkt);
    virtual Route *getNextHop(Packet &pkt, BaseQueue *ingress_port);

    static void set_strategy(routing_strategy s) {
        assert(_bc_strategy == NIX);
        _bc_strategy = s;
    }
    static void set_ar_fraction(uint16_t f) {
        assert(f >= 1);
        _ar_fraction = f;
    }

    static void set_precision_ts(int ts) { precision_ts = ts; }

    static routing_strategy _bc_strategy;
    static uint16_t _ar_fraction;
    static uint16_t _ar_sticky;
    static simtime_picosec _sticky_delta;
    static double _ecn_threshold_fraction;
    static int precision_ts;

  private:
    switch_type _type;
    Pipe *_pipe;
    BCubeTopology *_bt;
    vector<pair<simtime_picosec, uint64_t>> _list_sent;

    unordered_map<uint32_t, FlowletInfo *> _flowlet_maps;
    uint32_t _crt_route;
    uint32_t _hash_salt;
    simtime_picosec _last_choice;

    unordered_map<Packet *, bool> _packets;
};

#endif