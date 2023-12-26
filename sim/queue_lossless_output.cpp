// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "queue_lossless_output.h"
#include "hpccpacket.h"
#include "queue_lossless_input.h"
#include "switch.h"
#include <iostream>
#include <math.h>
#include <sstream>

LosslessOutputQueue::LosslessOutputQueue(linkspeed_bps bitrate, mem_b maxsize,
                                         EventList &eventlist,
                                         QueueLogger *logger, int ECN, int K)
        : Queue(bitrate, maxsize, eventlist, logger), _state_send(READY) {
    // assume worst case: PAUSE frame waits for one MSS packet to be sent to
    // other switch, and there is an MSS just beginning to be sent when PAUSE
    // frame arrives; this means 2 packets per incoming port, and we must have
    // buffering for all ports except this one (assuming no one hop cycles!)

    _sending = 0;

    _ecn_enabled = ECN;
    _K = K;
    _txbytes = 0;

    stringstream ss;
    ss << "queue lossless output(" << bitrate / 1000000 << "Mb/s," << maxsize
       << "bytes)";
    _nodename = ss.str();
}

void LosslessOutputQueue::receivePacket(Packet &pkt) {
    if (pkt.type() == ETH_PAUSE)
        receivePacket(pkt, NULL);
    else {
        LosslessInputQueue *q = pkt.get_ingress_queue();
        pkt.clear_ingress_queue();
        pkt.enter_timestamp = GLOBAL_TIME;
        receivePacket(pkt, dynamic_cast<VirtualQueue *>(q));
    }
}

bool LosslessOutputQueue::decide_ECN() {
    // ECN mark on deque
    if (_queuesize > _ecn_maxthresh) {
        return true;
    } else if (_queuesize > _ecn_minthresh) {
        uint64_t p = (0x7FFFFFFF * (_queuesize - _ecn_minthresh)) /
                     (_ecn_maxthresh - _ecn_minthresh);
        if ((uint64_t)random() < p) {
            return true;
        }
    }
    return false;
}

void LosslessOutputQueue::receivePacket(Packet &pkt, VirtualQueue *prev) {
    printf("Received here1 %d\n", pkt.size());
    // is this a PAUSE frame?
    if (pkt.type() == ETH_PAUSE) {
        EthPausePacket *p = (EthPausePacket *)&pkt;

        if (p->sleepTime() > 0) {
            // remote end is telling us to shut up.
            // assert(_state_send == READY);
            if (_sending) {
                // we have a packet in flight
                _state_send = PAUSE_RECEIVED;
            } else {
                printf("%s Pause Send for %d\n", _nodename.c_str(),
                       p->sleepTime());
                _state_send = PAUSED;
            }

            // cout << timeAsMs(eventlist().now()) << " " << _name << " PAUSED
            // "<<endl;
        } else {
            // we are allowed to send!
            _state_send = READY;
            // cout << timeAsMs(eventlist().now()) << " " << _name << " GO
            // "<<endl;
            printf("%s Can Send Again %d at %ld\n", _nodename.c_str(),
                   p->sleepTime(), eventlist().now() / 1000);
            // start transmission if we have packets to send!
            if (_enqueued.size() > 0 && !_sending)
                beginService();
        }

        pkt.free();
        return;
    }

    /* normal packet, enqueue it */

    // remember the virtual queue that has sent us this packet; will notify the
    // vq once the packet has left our buffer.
    assert(prev != NULL);

    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);

    bool queueWasEmpty = _enqueued.empty();

    _vq.push_front(prev);
    Packet *pkt_p = &pkt;
    _enqueued.push(pkt_p);

    _queuesize += pkt.size();

    if (_queuesize > _maxsize) {
        cout << " Queue " << _name
             << " LOSSLESS not working! I should have dropped this packet"
             << _queuesize / Packet::data_packet_size() << endl;
    }

    if (_logger)
        _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);

    if (queueWasEmpty && _state_send == READY) {
        /* schedule the dequeue event */
        assert(_enqueued.size() == 1);
        beginService();
    }
}

void LosslessOutputQueue::beginService() {
    assert(_state_send == READY && !_sending);

    Queue::beginService();
    _sending = 1;
}

void LosslessOutputQueue::completeService() {
    /* dequeue the packet */
    assert(!_enqueued.empty());

    // Edit Queue
    std::vector<Packet *> new_queue;
    std::vector<Packet *> temp_queue = _enqueued._queue;
    int count_good = 0;
    int dropped_p = 0;
    int initial_count = _enqueued._count;

    for (int i = 0; i < _enqueued._count; i++) {
        printf("Budget From %d - ID %d - PAcket Type %d - Enter TS %li - "
               "Budget %li - Index %d - Time "
               "%lu\n",
               temp_queue[_enqueued._next_pop + i]->from,
               temp_queue[_enqueued._next_pop + i]->id(),
               temp_queue[_enqueued._next_pop + i]->type(),
               temp_queue[_enqueued._next_pop + i]->enter_timestamp,
               temp_queue[_enqueued._next_pop + i]->timeout_budget, i,
               GLOBAL_TIME);

        int64_t diff = GLOBAL_TIME -
                       temp_queue[_enqueued._next_pop + i]->enter_timestamp;
        int64_t diff_budget =
                temp_queue[_enqueued._next_pop + i]->timeout_budget - diff;
        int size_p = temp_queue[_enqueued._next_pop + i]->size();
        if (diff_budget > 0 ||
            _enqueued._queue[count_good]->type() == ETH_PAUSE ||
            _enqueued._queue[count_good]->type() == UECACK_DROP) {
            _enqueued._queue[count_good] = temp_queue[_enqueued._next_pop + i];

            /*int64_t diff =
                    GLOBAL_TIME - _enqueued._queue[count_good]->enter_timestamp;

            int64_t diff_budget =
                    _enqueued._queue[count_good]->timeout_budget - diff;*/
            //_enqueued._queue[count_good]->timeout_budget = diff_budget;
            count_good++;
        } else {
            dropped_p++;
            _queuesize -= size_p;
        }
    }
    _enqueued._next_pop = 0;
    _enqueued._next_push = count_good;
    _enqueued._count = count_good;

    if (dropped_p > 0) {
        printf("Started With %d Pkts - Dropped %d Pkts - Count %d - Pop %d "
               "- Push %d\n",
               initial_count, dropped_p, _enqueued._count, _enqueued._next_pop,
               _enqueued._next_push);
    } else {
        printf("Not dropped\n");
    }

    fflush(stdout);
    if (_enqueued._count <= 0) {
        _sending = 0;
        return;
    }
    Packet *pkt = _enqueued.pop();
    VirtualQueue *q = _vq.back();

    printf("Budget From %d - ID %d - Budget %li\n", pkt->from, pkt->id(),
           pkt->timeout_budget);

    //_enqueued.pop_back();
    _vq.pop_back();

    printf("OUT (%s): MaxQueueSize %lld - CurrentSize %lld - PFC High %d - PFC "
           "Low "
           "%d\n",
           _nodename.c_str(), _maxsize, _queuesize, 1, 1);

    if (decide_ECN()) {
        pkt->set_flags(pkt->flags() | ECN_CE);
        printf("Setting ECN Flag %lu\n", eventlist().now());
    }

    if (pkt->type() == HPCC) {
        // HPPC INT information adding to packet
        HPCCPacket *h = dynamic_cast<HPCCPacket *>(pkt);
        assert(h->_int_hop < 5);

        h->_int_info[h->_int_hop]._queuesize = _queuesize;
        h->_int_info[h->_int_hop]._ts = eventlist().now();

        if (_switch) {
            h->_int_info[h->_int_hop]._switchID = _switch->getID();
            h->_int_info[h->_int_hop]._type = _switch->getType();
        }

        h->_int_info[h->_int_hop]._txbytes = _txbytes;
        h->_int_info[h->_int_hop]._linkrate = _bitrate;

        h->_int_hop++;
    }

    _queuesize -= pkt->size();
    _txbytes += pkt->size();

    pkt->flow().logTraffic(*pkt, *this, TrafficLogger::PKT_DEPART);

    if (_logger)
        _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);

    // tell the virtual input queue this packet is done!
    q->completedService(*pkt);

    // this is used for bandwidth utilization tracking.
    log_packet_send(drainTime(pkt));

    // if (((uint64_t)timeAsUs(eventlist().now()))%5==0)
    //     cout << "Queue bandwidth utilization " << average_utilization() <<
    //     "%" << endl;

    /* tell the packet to move on to the next pipe */
    pkt->sendOn();

    _sending = 0;

    // if (_state_send!=READY){
    // cout << timeAsMs(eventlist().now()) << " queue " << _name << " not ready
    // but sending pkt " << pkt->type() << endl;
    // }

    if (_state_send == PAUSE_RECEIVED)
        _state_send = PAUSED;

    if (!_enqueued.empty()) {
        if (_state_send == READY)
            /* start packet transmission, schedule the next dequeue event */
            beginService();
    }
}
