// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "dragonfly_switch.h"
#include "string.h"
#include <stdlib.h>
#include <sstream>
#include <vector>
#include <algorithm>

#include "callback_pipe.h"
#include "dragonfly_topology.h"
#include "queue_lossless.h"
#include "queue_lossless_output.h"
#include "routetable.h"

#include "compositequeue.h"
#include "compositequeuebts.h"
#include "ecnqueue.h"
#include "main.h"
#include "prioqueue.h"
#include "queue.h"
#include "queue_lossless_input.h"
#include "swift_scheduler.h"
#include <iostream>

#define HOST_TOR(src) (src / _p);
#define HOST_GROUP(src) (src / (_a * _p));

// RoundTrip Time.
extern uint32_t RTT;

// Convertes: Number (double) TO Ascii (string); Integer (uint64_t) TO Ascii (string).
string ntoa(double n);
string itoa(uint64_t n);

int DragonflySwitch::precision_ts = 1;

DragonflySwitch::DragonflySwitch(EventList &eventlist, string s, switch_type t,
    uint32_t id, simtime_picosec delay, DragonflyTopology *dt) : Switch(eventlist, s) {
    _id = id;
    _type = t;
    _pipe = new CallbackPipe(delay, eventlist, this);
    _uproutes = NULL;
    _dt = dt;
    _crt_route = 0;
    _hash_salt = random();
    _last_choice = eventlist.now();
    _fib = new RouteTable();
}

void DragonflySwitch::receivePacket(Packet &pkt) {

    if (pkt.type() == ETH_PAUSE) {
        EthPausePacket *p = (EthPausePacket *)&pkt;
        // I must be in lossless mode!
        // find the egress queue that should process this, and pass it over for
        // processing.
        printf("Received a Pause Packet\n");
        for (size_t i = 0; i < _ports.size(); i++) {
            LosslessQueue *q = (LosslessQueue *)_ports.at(i);
            if (q->getRemoteEndpoint() &&
                ((Switch *)q->getRemoteEndpoint())->getID() == p->senderID()) {
                q->receivePacket(pkt);
                break;
            }
        }

        return;
    }

    // printf("Node %s - Packet Received\n", nodename().c_str());

    // printf("Packet Destination is 3 %d\n", pkt.dst());

    if (_packets.find(&pkt) == _packets.end()) {
        // ingress pipeline processing.

        _packets[&pkt] = true;

        const Route *nh = getNextHop(pkt, NULL);
        // set next hop which is peer switch.
        pkt.set_route(*nh);

        // emulate the switching latency between ingress and packet arriving at
        // the egress queue.
        _pipe->receivePacket(pkt);
    } else {
        _packets.erase(&pkt);

        // egress queue processing.
        // cout << "Switch type " << _type <<  " id " << _id << " pkt dst " <<
        // pkt.dst() << " dir " << pkt.get_direction() << endl;

        pkt.hop_count++;

        /*printf("At %s - Hop %d - Time %lu\n", nodename().c_str(),
           pkt.hop_count, GLOBAL_TIME);*/

        if ((pkt.hop_count == 1) &&
            (pkt.type() == UEC || pkt.type() == NDP ||
             pkt.type() == SWIFTTRIMMING || pkt.type() == UEC_DROP)) {
            pkt.set_ts(GLOBAL_TIME -
                       (SINGLE_PKT_TRASMISSION_TIME_MODERN * 1000) -
                       (LINK_DELAY_MODERN * 1000));
            simtime_picosec my_time =
                    GLOBAL_TIME - (SINGLE_PKT_TRASMISSION_TIME_MODERN * 1000) -
                    (LINK_DELAY_MODERN * 1000);

            if (precision_ts == 1) {
                pkt.set_ts(my_time);
            } else {
                pkt.set_ts(((my_time + precision_ts - 1) / precision_ts) *
                           precision_ts);
            }

            if (COLLECT_DATA) {
                // Sent
                std::string file_name = PROJECT_ROOT_PATH /
                                        ("sim/output/sent/s" +
                                         std::to_string(this->from) + ".txt ");
                std::ofstream MyFile(file_name, std::ios_base::app);

                MyFile << (GLOBAL_TIME - 70000000) / 1000 << "," << 1
                       << std::endl;

                MyFile.close();
            }

            /*printf("ID %d - Hop %d - Previous time %lu - New time %lu - "
                   "%lu %lu - Name %s\n",
                   pkt.id(), 1, 0, GLOBAL_TIME,
                   SINGLE_PKT_TRASMISSION_TIME_MODERN,
                   GLOBAL_TIME - (SINGLE_PKT_TRASMISSION_TIME_MODERN * 1000) -
                           (LINK_DELAY_MODERN * 1000),
                   nodename().c_str());*/
            /*printf("From %d - Switch %s - Hop %d - %d\n", pkt.from,
                   nodename().c_str(), pkt.hop_count, GLOBAL_TIME / 1000);*/
        }

        /*if ((pkt.hop_count == 2) && (pkt.type() == UEC || pkt.type() == NDP))
        { printf("From %d - Switch %s - Hop %d - %d\n", pkt.from,
                   nodename().c_str(), pkt.hop_count, GLOBAL_TIME / 1000);
        }*/

        printf("SendOn.\n");
        pkt.sendOn();
    }
};

// !!!
void DragonflySwitch::addHostPort(int addr, int flowid, PacketSink *transport) {
    Route *rt = new Route();
    rt->push_back(_dt->queues_host_switch[_dt->HOST_TOR_FKT(addr)][addr]);
    rt->push_back(_dt->pipes_host_switch[_dt->HOST_TOR_FKT(addr)][addr]);
    rt->push_back(transport);
    _fib->addHostRoute(addr, rt, flowid);
}

void DragonflySwitch::permute_paths(vector<FibEntry *> *routes) {
    int len = routes->size();
    for (int i = 0; i < len; i++) {
        int ix = random() % (len - i);
        FibEntry *tmppath = (*routes)[ix];
        (*routes)[ix] = (*routes)[len - 1 - i];
        (*routes)[len - 1 - i] = tmppath;
    }
}

DragonflySwitch::routing_strategy DragonflySwitch::_strategy = DragonflySwitch::NIX;
uint16_t DragonflySwitch::_ar_fraction = 0;
uint16_t DragonflySwitch::_ar_sticky = DragonflySwitch::PER_PACKET;
simtime_picosec DragonflySwitch::_sticky_delta = timeFromUs((uint32_t)10);
double DragonflySwitch::_ecn_threshold_fraction = 1.0;


Route *DragonflySwitch::getNextHop(Packet &pkt, BaseQueue *ingress_port) {
    // Retrieve available routes for the destination from the Forwarding Information Base (FIB)
    vector<FibEntry *> *available_hops = _fib->getRoutes(pkt.dst());

    if (available_hops) {
        uint32_t ecmp_choice = 0;

        if (available_hops->size() > 1){
            switch (_strategy) {
            case NIX:
                abort();
            case MINIMAL:
                sort(available_hops->begin(), available_hops->end(), [](FibEntry *a, FibEntry *b) {
                    return a->getCost() < b->getCost();});

            case VALIANTS:
                if(pkt.hop_count == 0){
                    permute_paths(available_hops);
                }
                else{
                    set_strategy(MINIMAL);
                    return getNextHop(pkt, ingress_port);
                }
            }
        }

        FibEntry *e = (*available_hops)[ecmp_choice];
        pkt.set_direction(e->getDirection());

        return e->getEgressPort();
    }

    assert(_fib->getRoutes(pkt.dst()));

    return getNextHop(pkt, ingress_port);
};
