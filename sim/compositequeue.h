// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef COMPOSITE_QUEUE_H
#define COMPOSITE_QUEUE_H

/*
 * A composite queue that transforms packets into headers when there is no space
 * and services headers with priority.
 */

#define QUEUE_INVALID 0
#define QUEUE_LOW 1
#define QUEUE_HIGH 2

#include "config.h"
#include "eventlist.h"
#include "loggertypes.h"
#include "network.h"
#include "queue.h"
#include <list>

class CompositeQueue : public Queue {
  public:
    CompositeQueue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist,
                   QueueLogger *logger);
    virtual void receivePacket(Packet &pkt);
    virtual void doNextEvent();
    // should really be private, but loggers want to see
    mem_b _queuesize_low, _queuesize_high, _current_queuesize_phatom;
    int num_headers() const { return _num_headers; }
    int num_packets() const { return _num_packets; }
    int num_stripped() const { return _num_stripped; }
    int num_bounced() const { return _num_bounced; }
    int num_acks() const { return _num_acks; }
    int num_nacks() const { return _num_nacks; }
    int num_pulls() const { return _num_pulls; }
    virtual mem_b queuesize() const;
    virtual void setName(const string &name) {
        Logged::setName(name);
        _nodename += name;
    }
    virtual const string &nodename() { return _nodename; }
    void set_ecn_threshold(mem_b ecn_thresh) {
        _ecn_minthresh = ecn_thresh;
        _ecn_maxthresh = ecn_thresh;
    }
    void set_ecn_thresholds(mem_b min_thresh, mem_b max_thresh) {
        _ecn_minthresh = min_thresh;
        _ecn_maxthresh = max_thresh;
    }

    void set_bts_threshold(mem_b bts_triggers_at) {
        _bts_triggering = bts_triggers_at;
    }

    static void set_drop_when_full(bool do_drop_full) {
        _drop_when_full = do_drop_full;
    }
    static void set_use_mixed(bool use_m) { _use_mixed = use_m; }

    static void set_use_phantom_queue(bool use_phantom) {
        _use_phantom = use_phantom;
    }

    static void set_phantom_queue_size(int phantom_size) {
        _phantom_queue_size = phantom_size;
    }

    static void set_phantom_queue_slowdown(int phantom_size_slow) {
        _phantom_queue_slowdown = phantom_size_slow;
    }

    /*void set_os_link_ratio(int os_link_ratio) {
        _os_link_ratio = os_link_ratio;
    }*/
    static bool _use_mixed;
    static bool _drop_when_full;
    static bool _use_phantom;
    static int _phantom_queue_size;
    static int _phantom_queue_slowdown;
    int _num_packets;
    int _num_headers; // only includes data packets stripped to headers, not
                      // acks or nacks
    int _num_acks;
    int _num_nacks;
    int _num_pulls;
    int _num_stripped; // count of packets we stripped
    int _num_bounced;  // count of packets we bounced
    int packets_seen = 0;
    bool first_time = true;
    int trimmed_seen = 0;
    simtime_picosec _decrease_phantom_next = 0;
    simtime_picosec _draining_time_phantom = 0;

  protected:
    // Mechanism
    void beginService(); // start serving the item at the head of the queue
    void decreasePhantom();
    void completeService(); // wrap up serving the item at the head of the queue
    bool decide_ECN();

    int _serv;
    int _ratio_high, _ratio_low, _crt;
    // below minthresh, 0% marking, between minthresh and maxthresh
    // increasing random mark propbability, abve maxthresh, 100%
    // marking.
    mem_b _ecn_minthresh;
    mem_b _ecn_maxthresh;
    mem_b _bts_triggering;

    CircularBuffer<Packet *> _enqueued_low;
    CircularBuffer<Packet *> _enqueued_high;
};

#endif
