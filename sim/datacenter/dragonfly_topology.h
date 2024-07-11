// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef DRAGONFLY
#define DRAGONFLY
#include "config.h"
#include "eventlist.h"
#include "firstfit.h"
#include "logfile.h"
#include "loggers.h"
#include "main.h"
#include "network.h"
#include "pipe.h"
#include "randomqueue.h"
#include "switch.h"
#include "topology.h"
#include <ostream>
#include "dragonfly_switch.h"

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
//  N = ap (ah + 1) = 2h * h (2h*h +1) = 4h^4 + 2h^2.
//  h = sqrt(-1 + sqrt(1 + 4N)) / 2
//  g <= ah + 1 = 2h^2 + 1.

#ifndef QT
#define QT
typedef enum {
    RANDOM,
    ECN,
    COMPOSITE,
    CTRL_PRIO,
    LOSSLESS,
    LOSSLESS_INPUT,
    LOSSLESS_INPUT_ECN,
    COMPOSITE_ECN
} queue_type;
#endif

class DragonflyTopology : public Topology {
  public:
    vector<DragonflySwitch *> switches;

    vector<vector<Pipe *>> pipes_host_switch;
    vector<vector<Pipe *>> pipes_switch_switch;
    vector<vector<Pipe *>> pipes_switch_host;
    
    vector<vector<Queue *>> queues_host_switch;
    vector<vector<Queue *>> queues_switch_switch;
    vector<vector<Queue *>> queues_switch_host;

    Logfile *logfile;
    EventList *_eventlist;
    uint32_t failed_links;
    queue_type _qt;

    void init_pipes_queues();

    DragonflyTopology(uint32_t p, uint32_t a, uint32_t h, mem_b queuesize, EventList *ev, queue_type q, simtime_picosec hop_latency);
    DragonflyTopology(uint32_t p, uint32_t a, uint32_t h, mem_b queuesize, EventList *ev, queue_type q, simtime_picosec hop_latency, uint32_t strat);
    void init_network();
    Queue *alloc_src_queue(QueueLogger *q);
    Queue *alloc_queue(QueueLogger *q, mem_b queuesize, bool tor);
    Queue *alloc_queue(QueueLogger *q, uint64_t speed, mem_b queuesize, bool tor);

    uint32_t HOST_TOR_FKT(uint32_t src) { return (src / _p); }

    virtual vector<const Route *> *get_bidir_paths(uint32_t src, uint32_t dest, bool reverse);
    vector<uint32_t> *get_neighbours(uint32_t src) { return NULL; };
    
    uint32_t get_group_size() { return _a; }
    uint32_t get_no_groups() { return _a * _h + 1; }
    uint32_t get_no_global_links() { return _h; }

    static void set_ecn_parameters(bool enable_ecn, mem_b ecn_low, mem_b ecn_high) {
      _enable_ecn = enable_ecn;
      _ecn_low = ecn_low;
      _ecn_high = ecn_high;
    }

    static bool _enable_ecn;
    static mem_b _ecn_low, _ecn_high;

  private:
    void set_params();

    simtime_picosec _hop_latency;

    uint32_t _p, _a, _h;
    uint32_t _no_of_nodes;
    uint32_t _no_of_groups, _no_of_switches;
    mem_b _queuesize;
    uint32_t _df_routing_strategy = 0; // routing_strategy NIX
};
#endif