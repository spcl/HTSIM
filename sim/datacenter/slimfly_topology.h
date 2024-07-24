// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef SLIMFLY
#define SLIMFLY
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
#include "slimfly_switch.h"

class SlimflySwitch;

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

class SlimflyTopology : public Topology {
  public:
    vector<SlimflySwitch *> switches;

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

    SlimflyTopology(uint32_t p, uint32_t q_base, uint32_t q_exp, mem_b queuesize, EventList *ev, queue_type qt, simtime_picosec hop_latency, simtime_picosec short_hop_latency);
    SlimflyTopology(uint32_t p, uint32_t q_base, uint32_t q_exp, mem_b queuesize, EventList *ev, queue_type qt, simtime_picosec hop_latency, simtime_picosec short_hop_latency, uint32_t strat);
    
    void init_network();
    Queue *alloc_src_queue(QueueLogger *qL);
    Queue *alloc_queue(QueueLogger *qL, mem_b queuesize, bool tor);
    Queue *alloc_queue(QueueLogger *qL, uint64_t speed, mem_b queuesize, bool tor);

    uint32_t HOST_TOR_FKT(uint32_t src) { return (src / _p); }
    bool is_element(vector<uint32_t> arr, uint32_t el);

    virtual vector<const Route *> *get_bidir_paths(uint32_t src, uint32_t dest, bool reverse){ return NULL; };
    vector<uint32_t> *get_neighbours(uint32_t src) { return NULL; };
    
    uint32_t get_q() { return _q; }
    uint32_t get_xi() { return _xi; }
    vector<uint32_t> get_X() { return _X;}
    vector<uint32_t> get_Xp() { return _Xp;}

    static void set_ecn_parameters(bool enable_ecn, mem_b ecn_low, mem_b ecn_high) {
        _enable_ecn = enable_ecn;
        _ecn_low = ecn_low;
        _ecn_high = ecn_high;
    }

    static bool _enable_ecn;
    static mem_b _ecn_low, _ecn_high;

  private:
    void set_params();
    uint32_t get_generator(uint32_t q);
    bool is_prime(uint32_t q_base);
    uint32_t modulo(int x, int y);
    void create_switch_switch_link(uint32_t k, uint32_t j, QueueLoggerSampling *queueLogger, simtime_picosec hop_latency);

    simtime_picosec _hop_latency, _short_hop_latency;

    uint32_t _q_base, _q_exp;
    uint32_t _p, _q, _xi;
    vector<uint32_t> _X, _Xp;
    uint32_t _no_of_switches, _no_of_nodes;
    mem_b _queuesize;
    uint32_t _sf_routing_strategy = 0; // SlimflySwitch::routing_strategy::NIX
};
#endif