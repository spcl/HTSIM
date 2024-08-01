// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "b-cube_switch.h"
#include "string.h"
#include <stdlib.h>
#include <sstream>
#include <vector>
#include <algorithm>

#include "callback_pipe.h"
#include "b-cube_topology.h"
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

// RoundTrip Time.
extern uint32_t RTT;

// Convertes: Number (double) TO Ascii (string); Integer (uint64_t) TO Ascii (string).
string ntoa(double n);
string itoa(uint64_t n);

int BCubeSwitch::precision_ts = 1;

BCubeSwitch::BCubeSwitch(EventList &eventlist, string s, switch_type t,
    uint32_t id, simtime_picosec delay, BCubeTopology *bt) : Switch(eventlist, s) {
    _id = id;
    _type = t;
    _pipe = new CallbackPipe(delay, eventlist, this);
    _bt = bt;
    _crt_route = 0;
    _hash_salt = random();
    _last_choice = eventlist.now();
    _fib = new RouteTable();
}

BCubeSwitch::BCubeSwitch(EventList &eventlist, string s, switch_type t,
    uint32_t id, simtime_picosec delay, BCubeTopology *bt, uint32_t strat) : Switch(eventlist, s) {
    _id = id;
    _type = t;
    _pipe = new CallbackPipe(delay, eventlist, this);
    _bt = bt;
    _crt_route = 0;
    _hash_salt = random();
    _last_choice = eventlist.now();
    _fib = new RouteTable();
    _bc_strategy = (routing_strategy)strat;
}

void BCubeSwitch::receivePacket(Packet &pkt) {

    if (pkt.type() == ETH_PAUSE) {
        EthPausePacket *p = (EthPausePacket *)&pkt;
        // I must be in lossless mode!
        // find the egress queue that should process this, and pass it over for
        // processing.
        // printf("Received a Pause Packet\n");
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

        /* if(pkt.is_ack){
            printf("receivePacket: ACK_Packet with pkt_src %d dst %d arrived at %d.\n", pkt.to, pkt.dst(), _id);
        }else{
            printf("receivePacket: Packet with pkt_src %d dst %d arrived at %d.\n", pkt.from, pkt.dst(), _id);
        } */

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

        /* printf("At %s - Hop %d - Time %lu\n", nodename().c_str(),
           pkt.hop_count, GLOBAL_TIME); */

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

        //printf("receivePacket: SendOn.\n");
        pkt.sendOn();
    }
};

// Creates the connection between the switches and nodes.
void BCubeSwitch::addHostPort(int addr, uint32_t flowid, PacketSink *transport) {
    Route *rt = new Route();
    rt->push_back(_bt->queues_switch_host[addr][addr]);
    rt->push_back(_bt->pipes_switch_host[addr][addr]);
    rt->push_back(transport);
    _fib->addHostRoute(addr, rt, flowid);
}

// Permutes the availeble entries to be able to pick a random path.
void BCubeSwitch::permute_paths(vector<FibEntry *> *routes) {
    int len = routes->size();
    for (int i = 0; i < len; i++) {
        int ix = random() % (len - i);
        FibEntry *tmppath = (*routes)[ix];
        (*routes)[ix] = (*routes)[len - 1 - i];
        (*routes)[len - 1 - i] = tmppath;
    }
}

BCubeSwitch::routing_strategy BCubeSwitch::_bc_strategy = BCubeSwitch::NIX;
uint16_t BCubeSwitch::_ar_fraction = 0;
uint16_t BCubeSwitch::_ar_sticky = BCubeSwitch::PER_PACKET;
simtime_picosec BCubeSwitch::_sticky_delta = timeFromUs((uint32_t)10);
double BCubeSwitch::_ecn_threshold_fraction = 1.0;

// When in a switch: this function is called to determine to which switch go next.
Route *BCubeSwitch::getNextHop(Packet &pkt, BaseQueue *ingress_port) {
    // Retrieve available routes for the destination from the Forwarding Information Base (FIB)
    vector<FibEntry *> *available_hops = _fib->getRoutes(pkt.dst());
    if (available_hops) {
        // Here possibly taking something from the cache.
        uint32_t ecmp_choice = 0;

        if (available_hops->size() > 1){
            switch (_bc_strategy) {
                case NIX: {
                    abort();
                    break;
                }
                case MINIMAL: {
                    permute_paths(available_hops);
                    break;
                }
            }
        }
        FibEntry *e = (*available_hops)[ecmp_choice];
        pkt.set_direction(e->getDirection());

        return e->getEgressPort();
    }
    else{
        // Here building the routes for the first time.
        switch (_bc_strategy) {
            case NIX: {
                abort();
                break;
            }
            case MINIMAL: {
                Route *r = new Route();
                uint32_t n = _bt->get_n();
                uint32_t k = _bt->get_k();

                if (_id == pkt.dst()){ // Go to the node destination node in the next hop.
                    HostFibEntry *fe = _fib->getHostRoute(pkt.dst(), pkt.flow_id());
                    assert(fe);
                    pkt.set_direction(DOWN);
                    return fe->getEgressPort();
                }
                else{
                    uint32_t no_nodes = _bt->get_no_of_nodes();
                    uint32_t switches_per_layer = (uint32_t)pow(n, k-1);

                    if (_id < no_nodes){
                        for (uint32_t i = 0; i < k; i++){
                            uint32_t layer_pow = (uint32_t)pow(n, i);
                            uint32_t layer_id = (_id / layer_pow) % n;
                            uint32_t dst_layer_id = (pkt.dst() / layer_pow) % n;
                            if(layer_id != dst_layer_id){
                                uint32_t hop_layer_id = ((_id / (layer_pow * n)) * layer_pow) + (_id % layer_pow);
                                uint32_t next_node = no_nodes + i * switches_per_layer + hop_layer_id;

                                r->push_back(_bt->queues_switch_switch[_id][next_node]);
                                r->push_back(_bt->pipes_switch_switch[_id][next_node]);
                                r->push_back(_bt->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                        }
                    }
                    else{
                        uint32_t switch_id = _id - (uint32_t)pow(n, k);
                        uint32_t layer = switch_id / switches_per_layer;
                        uint32_t layer_id = switch_id % switches_per_layer;
                        uint32_t layer_pow = (uint32_t)pow(n, layer);
                        uint32_t corr_digit = (pkt.dst() / layer_pow) % n;

                        uint32_t hop_offset = (layer_id / layer_pow) * layer_pow * n + (layer_id % layer_pow);
                        uint32_t next_node = corr_digit * layer_pow + hop_offset;
                        
                        r->push_back(_bt->queues_switch_switch[_id][next_node]);
                        r->push_back(_bt->pipes_switch_switch[_id][next_node]);
                        r->push_back(_bt->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                    }
                }
                break;
            }
        }

        assert(_fib->getRoutes(pkt.dst()));

        return getNextHop(pkt, ingress_port);
    }
};
