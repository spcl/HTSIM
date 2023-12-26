// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef _LOSSLESS_INPUT_QUEUE_H
#define _LOSSLESS_INPUT_QUEUE_H
#include "queue.h"
/*
 * A FIFO queue that supports PAUSE frames and lossless operation
 */

#include "config.h"
#include "eth_pause_packet.h"
#include "eventlist.h"
#include "loggertypes.h"
#include "network.h"
#include "switch.h"
#include <list>

class Switch;

class LosslessInputQueue : public Queue, public VirtualQueue {
  public:
    LosslessInputQueue(EventList &eventlist);
    LosslessInputQueue(EventList &eventlist, BaseQueue *peer, Switch *sw);
    LosslessInputQueue(EventList &eventlist, BaseQueue *peer);

    virtual void receivePacket(Packet &pkt);

    void sendPause(unsigned int wait);
    virtual void completedService(Packet &pkt);

    virtual void setName(const string &name) {
        Logged::setName(name);
        _nodename += name;
    }
    virtual string &nodename() { return _nodename; }

    enum { PAUSED, READY, PAUSE_RECEIVED };

    static uint64_t _low_threshold;
    static uint64_t _high_threshold;
    static int _mark_pfc_amount;

    void set_ecn_threshold(mem_b ecn_thresh) {
        _ecn_minthresh = ecn_thresh;
        _ecn_maxthresh = ecn_thresh;
    }
    void set_ecn_thresholds(mem_b min_thresh, mem_b max_thresh) {
        _ecn_minthresh = min_thresh;
        _ecn_maxthresh = max_thresh;
    }

  private:
    mem_b _ecn_minthresh;
    mem_b _ecn_maxthresh;
    int _state_recv;
    bool decide_ECN();
    bool pfc_happened = false;
};

#endif
