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
    _dt = dt;
    _crt_route = 0;
    _hash_salt = random();
    _last_choice = eventlist.now();
    _fib = new RouteTable();
}

DragonflySwitch::DragonflySwitch(EventList &eventlist, string s, switch_type t,
    uint32_t id, simtime_picosec delay, DragonflyTopology *dt, uint32_t strat) : Switch(eventlist, s) {
    _id = id;
    _type = t;
    _pipe = new CallbackPipe(delay, eventlist, this);
    _dt = dt;
    _crt_route = 0;
    _hash_salt = random();
    _last_choice = eventlist.now();
    _fib = new RouteTable();
    _df_strategy = (routing_strategy)strat;
    // if(!_fib == NULL) printf("_fib is initialized.\n");
    // if(!_dt == NULL) printf("_dt is initialized.\n");
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
        printf("receivePacket: Packet arrived at %d.\n", _id);
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

// !!!
void DragonflySwitch::addHostPort(int addr, uint32_t flowid, PacketSink *transport) {
    Route *rt = new Route();
    uint32_t hostTorAddr = _dt->HOST_TOR_FKT(addr);
    printf("addHostPort: %d.\n", addr);
    rt->push_back(_dt->queues_switch_host[hostTorAddr][addr]);
    rt->push_back(_dt->pipes_switch_host[hostTorAddr][addr]);
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

DragonflySwitch::routing_strategy DragonflySwitch::_df_strategy = DragonflySwitch::NIX;
uint16_t DragonflySwitch::_ar_fraction = 0;
uint16_t DragonflySwitch::_ar_sticky = DragonflySwitch::PER_PACKET;
simtime_picosec DragonflySwitch::_sticky_delta = timeFromUs((uint32_t)10);
double DragonflySwitch::_ecn_threshold_fraction = 1.0;



Route *DragonflySwitch::getNextHop(Packet &pkt, BaseQueue *ingress_port) {
    // Retrieve available routes for the destination from the Forwarding Information Base (FIB)
    vector<FibEntry *> *available_hops = _fib->getRoutes(pkt.dst());

    // Could it be that "if (available_hops)" and "if (available_hops->size() > 1)" do the same?
    if (available_hops) {
        // Here possibly taking something from the cache.
        uint32_t ecmp_choice = 0;

        if (available_hops->size() > 1){
            switch (_df_strategy) {
                case NIX: {
                    abort();
                    break;
                }
                case MINIMAL: {
                    printf("Minimal.\n");
                    /* sort(available_hops->begin(), available_hops->end(), [](FibEntry *a, FibEntry *b) {
                        return a->getCost() < b->getCost();}); */
                    break;
                }
                case VALIANTS: {
                    // We should never go in here because we won't add an entry into _fib.
                    abort();
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
        switch (_df_strategy) {
            case NIX: {
                abort();
                break;
            }
            case MINIMAL: {
                Route *r = new Route();

                uint32_t _a = _dt->get_group_size();
                uint32_t _h = _dt->get_no_global_links();
                uint32_t noGroups = _dt->get_no_groups();

                uint32_t dst_switch = _dt->HOST_TOR_FKT(pkt.dst());

                uint32_t src_group = _id / _a;
                uint32_t dst_group = dst_switch / _a;

                // printf("getNextHop: src_group = %d\n", src_group);
                // printf("getNextHop: dst_group = %d\n", dst_group);

                uint32_t src_intra_group_id = _id % _a;
                uint32_t dst_intra_group_id = dst_switch % _a;

                if (_id == dst_switch){
                    // printf("Exit.\n");
                    HostFibEntry *fe = _fib->getHostRoute(pkt.dst(), pkt.flow_id());
                    assert(fe);
                    pkt.set_direction(DOWN);
                    return fe->getEgressPort();
                }
                else if (src_group == dst_group){
                    r->push_back(_dt->queues_switch_switch[_id][dst_switch]);
                    r->push_back(_dt->pipes_switch_switch[_id][dst_switch]);
                    r->push_back(_dt->queues_switch_switch[_id][dst_switch]->getRemoteEndpoint());
                    // printf("getNextHop: routeBack_size = %ld\n", r->size());
                    _fib->addRoute(pkt.dst(), r, 1, DOWN);
                }
                else{
                    uint32_t intra_group_connecting_node_id;
                    if(dst_group > src_group){
                        intra_group_connecting_node_id = (dst_group - 1) / _h;
                    }
                    else{
                        intra_group_connecting_node_id = dst_group / _h;
                    }
                    uint32_t group_connecting_node_id = src_group * _a + intra_group_connecting_node_id;

                    if(_id == group_connecting_node_id){
                        // In the topology it holds the following:
                        // The first _a + 1 nodes connect to the first node of other groups.
                        // The second _a + 1 nodes connect to the second node of other groups.
                        // And so on.

                        uint32_t dst_group_connecting_node_id = (dst_group * _a) + (_id / noGroups);
                        r->push_back(_dt->queues_switch_switch[_id][dst_group_connecting_node_id]);
                        r->push_back(_dt->pipes_switch_switch[_id][dst_group_connecting_node_id]);
                        r->push_back(_dt->queues_switch_switch[_id][dst_group_connecting_node_id]->getRemoteEndpoint());
                        // printf("getNextHop: routeBack_size = %ld\n", r->size());
                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                    }
                    else{
                        r->push_back(_dt->queues_switch_switch[_id][group_connecting_node_id]);
                        r->push_back(_dt->pipes_switch_switch[_id][group_connecting_node_id]);
                        r->push_back(_dt->queues_switch_switch[_id][group_connecting_node_id]->getRemoteEndpoint());
                        // printf("getNextHop: routeBack_size = %ld\n", r->size());
                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                    }
                }
                break;
            }
            case VALIANTS:{
                Route *r = new Route();
                FibEntry *e;

                uint32_t _a = _dt->get_group_size();
                uint32_t _h = _dt->get_no_global_links();
                uint32_t noGroups = _dt->get_no_groups();

                uint32_t dst_switch = _dt->HOST_TOR_FKT(pkt.dst());
                uint32_t pkt_src_switch;
                if(pkt.is_ack){
                    pkt_src_switch = _dt->HOST_TOR_FKT(pkt.to);
                }
                else{
                    pkt_src_switch = _dt->HOST_TOR_FKT(pkt.from);
                }

                //printf("src_switch = %d.\n", pkt_src_switch);

                uint32_t src_group = _id / _a;
                uint32_t dst_group = dst_switch / _a;
                uint32_t pkt_src_group = pkt_src_switch / _a;

                // printf("getNextHop: src_group = %d\n", src_group);
                // printf("getNextHop: dst_group = %d\n", dst_group);

                uint32_t src_intra_group_id = _id % _a;
                uint32_t dst_intra_group_id = dst_switch % _a;

                if (src_group == pkt_src_group && src_group != dst_group){
                    if(_id == pkt_src_switch){

                        uint32_t random_intra_group_node = random() % (_a);
                        uint32_t random_node_id = src_group * _a + random_intra_group_node;

                        if(random_node_id == _id){
                            int random_connection = random() % _h;
                            int next_group_id = src_intra_group_id * _h + random_connection;
                            if(next_group_id >= src_group){
                                next_group_id++;
                            }
                            uint32_t next_group_connecting_node_id = (next_group_id * _a) + (_id / noGroups);
                            r->push_back(_dt->queues_switch_switch[_id][next_group_connecting_node_id]);
                            r->push_back(_dt->pipes_switch_switch[_id][next_group_connecting_node_id]);
                            r->push_back(_dt->queues_switch_switch[_id][next_group_connecting_node_id]->getRemoteEndpoint());
                            e = new FibEntry(r, 1, DOWN);
                        }
                        else{
                            r->push_back(_dt->queues_switch_switch[_id][random_node_id]);
                            r->push_back(_dt->pipes_switch_switch[_id][random_node_id]);
                            r->push_back(_dt->queues_switch_switch[_id][random_node_id]->getRemoteEndpoint());
                            e = new FibEntry(r, 1, DOWN);
                        }
                    }
                    else{
                        int random_connection = random() % _h;
                        int next_group_id = src_intra_group_id * _h + random_connection;
                        if(next_group_id >= src_group){
                            next_group_id++;
                        }
                        uint32_t next_group_connecting_node_id = (next_group_id * _a) + (_id / noGroups);
                        r->push_back(_dt->queues_switch_switch[_id][next_group_connecting_node_id]);
                        r->push_back(_dt->pipes_switch_switch[_id][next_group_connecting_node_id]);
                        r->push_back(_dt->queues_switch_switch[_id][next_group_connecting_node_id]->getRemoteEndpoint());
                        e = new FibEntry(r, 1, DOWN);
                    }
                }
                else{
                    if (_id == dst_switch){
                        //printf("Exit.\n");
                        HostFibEntry *fe = _fib->getHostRoute(pkt.dst(), pkt.flow_id());
                        assert(fe);
                        pkt.set_direction(DOWN);
                        return fe->getEgressPort();
                    }
                    else if (src_group == dst_group){
                        r->push_back(_dt->queues_switch_switch[_id][dst_switch]);
                        r->push_back(_dt->pipes_switch_switch[_id][dst_switch]);
                        r->push_back(_dt->queues_switch_switch[_id][dst_switch]->getRemoteEndpoint());
                        // printf("getNextHop: routeBack_size = %ld\n", r->size());
                        e = new FibEntry(r, 1, DOWN);
                    }
                    else{
                        uint32_t intra_group_connecting_node_id;
                        if(dst_group > src_group){
                            intra_group_connecting_node_id = (dst_group - 1) / _h;
                        }
                        else{
                            intra_group_connecting_node_id = dst_group / _h;
                        }
                        uint32_t group_connecting_node_id = src_group * _a + intra_group_connecting_node_id;

                        if(_id == group_connecting_node_id){
                            // In the topology it holds the following:
                            // The first _a + 1 nodes connect to the first node of other groups.
                            // The second _a + 1 nodes connect to the second node of other groups.
                            // And so on.

                            uint32_t dst_group_connecting_node_id = (dst_group * _a) + (_id / noGroups);
                            r->push_back(_dt->queues_switch_switch[_id][dst_group_connecting_node_id]);
                            r->push_back(_dt->pipes_switch_switch[_id][dst_group_connecting_node_id]);
                            r->push_back(_dt->queues_switch_switch[_id][dst_group_connecting_node_id]->getRemoteEndpoint());
                            // printf("getNextHop: routeBack_size = %ld\n", r->size());
                            e = new FibEntry(r, 1, DOWN);
                        }
                        else{
                            r->push_back(_dt->queues_switch_switch[_id][group_connecting_node_id]);
                            r->push_back(_dt->pipes_switch_switch[_id][group_connecting_node_id]);
                            r->push_back(_dt->queues_switch_switch[_id][group_connecting_node_id]->getRemoteEndpoint());
                            // printf("getNextHop: routeBack_size = %ld\n", r->size());
                            e = new FibEntry(r, 1, DOWN);
                        }
                    }
                }
                pkt.set_direction(e->getDirection());
                return e->getEgressPort();
                break;
            }
        }

        assert(_fib->getRoutes(pkt.dst()));

        return getNextHop(pkt, ingress_port);
    }
};
