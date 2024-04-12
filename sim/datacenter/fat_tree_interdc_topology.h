// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef FAT_TREE_INTERDC_TOPOLOGY_H
#define FAT_TREE_INTERDC_TOPOLOGY_H
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

// #define N K*K*K/4

#ifndef QT
#define QT
typedef enum {
    UNDEFINED,
    RANDOM,
    ECN,
    COMPOSITE,
    COMPOSITE_BTS,
    PRIORITY,
    CTRL_PRIO,
    FAIR_PRIO,
    LOSSLESS,
    LOSSLESS_INPUT,
    LOSSLESS_INPUT_ECN,
    COMPOSITE_ECN,
    COMPOSITE_ECN_LB,
    SWIFT_SCHEDULER
} queue_type;
typedef enum { UPLINK, DOWNLINK } link_direction;
#endif

// avoid random constants
#define TOR_TIER 0
#define AGG_TIER 1
#define CORE_TIER 2
#define BORDER_TIER 3

class FatTreeInterDCTopology : public Topology {
  public:
    vector<vector<Switch *>> switches_lp;
    vector<vector<Switch *>> switches_up;
    vector<vector<Switch *>> switches_c;
    vector<vector<Switch *>> switches_border;

    vector<vector<vector<Pipe *>>> pipes_nborderl_nborderu;
    vector<vector<vector<vector<Pipe *>>>> pipes_nborder_nc;
    vector<vector<vector<vector<Pipe *>>>> pipes_nc_nup;
    vector<vector<vector<vector<Pipe *>>>> pipes_nup_nlp;
    vector<vector<vector<vector<Pipe *>>>> pipes_nlp_ns;
    vector<vector<vector<BaseQueue *>>> queues_nborderl_nborderu;
    vector<vector<vector<vector<BaseQueue *>>>> queues_nborder_nc;
    vector<vector<vector<vector<BaseQueue *>>>> queues_nc_nup;
    vector<vector<vector<vector<BaseQueue *>>>> queues_nup_nlp;
    vector<vector<vector<vector<BaseQueue *>>>> queues_nlp_ns;

    vector<vector<vector<Pipe *>>> pipes_nborderu_nborderl;
    vector<vector<vector<vector<Pipe *>>>> pipes_nc_nborder;
    vector<vector<vector<vector<Pipe *>>>> pipes_nup_nc;
    vector<vector<vector<vector<Pipe *>>>> pipes_nlp_nup;
    vector<vector<vector<vector<Pipe *>>>> pipes_ns_nlp;
    vector<vector<vector<BaseQueue *>>> queues_nborderu_nborderl;
    vector<vector<vector<vector<BaseQueue *>>>> queues_nc_nborder;
    vector<vector<vector<vector<BaseQueue *>>>> queues_nup_nc;
    vector<vector<vector<vector<BaseQueue *>>>> queues_nlp_nup;
    vector<vector<vector<vector<BaseQueue *>>>> queues_ns_nlp;

    FirstFit *ff;
    QueueLoggerFactory *_logger_factory;
    EventList *_eventlist;
    uint32_t failed_links;
    queue_type _qt;
    queue_type _sender_qt;

    // For regular topologies, just use the constructor.  For custom topologies, load from a config file.
    static FatTreeInterDCTopology *load(const char *filename, QueueLoggerFactory *logger_factory, EventList &eventlist,
                                        mem_b queuesize, queue_type q_type, queue_type sender_q_type);

    FatTreeInterDCTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
                           QueueLoggerFactory *logger_factory, EventList *ev, FirstFit *f, queue_type qt,
                           simtime_picosec latency, simtime_picosec switch_latency, queue_type snd = FAIR_PRIO);
    FatTreeInterDCTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
                           QueueLoggerFactory *logger_factory, EventList *ev, FirstFit *f, queue_type qt);
    FatTreeInterDCTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
                           QueueLoggerFactory *logger_factory, EventList *ev, FirstFit *f, queue_type qt,
                           uint32_t fail);
    FatTreeInterDCTopology(uint32_t no_of_nodes, linkspeed_bps linkspeed, mem_b queuesize,
                           QueueLoggerFactory *logger_factory, EventList *ev, FirstFit *f, queue_type qt,
                           queue_type sender_qt, uint32_t fail);

    static void set_tier_parameters(int tier, int radix_up, int radix_down, mem_b queue_up, mem_b queue_down,
                                    int bundlesize, linkspeed_bps downlink_speed, int oversub);

    void init_network();
    virtual vector<const Route *> *get_bidir_paths(uint32_t src, uint32_t dest, bool reverse);

    BaseQueue *alloc_src_queue(QueueLogger *q);
    BaseQueue *alloc_queue(QueueLogger *q, mem_b queuesize, link_direction dir, bool tor);
    BaseQueue *alloc_queue(QueueLogger *q, mem_b queuesize, link_direction dir, int switch_tier, bool tor);
    BaseQueue *alloc_queue(QueueLogger *q, uint64_t speed, mem_b queuesize, link_direction dir, int switch_tier,
                           bool tor);
    static void set_tiers(uint32_t tiers) { _tiers = tiers; }
    static void set_interdc_delay(uint64_t delay) { _interdc_delay = delay; }
    static void set_os_stage_2(uint32_t os) { _os = os; }
    static void set_os_stage_1(uint32_t os) { _os_ratio_stage_1 = os; }
    static void set_os_ratio_border(uint32_t os) { os_ratio_border = os; }
    static uint32_t get_os_ratio_border() { return os_ratio_border; }
    static uint32_t get_tiers() { return _tiers; }
    static void set_latencies(simtime_picosec src_lp, simtime_picosec lp_up, simtime_picosec up_cs,
                              simtime_picosec lp_switch, simtime_picosec up_switch, simtime_picosec core_switch) {
        _link_latencies[0] = src_lp;
        _link_latencies[1] = lp_up;
        _link_latencies[2] = up_cs;
        _switch_latencies[0] = lp_switch;   // aka tor
        _switch_latencies[1] = up_switch;   // aka tor
        _switch_latencies[2] = core_switch; // aka tor
    }
    static void set_podsize(int hosts_per_pod) { _hosts_per_pod = hosts_per_pod; }

    void count_queue(Queue *);
    void print_path(std::ofstream &paths, uint32_t src, const Route *route);
    vector<uint32_t> *get_neighbours(uint32_t src) { return NULL; };
    uint32_t no_of_nodes() const { return _no_of_nodes; }
    uint32_t no_of_cores() const { return NCORE; }
    uint32_t no_of_servers() const { return NSRV; }
    uint32_t no_of_pods() const { return NPOD; }
    uint32_t tor_switches_per_pod() const { return _tor_switches_per_pod; }
    uint32_t agg_switches_per_pod() const { return _agg_switches_per_pod; }
    uint32_t bundlesize(int tier) const { return _bundlesize[tier]; }
    uint32_t radix_up(int tier) const { return _radix_up[tier]; }
    uint32_t radix_down(int tier) const { return _radix_down[tier]; }
    uint32_t queue_up(int tier) const { return _queue_up[tier]; }
    uint32_t queue_down(int tier) const { return _queue_down[tier]; }

    uint32_t no_of_switches_per_pod() const { return K; }

    uint32_t no_of_links_core_to_border() const { return _no_of_core_to_border; }
    uint32_t no_of_links_core_to_same_border() const { return _num_links_same_border_from_core; }
    uint32_t no_of_links_between_border() const { return _num_links_between_borders; }
    uint32_t no_of_border_switches() const { return number_border_switches; }

    void add_failed_link(uint32_t type, uint32_t switch_id, uint32_t link_id);

    // add loggers to record total queue size at switches
    virtual void add_switch_loggers(Logfile &log, simtime_picosec sample_period);

    static void set_ecn_thresholds_as_queue_percentage(int min_thresh, int max_thresh) {
        kmin = min_thresh;
        kmax = max_thresh;
    }

    static void set_bts_threshold(int value) { bts_trigger = value; }
    static void set_ignore_data_ecn(bool value) { bts_ignore_data = value; }
    static void set_failed_links(int value) { num_failing_links = value; }
    int get_dc_id(int node);

    uint32_t HOST_POD_SWITCH(uint32_t src) { return src / _radix_down[TOR_TIER]; }
    uint32_t HOST_POD_ID(uint32_t src) {
        if (_tiers == 3)
            return src % _hosts_per_pod;
        else
            // only one pod in leaf-spine
            return src;
    }
    uint32_t HOST_POD(uint32_t src) {
        if (_tiers == 3)
            return src / _hosts_per_pod;
        else
            // only one pod in leaf-spine
            return 0;
    }

    uint32_t MIN_POD_TOR_SWITCH(uint32_t pod_id) {
        if (_tiers == 2)
            assert(pod_id == 0);
        return pod_id * _tor_switches_per_pod;
    }
    uint32_t MAX_POD_TOR_SWITCH(uint32_t pod_id) {
        if (_tiers == 2)
            assert(pod_id == 0);
        return (pod_id + 1) * _tor_switches_per_pod - 1;
    }
    uint32_t MIN_POD_AGG_SWITCH(uint32_t pod_id) {
        if (_tiers == 2)
            assert(pod_id == 0);
        return pod_id * _agg_switches_per_pod;
    }
    uint32_t MAX_POD_AGG_SWITCH(uint32_t pod_id) {
        if (_tiers == 2)
            assert(pod_id == 0);
        return (pod_id + 1) * _agg_switches_per_pod - 1;
    }

    // convert an agg switch ID to a pod ID
    uint32_t AGG_SWITCH_POD_ID(uint32_t agg_switch_id) { return agg_switch_id / _agg_switches_per_pod; }

    uint32_t getK() const { return K; }
    uint32_t getOS() const { return _os; }
    uint32_t getOSStage1() const { return _os_ratio_stage_1; }
    uint32_t getNAGG() const { return NAGG; }

  private:
    map<Queue *, int> _link_usage;
    static FatTreeInterDCTopology *load(istream &file, QueueLoggerFactory *logger_factory, EventList &eventlist,
                                        mem_b queuesize, queue_type q_type, queue_type sender_q_type);
    int64_t find_lp_switch(Queue *queue);
    void set_linkspeeds(linkspeed_bps linkspeed);
    int64_t find_up_switch(Queue *queue);
    int64_t find_core_switch(Queue *queue);
    void set_queue_sizes(mem_b queuesize);
    int64_t find_destination(Queue *queue);
    void set_params(uint32_t no_of_nodes);
    void set_custom_params(uint32_t no_of_nodes);
    void alloc_vectors();
    uint32_t K, NCORE, NAGG, NTOR, NSRV, NPOD;
    static uint32_t _tiers;
    static uint64_t _interdc_delay;
    static uint32_t _os, _os_ratio_stage_1;
    uint32_t _no_of_nodes, _no_of_core_to_border, _num_links_same_border_from_core, _num_links_between_borders;
    mem_b _queuesize;
    linkspeed_bps _linkspeed;
    simtime_picosec _hop_latency, _switch_latency;
    static int kmin;
    static int kmax;
    static int bts_trigger;
    static bool bts_ignore_data;
    static int num_failing_links;
    int curr_failed_link = 0;
    int number_border_switches = 1;
    int number_datacenters = 2;
    static int os_ratio_border;

    uint32_t _tor_switches_per_pod, _agg_switches_per_pod;
    // _link_latencies[0] is the ToR->host latency.
    static simtime_picosec _link_latencies[3];

    // _switch_latencies[0] is the ToR switch latency.
    static simtime_picosec _switch_latencies[3];

    static uint32_t _hosts_per_pod;

    static uint32_t _bundlesize[3];
    static linkspeed_bps _downlink_speeds[3];
    static uint32_t _oversub[3];
    static uint32_t _radix_down[3];
    static uint32_t _radix_up[2];
    static mem_b _queue_down[3];
    static mem_b _queue_up[2];
};

#endif
