// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef _DRAGONFLY_SWITCH
#define _DRAGONFLY_SWITCH

#include "callback_pipe.h"
#include "switch.h"
#include <unordered_map>
#include "dragonfly_topology.h"
#include "fat_tree_switch.h"

class DragonflySwitch;

// Dragon Fly parameters
//  p = number of hosts per router.
//  a = number of routers per group.
//  h = number of links used to connect to other groups.
//  k = router radix.
//  g = number of groups.
//
//  The dragonfly parameters a, p, and h can have any values.
//  However to balance channel load on load-balanced traffic, the
//  network should have a = 2p = 2h; p = h;
//  relations between parameters. [Kim et al, ISCA 2008,
//  https://static.googleusercontent.com/media/research.google.com/en//pubs/archive/34926.pdf]
//
//  k = p + h + a - 1
//  k = 4h - 1.
//  N = ap (ah + 1) = 2h * h (2h*h +1) = 4h^4 + 2h^2 = 4 * (k/4)^4 + 2 * (k/4)^2.
//  g <= ah + 1 = 2h^2 + 1.

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

class DragonflySwitch : public Switch {
  public:
    enum switch_type { NONE = 0, GENERAL = 1};

    enum routing_strategy {
        NIX = 0,
        MINIMAL = 1,
        VALIANTS = 2
    };

    enum sticky_choices { PER_PACKET = 0, PER_FLOWLET = 1 };

    DragonflySwitch(EventList &eventlist, string s, switch_type t, uint32_t id,
                  simtime_picosec switch_delay, DragonflyTopology *dt);

    virtual void addHostPort(int addr, int flowid, PacketSink *transport);

    virtual void permute_paths(vector<FibEntry *> *uproutes);

    virtual void receivePacket(Packet &pkt);
    virtual Route *getNextHop(Packet &pkt, BaseQueue *ingress_port);

    static void set_strategy(routing_strategy s) {
        assert(_strategy == NIX);
        _strategy = s;
    }
    static void set_ar_fraction(uint16_t f) {
        assert(f >= 1);
        _ar_fraction = f;
    }

    static void set_precision_ts(int ts) { precision_ts = ts; }

    static routing_strategy _strategy;
    static uint16_t _ar_fraction;
    static uint16_t _ar_sticky;
    static simtime_picosec _sticky_delta;
    static double _ecn_threshold_fraction;
    static int precision_ts;

  private:
    switch_type _type;
    Pipe *_pipe;
    DragonflyTopology *_dt;
    vector<pair<simtime_picosec, uint64_t>> _list_sent;

    // CAREFUL: can't always have a single FIB for all up destinations when
    // there are failures!
    vector<FibEntry *> *_uproutes;

    unordered_map<uint32_t, FlowletInfo *> _flowlet_maps;
    uint32_t _crt_route;
    uint32_t _hash_salt;
    simtime_picosec _last_choice;

    unordered_map<Packet *, bool> _packets;
};

#endif