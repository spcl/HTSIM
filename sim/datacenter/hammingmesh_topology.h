// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef HAMMINGMESH
#define HAMMINGMESH
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
#include "hammingmesh_switch.h"

class HammingmeshSwitch;

// Hammingmesh parameters:

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

class HammingmeshTopology : public Topology {
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

    // HammingmeshTopology(uint32_t height, uint32_t width, uint32_t height_board, uint32_t width_board, mem_b queuesize, EventList *ev, queue_type qt);
    HammingmeshTopology(uint32_t height, uint32_t width, uint32_t height_board, uint32_t width_board, mem_b queuesize, EventList *ev, queue_type qt, uint32_t strat);
    
    void init_network();
    Queue *alloc_src_queue(QueueLogger *qL);
    Queue *alloc_queue(QueueLogger *qL, mem_b queuesize, bool tor);
    Queue *alloc_queue(QueueLogger *qL, uint64_t speed, mem_b queuesize, bool tor);

    bool is_element(vector<uint32_t> arr, uint32_t el);

    virtual vector<const Route *> *get_bidir_paths(uint32_t src, uint32_t dest, bool reverse){ return NULL; };
    vector<uint32_t> *get_neighbours(uint32_t src) { return NULL; };
    
    // uint32_t get_group_size() { return _a; }
    // uint32_t get_no_groups() { return _a * _h + 1; }
    // uint32_t get_no_global_links() { return _h; }

  private:
    void set_params();
    int modulo(int x, int y);
    void create_switch_switch_link(uint32_t k, uint32_t j, QueueLoggerSampling *queueLogger);

    uint32_t _height, _width, _height_board, _width_board;
    uint32_t _no_of_switches, _no_of_nodes, _no_of_groups;
    mem_b _queuesize;
    uint32_t _hm_routing_strategy = 0; // routing_strategy NIX
};
#endif