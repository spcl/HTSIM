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
    vector<HammingmeshSwitch *> switches;

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

    HammingmeshTopology(uint32_t height, uint32_t width, uint32_t height_board, uint32_t width_board, mem_b queuesize, EventList *ev, queue_type qt, simtime_picosec hop_latency);
    HammingmeshTopology(uint32_t height, uint32_t width, uint32_t height_board, uint32_t width_board, mem_b queuesize, EventList *ev, queue_type qt, simtime_picosec hop_latency, uint32_t strat);
    
    void init_network();
    Queue *alloc_src_queue(QueueLogger *qL);
    Queue *alloc_queue(QueueLogger *qL, mem_b queuesize, bool tor);
    Queue *alloc_queue(QueueLogger *qL, uint64_t speed, mem_b queuesize, bool tor);

    bool is_element(vector<uint32_t> arr, uint32_t el);

    virtual vector<const Route *> *get_bidir_paths(uint32_t src, uint32_t dest, bool reverse){ return NULL; };
    vector<uint32_t> *get_neighbours(uint32_t src) { return NULL; };
    
    uint32_t get_height() { return _height; }
    uint32_t get_width() { return _width; }
    uint32_t get_height_board() { return _height_board; }
    uint32_t get_width_board() { return _width_board; }
    int get_no_nodes() {
      int ft_size_h, ft_size_w;
      if ((2 * _height) > 64){ ft_size_h = (2 * _height) / 63 + 1; }
      else{ ft_size_h = 1; }
      if ((2 * _width) > 64){ ft_size_w = (2 * _width) / 63 + 1; }
      else{ ft_size_w = 1; }
      return (_height * _width * _height_board * _width_board) + (_width * _width_board * ft_size_h) + (_height * _height_board * ft_size_w);
    }

    static void set_ecn_parameters(bool enable_ecn, mem_b ecn_low, mem_b ecn_high) {
      _enable_ecn = enable_ecn;
      _ecn_low = ecn_low;
      _ecn_high = ecn_high;
    }

    static bool _enable_ecn;
    static mem_b _ecn_low, _ecn_high;

  private:
    void set_params();
    int modulo(int x, int y);
    void create_switch_switch_link(uint32_t k, uint32_t j, QueueLoggerSampling *queueLogger);
    
    simtime_picosec _hop_latency;

    uint32_t _height, _width, _height_board, _width_board;
    uint32_t _no_of_switches, _no_of_nodes, _no_of_groups;
    mem_b _queuesize;
    uint32_t _hm_routing_strategy = 0; // HammingmeshSwitch::routing_strategy::NIX
};
#endif