// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef OV_FAT_TREE
#define OV_FAT_TREE
#include "config.h"
#include "eventlist.h"
#include "firstfit.h"
#include "logfile.h"
#include "loggers.h"
#include "main.h"
#include "network.h"
#include "pipe.h"
#include "randomqueue.h"
#include "topology.h"
#include <ostream>

/*#define K 8
#define NK (K * K / 2)
#define NC (K * K / 4)
#define NSRV (K * K * K)

#define HOST_POD_SWITCH(src) (src / 2 / K)
#define HOST_POD(src) (src / K / K)

#define MIN_POD_ID(pod_id) (pod_id * K / 2)
#define MAX_POD_ID(pod_id) ((pod_id + 1) * K / 2 - 1)*/

#ifndef QT
#define QT
typedef enum {
    RANDOM,
    ECN,
    COMPOSITE,
    COMPOSITE_BTS,
    CTRL_PRIO,
    LOSSLESS,
    LOSSLESS_INPUT,
    LOSSLESS_INPUT_ECN
} queue_type;
#endif

class OversubscribedFatTreeTopology : public Topology {
  public:
    vector<vector<Pipe *>> pipes_nc_nup;
    vector<vector<Pipe *>> pipes_nup_nlp;
    vector<vector<Pipe *>> pipes_nlp_ns;
    vector<vector<Queue *>> queues_nc_nup;
    vector<vector<Queue *>> queues_nup_nlp;
    vector<vector<Queue *>> queues_nlp_ns;

    vector<vector<Pipe *>> pipes_nup_nc;
    vector<vector<Pipe *>> pipes_nlp_nup;
    vector<vector<Pipe *>> pipes_ns_nlp;
    vector<vector<Queue *>> queues_nup_nc;
    vector<vector<Queue *>> queues_nlp_nup;
    vector<vector<Queue *>> queues_ns_nlp;

    FirstFit *ff;
    Logfile *logfile;
    EventList *eventlist;

    queue_type qt;

    OversubscribedFatTreeTopology(mem_b queuesize, linkspeed_bps linkspeed,
                                  Logfile *lg, EventList *ev, FirstFit *fit,
                                  queue_type q, simtime_picosec latency,
                                  simtime_picosec switch_latency, int k);

    void init_network();
    virtual vector<const Route *> *get_bidir_paths(uint32_t src, uint32_t dest,
                                                   bool reverse);

    static void set_ecn_thresholds_as_queue_percentage(int min_thresh,
                                                       int max_thresh) {
        kmin = min_thresh;
        kmax = max_thresh;
    }

    static void set_bts_threshold(int value) { bts_trigger = value; }

    Queue *alloc_src_queue(QueueLogger *q);
    Queue *alloc_queue(QueueLogger *q);
    Queue *alloc_queue(QueueLogger *q, uint64_t speed);

    void count_queue(Queue *);
    void print_path(std::ofstream &paths, uint32_t src, const Route *route);
    vector<uint32_t> *get_neighbours(uint32_t src);
    uint32_t no_of_nodes() const { return _no_of_nodes; }

    uint32_t HOST_POD_SWITCH(uint32_t src) { return src / 2 / K; }
    uint32_t HOST_POD(uint32_t src) { return src / K / K; }
    uint32_t MIN_POD_ID(uint32_t pod_id) { return pod_id * K / 2; }
    uint32_t MAX_POD_ID(uint32_t pod_id) { return (pod_id + 1) * K / 2 - 1; }

  private:
    map<Queue *, int> _link_usage;
    int64_t find_lp_switch(Queue *queue);
    int64_t find_up_switch(Queue *queue);
    int64_t find_core_switch(Queue *queue);
    int64_t find_destination(Queue *queue);
    void set_params(uint32_t no_of_nodes);
    uint32_t K, _no_of_nodes;
    mem_b _queuesize;
    uint32_t NCORE, NAGG, NTOR, NSRV, NPOD;
    static int kmin;
    static int kmax;
    static int bts_trigger;
    linkspeed_bps _linkspeed;
    simtime_picosec _hop_latency, _switch_latency;
};

#endif
