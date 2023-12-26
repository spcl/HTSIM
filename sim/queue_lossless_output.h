// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef _LOSSLESS_OUTPUT_QUEUE_H
#define _LOSSLESS_OUTPUT_QUEUE_H
/*
 * A FIFO queue that supports PAUSE frames and lossless operation
 */

#include "config.h"
#include "ecn.h"
#include "eth_pause_packet.h"
#include "eventlist.h"
#include "loggertypes.h"
#include "network.h"
#include "queue.h"
#include <list>

#define QUEUE_INVALID 0
#define QUEUE_LOW 1
#define QUEUE_HIGH 2

class LosslessOutputQueue : public Queue {
  public:
    LosslessOutputQueue(linkspeed_bps bitrate, mem_b maxsize,
                        EventList &eventlist, QueueLogger *logger, int ECN = 0,
                        int K = 0);

    void receivePacket(Packet &pkt);
    void receivePacket(Packet &pkt, VirtualQueue *q);

    void beginService();
    void completeService();

    bool is_paused() {
        return _state_send == PAUSED || _state_send == PAUSE_RECEIVED;
    }

    enum queue_state { PAUSED, READY, PAUSE_RECEIVED };

    void set_ecn_threshold(mem_b ecn_thresh) {
        _ecn_minthresh = ecn_thresh;
        _ecn_maxthresh = ecn_thresh;
    }
    void set_ecn_thresholds(mem_b min_thresh, mem_b max_thresh) {
        _ecn_minthresh = min_thresh;
        _ecn_maxthresh = max_thresh;
    }

  private:
    list<VirtualQueue *> _vq;

    int _state_send;
    int _sending;
    uint64_t _txbytes;

    mem_b _ecn_minthresh;
    mem_b _ecn_maxthresh;
    bool decide_ECN();

    int _ecn_enabled;
    int _K;
};

#endif
