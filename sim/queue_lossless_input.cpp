// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "queue_lossless_input.h"
#include "ecn.h"
#include "switch.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <math.h>
#include <regex>
#include <sstream>
#include <stdio.h>
#include <utility>

uint64_t LosslessInputQueue::_high_threshold = 0;
uint64_t LosslessInputQueue::_low_threshold = 0;
int LosslessInputQueue::_mark_pfc_amount = 0;

LosslessInputQueue::LosslessInputQueue(EventList &eventlist)
        : Queue(speedFromGbps(800), Packet::data_packet_size() * 2000,
                eventlist, NULL),
          VirtualQueue(), _state_recv(READY) {
    assert(_high_threshold > 0);
    assert(_high_threshold > _low_threshold);
}

LosslessInputQueue::LosslessInputQueue(EventList &eventlist, BaseQueue *peer)
        : Queue(speedFromGbps(800), Packet::data_packet_size() * 2000,
                eventlist, NULL),
          VirtualQueue(), _state_recv(READY) {
    assert(_high_threshold > 0);
    assert(_high_threshold > _low_threshold);

    stringstream ss;
    ss << "VirtualQueue(" << peer->_name << ")";
    _nodename = ss.str();
    _remoteEndpoint = peer;
    _switch = NULL;

    peer->setRemoteEndpoint(this);
}

LosslessInputQueue::LosslessInputQueue(EventList &eventlist, BaseQueue *peer,
                                       Switch *sw)
        : Queue(speedFromGbps(800), Packet::data_packet_size() * 2000,
                eventlist, NULL),
          VirtualQueue(), _state_recv(READY) {
    assert(_high_threshold > 0);
    assert(_high_threshold > _low_threshold);

    stringstream ss;
    ss << "VirtualQueue(" << peer->_name << ")";
    _nodename = ss.str();
    _remoteEndpoint = peer;
    _switch = sw;

    assert(_switch);

    peer->setRemoteEndpoint(this);
}

void LosslessInputQueue::receivePacket(Packet &pkt) {
    /* normal packet, enqueue it */
    _queuesize += pkt.size();

    if (COLLECT_DATA) {
        if (_queuesize != 0) {
            std::string file_name =
                    PROJECT_ROOT_PATH /
                    ("sim/output/queue/queue" + _nodename + ".txt");
            std::ofstream MyFile(file_name, std::ios_base::app);
            printf("Bit rate is %lu\n", _bitrate);
            fflush(stdout);
            MyFile << eventlist().now() / 1000 << ","
                   << int(_queuesize * 8 / (_bitrate / 1e9)) / (1) << "," << 1
                   << "," << 1 << std::endl;

            MyFile.close();
        }
    }

    /*printf("LosslessInputQueue received (%s / %d) : %d %d %d - Time %ld\n",
           _nodename.c_str(), pkt.from, _queuesize, _high_threshold,
           _state_recv, eventlist().now() / 1000);*/
    // send PAUSE notifications if that is the case!
    assert(_queuesize > 0);
    if ((uint64_t)_queuesize > _high_threshold) {
        if (_mark_pfc_amount == -1) {
            // pkt.pfc_just_happened = true;
            printf("Marking packet PFC %lu - %d@%d\n", eventlist().now() / 1000,
                   pkt.from, pkt.id());
        }
        if ((uint64_t)_queuesize > _high_threshold && _state_recv != PAUSED) {
            if (_mark_pfc_amount == -1 || _mark_pfc_amount == 1) {
                // pkt.pfc_just_happened = true;
            }
            _state_recv = PAUSED;
            printf("\nPFC NOW! I am %s (%d) - Remote EndPoint is %s - Time "
                   "%lu\n",
                   nodename().c_str(), 1,
                   getRemoteEndpoint()->nodename().c_str(),
                   eventlist().now() / 1000);
            sendPause(1000);
        }
    }

    // if (_state_recv==PAUSED)
    // cout << timeAsMs(eventlist().now()) << " queue " << _name << " switch ("
    // << _switch->_name << ") "<< " recv when paused pkt " << pkt.type() << "
    // sz " << _queuesize << endl;

    /*printf("IN: MaxQueueSize %d - CurrentSize %d - PFC High %d - PFC Low
       %d\n", _maxsize, _queuesize, _high_threshold, _low_threshold);*/

    if (_queuesize > _maxsize) {
        cout << " Queue " << _name
             << " LOSSLESS not working! I should have dropped this packet"
             << _queuesize / Packet::data_packet_size() << endl;
    }

    // tell the output queue we're here!
    if (pkt.nexthop() < pkt.route()->size()) {
        // this should not work...
        // assert(0);
        pkt.sendOn2(this);
    } else {
        assert(_switch);
        pkt.set_ingress_queue(this);
        _switch->receivePacket(pkt);
    }
}

bool LosslessInputQueue::decide_ECN() {
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

void LosslessInputQueue::completedService(Packet &pkt) {
    _queuesize -= pkt.size();

    if (decide_ECN()) {
        // pkt.set_flags(pkt.flags() | ECN_CE);
    }

    // unblock if that is the case
    assert(_queuesize >= 0);
    if ((uint64_t)_queuesize < _low_threshold && _state_recv == PAUSED) {
        _state_recv = READY;
        sendPause(0);
        printf("\nPFC UNLOCK! I am %s (%d) - Remote EndPoint is %s - Time "
               "%lu\n",
               nodename().c_str(), 1, getRemoteEndpoint()->nodename().c_str(),
               eventlist().now() / 1000);
    }
}

void LosslessInputQueue::sendPause(unsigned int wait) {
    // cout << "Ingress link " << getRemoteEndpoint() << " PAUSE " << wait <<
    // endl;
    uint32_t switchID = 0;
    if (_switch) {
        switchID = getSwitch()->getID();
    } else {
        printf("Possible error\n");
    }

    pfc_happened = true;
    EthPausePacket *pkt = EthPausePacket::newpkt(wait, switchID);
    getRemoteEndpoint()->receivePacket(*pkt);
};
