// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "slimfly_switch.h"
#include "string.h"
#include <stdlib.h>
#include <sstream>
#include <vector>
#include <algorithm>

#include "callback_pipe.h"
#include "slimfly_topology.h"
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

int SlimflySwitch::precision_ts = 1;

SlimflySwitch::SlimflySwitch(EventList &eventlist, string s, switch_type t,
    uint32_t id, simtime_picosec delay, SlimflyTopology *st) : Switch(eventlist, s) {
    _id = id;
    _type = t;
    _pipe = new CallbackPipe(delay, eventlist, this);
    _st = st;
    _crt_route = 0;
    _hash_salt = random();
    _last_choice = eventlist.now();
    _fib = new RouteTable();
}

SlimflySwitch::SlimflySwitch(EventList &eventlist, string s, switch_type t,
    uint32_t id, simtime_picosec delay, SlimflyTopology *st, uint32_t strat) : Switch(eventlist, s) {
    _id = id;
    _type = t;
    _pipe = new CallbackPipe(delay, eventlist, this);
    _st = st;
    _crt_route = 0;
    _hash_salt = random();
    _last_choice = eventlist.now();
    _fib = new RouteTable();
    _sf_strategy = (routing_strategy)strat;
    // if(!_fib == NULL) printf("_fib is initialized.\n");
    // if(!_st == NULL) printf("_st is initialized.\n");
}

void SlimflySwitch::receivePacket(Packet &pkt) {

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

// !!!
void SlimflySwitch::addHostPort(int addr, uint32_t flowid, PacketSink *transport) {
    Route *rt = new Route();
    uint32_t hostTorAddr = _st->HOST_TOR_FKT(addr);
    printf("addHostPort: %d.\n", addr);
    rt->push_back(_st->queues_switch_host[hostTorAddr][addr]);
    rt->push_back(_st->pipes_switch_host[hostTorAddr][addr]);
    rt->push_back(transport);
    _fib->addHostRoute(addr, rt, flowid);
}

void SlimflySwitch::permute_paths(vector<FibEntry *> *routes) {
    int len = routes->size();
    for (int i = 0; i < len; i++) {
        int ix = random() % (len - i);
        FibEntry *tmppath = (*routes)[ix];
        (*routes)[ix] = (*routes)[len - 1 - i];
        (*routes)[len - 1 - i] = tmppath;
    }
}

SlimflySwitch::routing_strategy SlimflySwitch::_sf_strategy = SlimflySwitch::NIX;
uint16_t SlimflySwitch::_ar_fraction = 0;
uint16_t SlimflySwitch::_ar_sticky = SlimflySwitch::PER_PACKET;
simtime_picosec SlimflySwitch::_sticky_delta = timeFromUs((uint32_t)10);
double SlimflySwitch::_ecn_threshold_fraction = 1.0;

int SlimflySwitch::modulo (int x, int y){
    int res = x % y;
    if (res < 0){
        res += y;
    }
    return res;
}

Route *SlimflySwitch::getNextHop(Packet &pkt, BaseQueue *ingress_port) {
    // Retrieve available routes for the destination from the Forwarding Information Base (FIB)
    vector<FibEntry *> *available_hops = _fib->getRoutes(pkt.dst());

    // Could it be that "if (available_hops)" and "if (available_hops->size() > 1)" do the same?
    if (available_hops) {
        // Here possibly taking something from the cache.
        uint32_t ecmp_choice = 0;

        if (available_hops->size() > 1){
            //printf("access: _sf_strategy = %u\n", _sf_strategy);
            switch (_sf_strategy) {
                case NIX: {
                    //printf("access: nix: _sf_strategy = %u\n", _sf_strategy);
                    abort();
                    break;
                }
                case MINIMAL: {
                    //printf("access: minimal: _sf_strategy = %u\n", _sf_strategy);
                    /* sort(available_hops->begin(), available_hops->end(), [](FibEntry *a, FibEntry *b) {
                        return a->getCost() < b->getCost();}); */
                    permute_paths(available_hops);
                    break;
                }
                case VALIANTS: {
                    //printf("access: valiants: _sf_strategy = %u\n", _sf_strategy);
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
        switch (_sf_strategy) {
            case NIX: {
                //printf("build: nix: _sf_strategy = %u\n", _sf_strategy);
                abort();
                break;
            }
            case MINIMAL: {
                //printf("build: minimal: _sf_strategy = %u\n", _sf_strategy);
                Route *r = new Route();

                uint32_t dst_switch = _st->HOST_TOR_FKT(pkt.dst());

                int q = (int)_st->get_q();
                uint32_t q2 = q * q;

                int u, v, w, x, y, z;

                if(_id < q2){
                    u = _id / q;
                    v = modulo(_id, q);
                    w = 0;
                }
                else{
                    u = (_id - q2) / q;
                    v = modulo((_id - q2), q);
                    w = 1;
                }

                if(dst_switch < q2){
                    x = dst_switch / q;
                    y = modulo(dst_switch, q);
                    z = 0;
                }
                else{
                    x = (dst_switch - q2) / q;
                    y = modulo((dst_switch - q2), q);
                    z = 1;
                }

                printf("u = %d,\tv = %d,\tw = %d,\tx = %d,\ty = %d,\tz = %d,\n", u, v, w, x, y, z);
                printf("getNextHop: src_block = %d;\tsrc_group = %d\tsrc_switch = %d\n", w, u, _id);
                printf("\t\t\tdst_block = %d\tdst_group = %d\tdst_switch = %d\n", z, x, dst_switch);
                printf("\t\t\tpacket_size = %d\n", pkt.size());
                printf("\t\t\tmod = %d\n", modulo((y - v), q));

                if (_id == dst_switch){
                    // printf("Exit.\n");
                    HostFibEntry *fe = _fib->getHostRoute(pkt.dst(), pkt.flow_id());
                    assert(fe);
                    pkt.set_direction(DOWN);
                    return fe->getEgressPort();
                }
                else if (w == z){
                    if(u == x){ // w == z and u == x
                    printf("Hoi.\n");
                        if(w == 0 && _st->is_element(_st->get_X(), modulo((y - v), q))){
                            printf("Ciao.\n");
                            r->push_back(_st->queues_switch_switch[_id][dst_switch]);
                            r->push_back(_st->pipes_switch_switch[_id][dst_switch]);
                            r->push_back(_st->queues_switch_switch[_id][dst_switch]->getRemoteEndpoint());
                            // printf("getNextHop: routeBack_size = %ld\n", r->size());
                            _fib->addRoute(pkt.dst(), r, 1, DOWN);
                        }
                        else if(w == 1 && _st->is_element(_st->get_Xp(), modulo((y - v), q))){
                            r->push_back(_st->queues_switch_switch[_id][dst_switch]);
                            r->push_back(_st->pipes_switch_switch[_id][dst_switch]);
                            r->push_back(_st->queues_switch_switch[_id][dst_switch]->getRemoteEndpoint());
                            // printf("getNextHop: routeBack_size = %ld\n", r->size());
                            _fib->addRoute(pkt.dst(), r, 1, DOWN);
                        }
                        else{ // same group but 2 hops needed
                            printf("Tschüss.\n");
                            uint32_t exists_hop = 0;
                            vector<uint32_t> X;
                            if(w == 0){
                                X = _st->get_X();
                            }
                            else{
                                X = _st->get_Xp();
                            }
                            int size = X.size();
                            uint32_t diff = modulo((y - v), q);
                            
                            for (int i = 0; i < size; i++){
                                for (int j = i+1; j < size; j++){
                                    if (modulo((X[i] + X[j]), q) == diff){
                                        int next_i = u * q + modulo((v + X[i]), q);
                                        int next_j = u * q + modulo((v + X[j]), q);
                                        if(w == 1){
                                            next_i += q2;
                                            next_j += q2;
                                        }
                                        r->push_back(_st->queues_switch_switch[_id][next_i]);
                                        r->push_back(_st->pipes_switch_switch[_id][next_i]);
                                        r->push_back(_st->queues_switch_switch[_id][next_i]->getRemoteEndpoint());
                                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                                        r->push_back(_st->queues_switch_switch[_id][next_j]);
                                        r->push_back(_st->pipes_switch_switch[_id][next_j]);
                                        r->push_back(_st->queues_switch_switch[_id][next_j]->getRemoteEndpoint());
                                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                                        exists_hop += 2;
                                    }
                                    else if (modulo((X[i] - X[j]), q) == diff){
                                        int next_i = u * q + modulo((v + X[i]), q);
                                        int next_j = u * q + modulo((v - X[j]), q);
                                        if(w == 1){
                                            next_i += q2;
                                            next_j += q2;
                                        }
                                        r->push_back(_st->queues_switch_switch[_id][next_i]);
                                        r->push_back(_st->pipes_switch_switch[_id][next_i]);
                                        r->push_back(_st->queues_switch_switch[_id][next_i]->getRemoteEndpoint());
                                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                                        r->push_back(_st->queues_switch_switch[_id][next_j]);
                                        r->push_back(_st->pipes_switch_switch[_id][next_j]);
                                        r->push_back(_st->queues_switch_switch[_id][next_j]->getRemoteEndpoint());
                                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                                        exists_hop += 2;
                                    }
                                    else if (modulo(( -X[i] + X[j]), q) == diff){
                                        int next_i = u * q + modulo((v - X[i]), q);
                                        int next_j = u * q + modulo((v + X[j]), q);
                                        if(w == 1){
                                            next_i += q2;
                                            next_j += q2;
                                        }
                                        r->push_back(_st->queues_switch_switch[_id][next_i]);
                                        r->push_back(_st->pipes_switch_switch[_id][next_i]);
                                        r->push_back(_st->queues_switch_switch[_id][next_i]->getRemoteEndpoint());
                                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                                        r->push_back(_st->queues_switch_switch[_id][next_j]);
                                        r->push_back(_st->pipes_switch_switch[_id][next_j]);
                                        r->push_back(_st->queues_switch_switch[_id][next_j]->getRemoteEndpoint());
                                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                                        exists_hop += 2;
                                    }
                                    else if (modulo(( -X[i] - X[j]), q) == diff){
                                        int next_i = u * q + modulo((v - X[i]), q);
                                        int next_j = u * q + modulo((v - X[j]), q);
                                        if(w == 1){
                                            next_i += q2;
                                            next_j += q2;
                                        }
                                        r->push_back(_st->queues_switch_switch[_id][next_i]);
                                        r->push_back(_st->pipes_switch_switch[_id][next_i]);
                                        r->push_back(_st->queues_switch_switch[_id][next_i]->getRemoteEndpoint());
                                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                                        r->push_back(_st->queues_switch_switch[_id][next_j]);
                                        r->push_back(_st->pipes_switch_switch[_id][next_j]);
                                        r->push_back(_st->queues_switch_switch[_id][next_j]->getRemoteEndpoint());
                                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                                        exists_hop += 2;
                                    }
                                }
                                if (modulo((2 * X[i]), q) == diff){
                                    int next_i = u * q + modulo((v + X[i]), q);
                                    if(w == 1){
                                        next_i += q2;
                                    }
                                    r->push_back(_st->queues_switch_switch[_id][next_i]);
                                    r->push_back(_st->pipes_switch_switch[_id][next_i]);
                                    r->push_back(_st->queues_switch_switch[_id][next_i]->getRemoteEndpoint());
                                    _fib->addRoute(pkt.dst(), r, 1, DOWN);
                                    exists_hop++;
                                }
                                else if (modulo((2 * (-X[i])), q) == diff){
                                    int next_i = u * q + modulo((v - X[i]), q);
                                    if(w == 1){
                                        next_i += q2;
                                    }
                                    r->push_back(_st->queues_switch_switch[_id][next_i]);
                                    r->push_back(_st->pipes_switch_switch[_id][next_i]);
                                    r->push_back(_st->queues_switch_switch[_id][next_i]->getRemoteEndpoint());
                                    _fib->addRoute(pkt.dst(), r, 1, DOWN);
                                    exists_hop++;
                                }
                            }
                            if (exists_hop == 0){
                                // This should never happen as all elements within a group are connected by 2 hops.
                                abort();
                            }
                        }
                    }
                    else{ //w == z && u != x
                        // w == z and other group: 2 hops through other side
                        uint32_t next_hop;
                        if(w == 0){
                            uint32_t m = modulo(((v-y)/(u-x)), q);
                            uint32_t c = modulo((v - m * u), q);
                            next_hop = q2 + m * q + c;
                        }
                        else{ // w == 1
                            uint32_t m = modulo(((y-v)/(u-x)), q);
                            uint32_t c = modulo((v + m * u), q);
                            next_hop = m * q + c;
                        }
                        r->push_back(_st->queues_switch_switch[_id][next_hop]);
                        r->push_back(_st->pipes_switch_switch[_id][next_hop]);
                        r->push_back(_st->queues_switch_switch[_id][next_hop]->getRemoteEndpoint());
                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                    }
                }
                else{ // w != z
                    if(v == modulo((u * x + y), q) && w == 0){ // directly connected
                        r->push_back(_st->queues_switch_switch[_id][dst_switch]);
                        r->push_back(_st->pipes_switch_switch[_id][dst_switch]);
                        r->push_back(_st->queues_switch_switch[_id][dst_switch]->getRemoteEndpoint());
                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                    }
                    else if(y == modulo((u * x + v), q) && w == 1){ // directly connected
                        r->push_back(_st->queues_switch_switch[_id][dst_switch]);
                        r->push_back(_st->pipes_switch_switch[_id][dst_switch]);
                        r->push_back(_st->queues_switch_switch[_id][dst_switch]->getRemoteEndpoint());
                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                    }
                    else{ // w != z and 2 hops
                        if(w == 0){
                            int yp = modulo(v - u * x, q);
                            int vp = modulo(y + u * x, q);
                            int src_diff = modulo(v - vp, q);
                            int dst_diff = modulo(y - yp, q);
                            if(_st->is_element(_st->get_X(), src_diff)){
                                int next_hop = q * u + vp;
                                r->push_back(_st->queues_switch_switch[_id][next_hop]);
                                r->push_back(_st->pipes_switch_switch[_id][next_hop]);
                                r->push_back(_st->queues_switch_switch[_id][next_hop]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                            if(_st->is_element(_st->get_Xp(), dst_diff)){
                                int next_hop = q2 + q * x + yp;
                                r->push_back(_st->queues_switch_switch[_id][next_hop]);
                                r->push_back(_st->pipes_switch_switch[_id][next_hop]);
                                r->push_back(_st->queues_switch_switch[_id][next_hop]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                        }
                        else{ // w == 1
                            int yp = modulo(v + u * x, q);
                            int vp = modulo(y - u * x, q);
                            int src_diff = modulo(v - vp, q);
                            int dst_diff = modulo(y - yp, q);
                            if(_st->is_element(_st->get_Xp(), src_diff)){
                                int next_hop = q2 + q * u + vp;
                                r->push_back(_st->queues_switch_switch[_id][next_hop]);
                                r->push_back(_st->pipes_switch_switch[_id][next_hop]);
                                r->push_back(_st->queues_switch_switch[_id][next_hop]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                            if(_st->is_element(_st->get_X(), dst_diff)){
                                int next_hop = q * x + yp;
                                r->push_back(_st->queues_switch_switch[_id][next_hop]);
                                r->push_back(_st->pipes_switch_switch[_id][next_hop]);
                                r->push_back(_st->queues_switch_switch[_id][next_hop]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                        }
                    }
                }
                break;
            }
            case VALIANTS:{
                //printf("build:valiants: _sf_strategy = %u\n", _sf_strategy);
                abort();
                /* Route *r = new Route();
                FibEntry *e;

                uint32_t _a = _st->get_group_size();
                uint32_t _h = _st->get_no_global_links();
                uint32_t noGroups = _st->get_no_groups();

                uint32_t dst_switch = _st->HOST_TOR_FKT(pkt.dst());
                uint32_t pkt_src_switch;
                if(pkt.is_ack){
                    pkt_src_switch = _st->HOST_TOR_FKT(pkt.to);
                }
                else{
                    pkt_src_switch = _st->HOST_TOR_FKT(pkt.from);
                }

                //printf("src_switch = %d.\n", pkt_src_switch);

                uint32_t src_group = _id / _a;
                uint32_t dst_group = dst_switch / _a;
                uint32_t pkt_src_group = pkt_src_switch / _a;

                // printf("getNextHop: src_group = %d\n", src_group);
                // printf("getNextHop: dst_group = %d\n", dst_group);

                uint32_t src_intra_group_id = _id % _a;
                uint32_t dst_intra_group_id = dst_switch % _a;

                if (src_group == pkt_src_group && pkt_src_group != dst_group){ // Go to random group.
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
                            r->push_back(_st->queues_switch_switch[_id][next_group_connecting_node_id]);
                            r->push_back(_st->pipes_switch_switch[_id][next_group_connecting_node_id]);
                            r->push_back(_st->queues_switch_switch[_id][next_group_connecting_node_id]->getRemoteEndpoint());
                            e = new FibEntry(r, 1, DOWN);
                        }
                        else{
                            r->push_back(_st->queues_switch_switch[_id][random_node_id]);
                            r->push_back(_st->pipes_switch_switch[_id][random_node_id]);
                            r->push_back(_st->queues_switch_switch[_id][random_node_id]->getRemoteEndpoint());
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
                        r->push_back(_st->queues_switch_switch[_id][next_group_connecting_node_id]);
                        r->push_back(_st->pipes_switch_switch[_id][next_group_connecting_node_id]);
                        r->push_back(_st->queues_switch_switch[_id][next_group_connecting_node_id]->getRemoteEndpoint());
                        e = new FibEntry(r, 1, DOWN);
                    }
                }
                else if (src_group == pkt_src_group && pkt_src_group == dst_group){ // Go just to a random node within the group.
                    if (_id == dst_switch){
                        //printf("Exit.\n");
                        HostFibEntry *fe = _fib->getHostRoute(pkt.dst(), pkt.flow_id());
                        assert(fe);
                        pkt.set_direction(DOWN);
                        return fe->getEgressPort();
                    }
                    else {
                        if(pkt.hop_count == 0){
                            uint32_t random_intra_group_node = random() % (_a - 1);
                            uint32_t random_node_id = src_group * _a + random_intra_group_node;
                            if(random_node_id >= _id){random_node_id++;}
                            r->push_back(_st->queues_switch_switch[_id][random_node_id]);
                            r->push_back(_st->pipes_switch_switch[_id][random_node_id]);
                            r->push_back(_st->queues_switch_switch[_id][random_node_id]->getRemoteEndpoint());
                            // printf("getNextHop: routeBack_size = %ld\n", r->size());
                            e = new FibEntry(r, 1, DOWN);
                        }
                        else{ // pkt.hop_count == 1
                            r->push_back(_st->queues_switch_switch[_id][dst_switch]);
                            r->push_back(_st->pipes_switch_switch[_id][dst_switch]);
                            r->push_back(_st->queues_switch_switch[_id][dst_switch]->getRemoteEndpoint());
                            // printf("getNextHop: routeBack_size = %ld\n", r->size());
                            e = new FibEntry(r, 1, DOWN);
                        }
                    }
                }
                else{ // src_group != pkt_src_group
                    if(pkt_src_group == dst_group){ // src_group != pkt_src_group && pkt_src_group == dst_group
                        abort();
                    }
                    else{ // src_group != pkt_src_group && pkt_src_group != dst_group
                        if (_id == dst_switch){
                            //printf("Exit.\n");
                            HostFibEntry *fe = _fib->getHostRoute(pkt.dst(), pkt.flow_id());
                            assert(fe);
                            pkt.set_direction(DOWN);
                            return fe->getEgressPort();
                        }
                        else if (src_group == dst_group){
                            r->push_back(_st->queues_switch_switch[_id][dst_switch]);
                            r->push_back(_st->pipes_switch_switch[_id][dst_switch]);
                            r->push_back(_st->queues_switch_switch[_id][dst_switch]->getRemoteEndpoint());
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
                                r->push_back(_st->queues_switch_switch[_id][dst_group_connecting_node_id]);
                                r->push_back(_st->pipes_switch_switch[_id][dst_group_connecting_node_id]);
                                r->push_back(_st->queues_switch_switch[_id][dst_group_connecting_node_id]->getRemoteEndpoint());
                                // printf("getNextHop: routeBack_size = %ld\n", r->size());
                                e = new FibEntry(r, 1, DOWN);
                            }
                            else{
                                r->push_back(_st->queues_switch_switch[_id][group_connecting_node_id]);
                                r->push_back(_st->pipes_switch_switch[_id][group_connecting_node_id]);
                                r->push_back(_st->queues_switch_switch[_id][group_connecting_node_id]->getRemoteEndpoint());
                                // printf("getNextHop: routeBack_size = %ld\n", r->size());
                                e = new FibEntry(r, 1, DOWN);
                            }
                        }
                    }
                    
                }
                pkt.set_direction(e->getDirection());
                return e->getEgressPort(); */
                break;
            }
        }
        
        assert(_fib->getRoutes(pkt.dst()));
        printf("Debug: Ok.\tid: %d\tdst: %d\n", _id, pkt.dst());

        return getNextHop(pkt, ingress_port);
    }
};
