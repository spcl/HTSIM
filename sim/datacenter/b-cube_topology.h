// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef BCUBE
#define BCUBE
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
#include "b-cube_switch.h"

class BCubeSwitch;

// B-Cube parameters
//  n = number of hosts per router.
//  k = number of levels in the B-Cube.

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

class BCubeTopology : public Topology {
  public:
    vector<BCubeSwitch *> switches;

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

    BCubeTopology(uint32_t n, uint32_t k, mem_b queuesize, EventList *ev, queue_type q);
    BCubeTopology(uint32_t n, uint32_t k, mem_b queuesize, EventList *ev, queue_type q, uint32_t strat);
    void init_network();
    Queue *alloc_src_queue(QueueLogger *q);
    Queue *alloc_queue(QueueLogger *q, mem_b queuesize, bool tor);
    Queue *alloc_queue(QueueLogger *q, uint64_t speed, mem_b queuesize, bool tor);

    virtual vector<const Route *> *get_bidir_paths(uint32_t src, uint32_t dest, bool reverse);
    vector<uint32_t> *get_neighbours(uint32_t src) { return NULL; };
    
    uint32_t get_n() { return _n; }
    uint32_t get_k() { return _k; }

    uint32_t get_no_of_nodes() { return _no_of_nodes; }

  private:
    void set_params();

    uint32_t _n, _k;
    uint32_t _no_of_nodes;
    uint32_t _no_of_switches;
    mem_b _queuesize;
    uint32_t _bc_routing_strategy = 0; // routing_strategy NIX
};
#endif