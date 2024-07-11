// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "hammingmesh_switch.h"
#include "string.h"
#include <stdlib.h>
#include <sstream>
#include <vector>
#include <algorithm>

#include "callback_pipe.h"
#include "hammingmesh_topology.h"
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

#define HOST_GROUP(src) (src / (_a * _p));

// RoundTrip Time.
extern uint32_t RTT;

// Convertes: Number (double) TO Ascii (string); Integer (uint64_t) TO Ascii (string).
string ntoa(double n);
string itoa(uint64_t n);

int HammingmeshSwitch::precision_ts = 1;

HammingmeshSwitch::HammingmeshSwitch(EventList &eventlist, string s, switch_type t,
    uint32_t id, simtime_picosec delay, HammingmeshTopology *ht) : Switch(eventlist, s) {
    _id = id;
    _type = t;
    _pipe = new CallbackPipe(delay, eventlist, this);
    _ht = ht;
    _crt_route = 0;
    _hash_salt = random();
    _last_choice = eventlist.now();
    _fib = new RouteTable();
}

HammingmeshSwitch::HammingmeshSwitch(EventList &eventlist, string s, switch_type t,
    uint32_t id, simtime_picosec delay, HammingmeshTopology *ht, uint32_t strat) : Switch(eventlist, s) {
    _id = id;
    _type = t;
    _pipe = new CallbackPipe(delay, eventlist, this);
    _ht = ht;
    _crt_route = 0;
    _hash_salt = random();
    _last_choice = eventlist.now();
    _fib = new RouteTable();
    _hm_strategy = (routing_strategy)strat;
}

void HammingmeshSwitch::receivePacket(Packet &pkt) {

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
        pkt.sendOn();
    }
};

void HammingmeshSwitch::addHostPort(int addr, uint32_t flowid, PacketSink *transport) {
    Route *rt = new Route();
    rt->push_back(_ht->queues_switch_host[addr][addr]);
    rt->push_back(_ht->pipes_switch_host[addr][addr]);
    rt->push_back(transport);
    _fib->addHostRoute(addr, rt, flowid);
}

void HammingmeshSwitch::permute_paths(vector<FibEntry *> *routes) {
    int len = routes->size();
    for (int i = 0; i < len; i++) {
        int ix = random() % (len - i);
        FibEntry *tmppath = (*routes)[ix];
        (*routes)[ix] = (*routes)[len - 1 - i];
        (*routes)[len - 1 - i] = tmppath;
    }
}

HammingmeshSwitch::routing_strategy HammingmeshSwitch::_hm_strategy = HammingmeshSwitch::NIX;
uint16_t HammingmeshSwitch::_ar_fraction = 0;
uint16_t HammingmeshSwitch::_ar_sticky = HammingmeshSwitch::PER_PACKET;
simtime_picosec HammingmeshSwitch::_sticky_delta = timeFromUs((uint32_t)10);
double HammingmeshSwitch::_ecn_threshold_fraction = 1.0;

// In contrast to the normal modulo (%) this function always returns positive values.
int HammingmeshSwitch::modulo (int x, int y){
    int res = x % y;
    if (res < 0){
        res += y;
    }
    return res;
}

Route *HammingmeshSwitch::getNextHop(Packet &pkt, BaseQueue *ingress_port) {
    // Retrieve available routes for the destination from the Forwarding Information Base (FIB)
    vector<FibEntry *> *available_hops = _fib->getRoutes(pkt.dst());
    if (available_hops) {
        uint32_t ecmp_choice = 0;

        if (available_hops->size() > 1){
            switch (_hm_strategy) {
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
        switch (_hm_strategy) {
            case NIX: {
                abort();
                break;
            }
            case MINIMAL: {
                Route *r = new Route();

                uint32_t pkt_dst = pkt.dst();

                uint32_t _height = _ht->get_height();
                uint32_t _width = _ht->get_width();
                uint32_t _height_board = _ht->get_height_board();
                uint32_t _width_board = _ht->get_width_board();
                uint32_t _no_board_switches = _height * _width * _height_board * _width_board;
                uint32_t _no_switches_per_board = _height_board * _width_board;
                bool _2nd_fat_tree_layer_height = (2 * _height > 64);
                bool _2nd_fat_tree_layer_width = (2 * _width > 64);
                uint32_t _fat_tree_size_height;
                uint32_t _fat_tree_size_width;
                if(_2nd_fat_tree_layer_height){
                    if((2 * _height) % 63 == 0){
                        _fat_tree_size_height = ((2 * _height) / 63) + 1;
                    }
                    else{
                         _fat_tree_size_height = ((2 * _height) / 63) + 2;
                    }
                }
                else{
                    _fat_tree_size_height = 1;
                }
                if(_2nd_fat_tree_layer_width){
                    if((2 * _width) % 63 == 0){
                        _fat_tree_size_width = ((2 * _width) / 63) + 1;
                    }
                    else{
                        _fat_tree_size_width = ((2 * _width) / 63) + 2;
                    }
                }
                else{
                    _fat_tree_size_width = 1;
                }

                uint32_t dst_height = pkt_dst / (_width * _no_switches_per_board);
                uint32_t dst_width = (pkt_dst % (_width * _no_switches_per_board)) / _no_switches_per_board;
                uint32_t dst_height_board = (pkt_dst % _no_switches_per_board) / _width_board;
                uint32_t dst_width_board = pkt_dst % _width_board;
                
                if (_id < _no_board_switches){ // On board switch.
                    uint32_t this_height = _id / (_width * _no_switches_per_board);
                    uint32_t this_width = (_id % (_width * _no_switches_per_board)) / _no_switches_per_board;
                    uint32_t this_height_board = (_id % _no_switches_per_board) / _width_board;
                    uint32_t this_width_board = _id % _width_board;
                    uint32_t this_fat_tree_nodes_height;
                    uint32_t this_fat_tree_nodes_width;
                    if(_2nd_fat_tree_layer_height && this_height % 63 == 31){
                        this_fat_tree_nodes_height = 3;
                    }
                    else{
                        this_fat_tree_nodes_height = 1;
                    }
                    if(_2nd_fat_tree_layer_width && this_width % 63 == 31){
                        this_fat_tree_nodes_width = 3;
                    }
                    else{
                        this_fat_tree_nodes_width = 1;
                    }

                    if (this_height == dst_height && this_width == dst_width){ // Same board.
                        if (this_height_board == dst_height_board && this_width_board == dst_width_board){ // Same node.
                            HostFibEntry *fe = _fib->getHostRoute(pkt.dst(), pkt.flow_id());
                            assert(fe);
                            pkt.set_direction(DOWN);
                            return fe->getEgressPort();
                        }
                        else if (this_height_board == dst_height_board){ // Same board. Same row.
                            bool send_left, send_right;
                            if (this_width_board < dst_width_board){
                                send_left = ((dst_width_board - this_width_board) >= (_width_board + this_width_board + this_fat_tree_nodes_width - dst_width_board));
                                send_right = ((dst_width_board - this_width_board) <= (_width_board + this_width_board + this_fat_tree_nodes_width - dst_width_board));
                            }
                            else{ // this_width_board > dst_width_board
                                send_left = ((this_width_board - dst_width_board) <= (_width_board + dst_width_board + this_fat_tree_nodes_width - this_width_board));
                                send_right = ((this_width_board - dst_width_board) >= (_width_board + dst_width_board + this_fat_tree_nodes_width - this_width_board));
                            }

                            if (send_right){
                                uint32_t next_node;
                                if (this_width_board == _width_board - 1){ // Send over fat tree.
                                    if (_2nd_fat_tree_layer_width){
                                        next_node = (2 * this_width + 1) / 63;
                                        next_node += _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                        next_node += this_height * _height_board * _fat_tree_size_width + this_height_board * _fat_tree_size_width;
                                    }
                                    else{
                                        next_node = _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                        next_node += this_height * _height_board + this_height_board;
                                    }
                                }
                                else{ // Send to node to the right.
                                    next_node = _id + 1;
                                }
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                            if (send_left){
                                r = new Route();
                                uint32_t next_node;
                                if (this_width_board == 0){ // Send over fat tree.
                                    if (_2nd_fat_tree_layer_width){
                                        next_node = (2 * this_width) / 63;
                                        next_node += _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                        next_node += this_height * _height_board * _fat_tree_size_width + this_height_board * _fat_tree_size_width;
                                    }
                                    else{
                                        next_node = _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                        next_node += this_height * _height_board + this_height_board;
                                    }
                                }
                                else{ // Send to node to the right.
                                    next_node = _id - 1;
                                }
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                        }
                        else if (this_width_board == dst_width_board){ // Same board. Same column.
                            bool send_up, send_down;
                            if (this_height_board < dst_height_board){
                                send_up = ((dst_height_board - this_height_board) >= (_height_board + this_height_board + this_fat_tree_nodes_height - dst_height_board));
                                send_down = ((dst_height_board - this_height_board) <= (_height_board + this_height_board + this_fat_tree_nodes_height - dst_height_board));
                            }
                            else{ // this_height_board > dst_height_board
                                send_up = ((this_height_board - dst_height_board) <= (_height_board + dst_height_board + this_fat_tree_nodes_height - this_height_board));
                                send_down = ((this_height_board - dst_height_board) >= (_height_board + dst_height_board + this_fat_tree_nodes_height - this_height_board));
                            }

                            if (send_down){
                                uint32_t next_node;
                                if (this_height_board == _height_board - 1){ // Send over fat tree.
                                    if (_2nd_fat_tree_layer_height){
                                        next_node = (2 * this_height + 1) / 63;
                                        next_node += _no_board_switches + this_width * _width_board * _fat_tree_size_height + this_width_board * _fat_tree_size_height;
                                    }
                                    else{
                                        next_node = _no_board_switches + this_width * _width_board + this_width_board;
                                    }
                                }
                                else{ // Send to node to the bottom.
                                    next_node = _id + _width_board;
                                }
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                            if (send_up){
                                r = new Route();
                                uint32_t next_node;
                                if (this_height_board == 0){ // Send over fat tree.
                                    if (_2nd_fat_tree_layer_height){
                                        next_node = (2 * this_height) / 63;
                                        next_node += _no_board_switches + this_width * _width_board * _fat_tree_size_height + this_width_board * _fat_tree_size_height;
                                    }
                                    else{
                                        next_node = _no_board_switches + this_width * _width_board + this_width_board;
                                    }
                                }
                                else{ // Send to node to the top.
                                    next_node = _id - _width_board;
                                }
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                        }
                        else{ // Same board. Neither same row nor same column.
                            bool send_left, send_right;
                            if (this_width_board < dst_width_board){
                                send_left = ((dst_width_board - this_width_board) >= (_width_board + this_width_board + this_fat_tree_nodes_width - dst_width_board));
                                send_right = ((dst_width_board - this_width_board) <= (_width_board + this_width_board + this_fat_tree_nodes_width - dst_width_board));
                            }
                            else{ // this_width_board > dst_width_board
                                send_left = ((this_width_board - dst_width_board) <= (_width_board + dst_width_board + this_fat_tree_nodes_width - this_width_board));
                                send_right = ((this_width_board - dst_width_board) >= (_width_board + dst_width_board + this_fat_tree_nodes_width - this_width_board));
                            }

                            if (send_right){
                                uint32_t next_node;
                                if (this_width_board == _width_board - 1){ // Send over fat tree.
                                    if (_2nd_fat_tree_layer_width){
                                        next_node = (2 * this_width + 1) / 63;
                                        next_node += _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                        next_node += this_height * _height_board * _fat_tree_size_width + this_height_board * _fat_tree_size_width;
                                    }
                                    else{
                                        next_node = _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                        next_node += this_height * _height_board + this_height_board;
                                    }
                                }
                                else{ // Send to node to the right.
                                    next_node = _id + 1;
                                }
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                            if (send_left){
                                r = new Route();
                                uint32_t next_node;
                                if (this_width_board == 0){ // Send over fat tree.
                                    if (_2nd_fat_tree_layer_width){
                                        next_node = (2 * this_width) / 63;
                                        next_node += _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                        next_node += this_height * _height_board * _fat_tree_size_width + this_height_board * _fat_tree_size_width;
                                    }
                                    else{
                                        next_node = _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                        next_node += this_height * _height_board + this_height_board;
                                    }
                                }
                                else{ // Send to node to the right.
                                    next_node = _id - 1;
                                }
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }

                            bool send_up, send_down;
                            if (this_height_board < dst_height_board){
                                send_up = ((dst_height_board - this_height_board) >= (_height_board + this_height_board + this_fat_tree_nodes_height - dst_height_board));
                                send_down = ((dst_height_board - this_height_board) <= (_height_board + this_height_board + this_fat_tree_nodes_height - dst_height_board));
                            }
                            else{ // this_height_board > dst_height_board
                                send_up = ((this_height_board - dst_height_board) <= (_height_board + dst_height_board + this_fat_tree_nodes_height - this_height_board));
                                send_down = ((this_height_board - dst_height_board) >= (_height_board + dst_height_board + this_fat_tree_nodes_height - this_height_board));
                            }

                            if (send_down){
                                r = new Route();
                                uint32_t next_node;
                                if (this_height_board == _height_board - 1){ // Send over fat tree.
                                    if (_2nd_fat_tree_layer_height){
                                        next_node = (2 * this_height + 1) / 63;
                                        next_node += _no_board_switches + this_width * _width_board * _fat_tree_size_height + this_width_board * _fat_tree_size_height;
                                    }
                                    else{
                                        next_node = _no_board_switches + this_width * _width_board + this_width_board;
                                    }
                                }
                                else{ // Send to node to the bottom.
                                    next_node = _id + _width_board;
                                }
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                            if (send_up){
                                r = new Route();
                                uint32_t next_node;
                                if (this_height_board == 0){ // Send over fat tree.
                                    if (_2nd_fat_tree_layer_height){
                                        next_node = (2 * this_height) / 63;
                                        next_node += _no_board_switches + this_width * _width_board * _fat_tree_size_height + this_width_board * _fat_tree_size_height;
                                    }
                                    else{
                                        next_node = _no_board_switches + this_width * _width_board + this_width_board;
                                    }
                                }
                                else{ // Send to node to the top.
                                    next_node = _id - _width_board;
                                }
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                        }
                    }
                    else if (this_height == dst_height){ // Boards in same row.
                        uint32_t next_node;
                        if (this_width_board == 0){
                            if (_2nd_fat_tree_layer_width){
                                next_node = (2 * this_width) / 63;
                                next_node += _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                next_node += this_height * _height_board * _fat_tree_size_width + this_height_board * _fat_tree_size_width;
                            }
                            else{
                                next_node = _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                next_node += this_height * _height_board + this_height_board;
                            }
                        }
                        else if (this_width_board == _width_board - 1){
                            if (_2nd_fat_tree_layer_width){
                                next_node = (2 * this_width + 1) / 63;
                                next_node += _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                next_node += this_height * _height_board * _fat_tree_size_width + this_height_board * _fat_tree_size_width;
                            }
                            else{
                                next_node = _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                next_node += this_height * _height_board + this_height_board;
                            }
                        }
                        else{
                            if (this_width_board < _width_board - 1 - this_width_board){ // Send left.
                                next_node = _id - 1;
                            }
                            else if (this_width_board > _width_board - 1 - this_width_board){ // Send right.
                                next_node = _id + 1;
                            }
                            else{ // this_width_board == _width_board - 1 - this_width_board
                                r->push_back(_ht->queues_switch_switch[_id][_id - 1]);
                                r->push_back(_ht->pipes_switch_switch[_id][_id - 1]);
                                r->push_back(_ht->queues_switch_switch[_id][_id - 1]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);

                                r = new Route();
                                next_node = _id + 1;
                            }
                        }
                        r->push_back(_ht->queues_switch_switch[_id][next_node]);
                        r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                        r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                        
                        if (this_height_board != dst_height_board){
                            r = new Route();
                            bool send_up, send_down;
                            if (this_height_board < dst_height_board){
                                send_up = ((dst_height_board - this_height_board) >= (_height_board + this_height_board + this_fat_tree_nodes_height - dst_height_board));
                                send_down = ((dst_height_board - this_height_board) <= (_height_board + this_height_board + this_fat_tree_nodes_height - dst_height_board));
                            }
                            else{ // this_height_board > dst_height_board
                                send_up = ((this_height_board - dst_height_board) <= (_height_board + dst_height_board + this_fat_tree_nodes_height - this_height_board));
                                send_down = ((this_height_board - dst_height_board) >= (_height_board + dst_height_board + this_fat_tree_nodes_height - this_height_board));
                            }

                            if (send_down){
                                uint32_t next_node;
                                if (this_height_board == _height_board - 1){ // Send over fat tree.
                                    if (_2nd_fat_tree_layer_height){
                                        next_node = (2 * this_height + 1) / 63;
                                        next_node += _no_board_switches + this_width * _width_board * _fat_tree_size_height + this_width_board * _fat_tree_size_height;
                                    }
                                    else{
                                        next_node = _no_board_switches + this_width * _width_board + this_width_board;
                                    }
                                }
                                else{ // Send to node to the bottom.
                                    next_node = _id + _width_board;
                                }
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                            if (send_up){
                                r = new Route();
                                uint32_t next_node;
                                if (this_height_board == 0){ // Send over fat tree.
                                    if (_2nd_fat_tree_layer_height){
                                        next_node = (2 * this_height) / 63;
                                        next_node += _no_board_switches + this_width * _width_board * _fat_tree_size_height + this_width_board * _fat_tree_size_height;
                                    }
                                    else{
                                        next_node = _no_board_switches + this_width * _width_board + this_width_board;
                                    }
                                }
                                else{ // Send to node to the top.
                                    next_node = _id - _width_board;
                                }
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                        }
                    }
                    else if (this_width == dst_width){ // Boards in same column.
                        uint32_t next_node;
                        if (this_height_board == 0){
                            if (_2nd_fat_tree_layer_height){
                                next_node = (2 * this_height) / 63;
                                next_node += _no_board_switches + this_width * _width_board * _fat_tree_size_height + this_width_board * _fat_tree_size_height;
                            }
                            else{
                                next_node = _no_board_switches + this_width * _width_board + this_width_board;
                            }
                        }
                        else if (this_height_board == _height_board - 1){
                            if (_2nd_fat_tree_layer_height){
                                next_node = (2 * this_height + 1) / 63;
                                next_node += _no_board_switches + this_width * _width_board * _fat_tree_size_height + this_width_board * _fat_tree_size_height;
                            }
                            else{
                                next_node = _no_board_switches + this_width * _width_board + this_width_board;
                            }
                        }
                        else{
                            if (this_height_board < _height_board - 1 - this_height_board){ // Send up.
                                next_node = _id - _width_board;
                            }
                            else if (this_height_board > _height_board - 1 - this_height_board){ // Send down.
                                next_node = _id + _width_board;
                            }
                            else{ // this_width_board == _width_board - 1 - this_width_board
                                r->push_back(_ht->queues_switch_switch[_id][_id - _width_board]);
                                r->push_back(_ht->pipes_switch_switch[_id][_id - _width_board]);
                                r->push_back(_ht->queues_switch_switch[_id][_id - _width_board]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);

                                r = new Route();
                                next_node = _id + _width_board;
                            }
                        }
                        r->push_back(_ht->queues_switch_switch[_id][next_node]);
                        r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                        r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                        
                        if (this_width_board != dst_width_board){
                            r = new Route();
                            bool send_left, send_right;
                            if (this_width_board < dst_width_board){
                                send_left = ((dst_width_board - this_width_board) >= (_width_board + this_width_board + this_fat_tree_nodes_width - dst_width_board));
                                send_right = ((dst_width_board - this_width_board) <= (_width_board + this_width_board + this_fat_tree_nodes_width - dst_width_board));
                            }
                            else{ // this_width_board > dst_width_board
                                send_left = ((this_width_board - dst_width_board) <= (_width_board + dst_width_board + this_fat_tree_nodes_width - this_width_board));
                                send_right = ((this_width_board - dst_width_board) >= (_width_board + dst_width_board + this_fat_tree_nodes_width - this_width_board));
                            }

                            if (send_right){
                                uint32_t next_node;
                                if (this_width_board == _width_board - 1){ // Send over fat tree.
                                    if (_2nd_fat_tree_layer_width){
                                        next_node = (2 * this_width + 1) / 63;
                                        next_node += _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                        next_node += this_height * _height_board * _fat_tree_size_width + this_height_board * _fat_tree_size_width;
                                    }
                                    else{
                                        next_node = _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                        next_node += this_height * _height_board + this_height_board;
                                    }
                                }
                                else{ // Send to node to the right.
                                    next_node = _id + 1;
                                }
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                            if (send_left){
                                r = new Route();
                                uint32_t next_node;
                                if (this_width_board == 0){ // Send over fat tree.
                                    if (_2nd_fat_tree_layer_width){
                                        next_node = (2 * this_width) / 63;
                                        next_node += _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                        next_node += this_height * _height_board * _fat_tree_size_width + this_height_board * _fat_tree_size_width;
                                    }
                                    else{
                                        next_node = _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                        next_node += this_height * _height_board + this_height_board;
                                    }
                                }
                                else{ // Send to node to the right.
                                    next_node = _id - 1;
                                }
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);
                            }
                        }
                    }
                    else{ // Boards neither in same row nor same column.
                        uint32_t next_node;
                        if (this_height_board == 0){
                            if (_2nd_fat_tree_layer_height){
                                next_node = (2 * this_height) / 63;
                                next_node += _no_board_switches + this_width * _width_board * _fat_tree_size_height + this_width_board * _fat_tree_size_height;
                            }
                            else{
                                next_node = _no_board_switches + this_width * _width_board + this_width_board;
                            }
                        }
                        else if (this_height_board == _height_board - 1){
                            if (_2nd_fat_tree_layer_height){
                                next_node = (2 * this_height + 1) / 63;
                                next_node += _no_board_switches + this_width * _width_board * _fat_tree_size_height + this_width_board * _fat_tree_size_height;
                            }
                            else{
                                next_node = _no_board_switches + this_width * _width_board + this_width_board;
                            }
                        }
                        else{
                            if (this_height_board < _height_board - 1 - this_height_board){ // Send up.
                                next_node = _id - _width_board;
                            }
                            else if (this_height_board > _height_board - 1 - this_height_board){ // Send down.
                                next_node = _id + _width_board;
                            }
                            else{ // this_width_board == _width_board - 1 - this_width_board
                                r->push_back(_ht->queues_switch_switch[_id][_id - _width_board]);
                                r->push_back(_ht->pipes_switch_switch[_id][_id - _width_board]);
                                r->push_back(_ht->queues_switch_switch[_id][_id - _width_board]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);

                                r = new Route();
                                next_node = _id + _width_board;
                            }
                        }
                        r->push_back(_ht->queues_switch_switch[_id][next_node]);
                        r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                        r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                        
                        if (this_width_board == 0){
                            if (_2nd_fat_tree_layer_width){
                                next_node = (2 * this_width) / 63;
                                next_node += _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                next_node += this_height * _height_board * _fat_tree_size_width + this_height_board * _fat_tree_size_width;
                            }
                            else{
                                next_node = _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                next_node += this_height * _height_board + this_height_board;
                            }
                        }
                        else if (this_width_board == _width_board - 1){
                            if (_2nd_fat_tree_layer_width){
                                next_node = (2 * this_width + 1) / 63;
                                next_node += _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                next_node += this_height * _height_board * _fat_tree_size_width + this_height_board * _fat_tree_size_width;
                            }
                            else{
                                next_node = _no_board_switches + _width * _width_board * _fat_tree_size_height;
                                next_node += this_height * _height_board + this_height_board;
                            }
                        }
                        else{
                            if (this_width_board < _width_board - 1 - this_width_board){ // Send left.
                                next_node = _id - 1;
                            }
                            else if (this_width_board > _width_board - 1 - this_width_board){ // Send right.
                                next_node = _id + 1;
                            }
                            else{ // this_width_board == _width_board - 1 - this_width_board
                                r->push_back(_ht->queues_switch_switch[_id][_id - 1]);
                                r->push_back(_ht->pipes_switch_switch[_id][_id - 1]);
                                r->push_back(_ht->queues_switch_switch[_id][_id - 1]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);

                                r = new Route();
                                next_node = _id + 1;
                            }
                        }
                        r->push_back(_ht->queues_switch_switch[_id][next_node]);
                        r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                        r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                        _fib->addRoute(pkt.dst(), r, 1, DOWN);
                    }
                }
                else{ // Fat tree switch.
                    if(_id < _no_board_switches + _width * _width_board * _fat_tree_size_height){ // Height fat tree.
                        uint32_t fat_tree_id = _id - _no_board_switches;
                        uint32_t this_width = fat_tree_id / (_width_board * _fat_tree_size_height);
                        uint32_t this_width_board = (fat_tree_id % (_width_board * _fat_tree_size_height)) / _fat_tree_size_height;
                        uint32_t dst_board_entry_top = dst_height * _width * _width_board * _height_board + this_width * _width_board * _height_board + this_width_board;
                        uint32_t dst_board_entry_bottom = dst_height * _width * _width_board * _height_board + this_width * _width_board * _height_board + _width_board * (_height_board - 1) + this_width_board;
                        
                        if (_2nd_fat_tree_layer_height){
                            uint32_t dst_fat_tree_entry_top = _no_board_switches + this_width * _width_board * _fat_tree_size_height + this_width_board * _fat_tree_size_height + ((2 * dst_height) / 63);
                            uint32_t dst_fat_tree_entry_bottom = _no_board_switches + this_width * _width_board * _fat_tree_size_height + this_width_board * _fat_tree_size_height + ((2 * dst_height + 1) / 63);
                            uint32_t dst_fat_tree_root = _no_board_switches + (this_width * _width_board + this_width_board + 1) * _fat_tree_size_height - 1;

                            uint32_t dist_to_entry_top;
                            uint32_t dist_to_entry_bottom;
                            if (fat_tree_id % _fat_tree_size_height == _fat_tree_size_height - 1){
                                dist_to_entry_top = 1;
                                dist_to_entry_bottom = 1;
                            }
                            else{
                                if (_id == dst_fat_tree_entry_top){
                                    dist_to_entry_top = 0;
                                }
                                else{
                                    dist_to_entry_top = 2;
                                }
                                if (_id == dst_fat_tree_entry_bottom){
                                    dist_to_entry_bottom = 0;
                                }
                                else{
                                    dist_to_entry_bottom = 2;
                                }
                            }

                            uint32_t next_node;
                            if (dist_to_entry_top + 1 + dst_height_board < dist_to_entry_bottom + 1 + (_height_board - dst_height_board)){
                                if (dist_to_entry_top == 0){
                                    next_node = dst_board_entry_top;
                                }
                                else if (dist_to_entry_top == 1){
                                    next_node = dst_fat_tree_entry_top;
                                }
                                else { // dist_to_entry_top == 2
                                    next_node = dst_fat_tree_root;
                                }
                            }
                            else if (dist_to_entry_top + 1 + dst_height_board > dist_to_entry_bottom + 1 + (_height_board - dst_height_board)){
                                if (dist_to_entry_bottom == 0){
                                    next_node = dst_board_entry_bottom;
                                }
                                else if (dist_to_entry_bottom == 1){
                                    next_node = dst_fat_tree_entry_bottom;
                                }
                                else { // dist_to_entry_bottom == 2
                                    next_node = dst_fat_tree_root;
                                }
                            }
                            else{ // dist_to_entry_top + 1 + dst_height_board == dist_to_entry_bottom + 1 + (_height_board - dst_height_board
                                if (dist_to_entry_top == 0){
                                    next_node = dst_board_entry_top;
                                }
                                else if (dist_to_entry_top == 1){
                                    next_node = dst_fat_tree_entry_top;
                                }
                                else { // dist_to_entry_top == 2
                                    next_node = dst_fat_tree_root;
                                }
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);

                                r = new Route();

                                if (dist_to_entry_bottom == 0){
                                    next_node = dst_board_entry_bottom;
                                }
                                else if (dist_to_entry_bottom == 1){
                                    next_node = dst_fat_tree_entry_bottom;
                                }
                                else { // dist_to_entry_bottom == 2
                                    next_node = dst_fat_tree_root;
                                }
                            }
                            r->push_back(_ht->queues_switch_switch[_id][next_node]);
                            r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                            r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                            _fib->addRoute(pkt.dst(), r, 1, DOWN);
                        }
                        else{
                            uint32_t next_node;
                            if (dst_height_board < _height_board - 1 - dst_height_board){ // Send up.
                                next_node = dst_board_entry_top;
                            }
                            else if (dst_height_board > _height_board - 1 - dst_height_board){ // Send down.
                                next_node = dst_board_entry_bottom;
                            }
                            else{ // dst_height_board == _height_board - 1 - dst_height_board
                                next_node = dst_board_entry_top;
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);

                                r = new Route();
                                next_node = dst_board_entry_bottom;
                            }
                            r->push_back(_ht->queues_switch_switch[_id][next_node]);
                            r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                            r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                            _fib->addRoute(pkt.dst(), r, 1, DOWN);
                        }
                    }
                    else{ // Width fat tree.
                        uint32_t fat_tree_id = _id - _no_board_switches - _width * _width_board * _fat_tree_size_height;
                        uint32_t this_height = fat_tree_id / (_height_board * _fat_tree_size_width);
                        uint32_t this_height_board = (fat_tree_id % (_height_board * _fat_tree_size_width)) / _fat_tree_size_width;
                        uint32_t dst_board_entry_left = this_height * _width * _width_board * _height_board + dst_width * _width_board * _height_board + this_height_board * _width_board;
                        uint32_t dst_board_entry_right = dst_board_entry_left + _width_board - 1;

                        if (_2nd_fat_tree_layer_width){
                            uint32_t dst_fat_tree_entry_left = _no_board_switches + _width * _width_board * _fat_tree_size_height + this_height * _height_board * _fat_tree_size_width + this_height_board * _fat_tree_size_width + ((2 * dst_width) / 63);
                            uint32_t dst_fat_tree_entry_right = _no_board_switches + _width * _width_board * _fat_tree_size_height + this_height * _height_board * _fat_tree_size_width + this_height_board * _fat_tree_size_width + ((2 * dst_width + 1) / 63);
                            uint32_t dst_fat_tree_root = _no_board_switches + _width * _width_board * _fat_tree_size_height + (this_height * _height_board + this_height_board + 1) * _fat_tree_size_width - 1;

                            uint32_t dist_to_entry_left;
                            uint32_t dist_to_entry_right;
                            if (fat_tree_id % _fat_tree_size_width == _fat_tree_size_width - 1){
                                dist_to_entry_left = 1;
                                dist_to_entry_right = 1;
                            }
                            else{
                                if (_id == dst_fat_tree_entry_left){
                                    dist_to_entry_left = 0;
                                }
                                else{
                                    dist_to_entry_left = 2;
                                }
                                if (_id == dst_fat_tree_entry_right){
                                    dist_to_entry_right = 0;
                                }
                                else{
                                    dist_to_entry_right = 2;
                                }
                            }

                            uint32_t next_node;
                            if (dist_to_entry_left + 1 + dst_width_board < dist_to_entry_right + 1 + (_width_board - dst_width_board)){
                                if (dist_to_entry_left == 0){
                                    next_node = dst_board_entry_left;
                                }
                                else if (dist_to_entry_left == 1){
                                    next_node = dst_fat_tree_entry_left;
                                }
                                else { // dist_to_entry_left == 2
                                    next_node = dst_fat_tree_root;
                                }
                            }
                            else if (dist_to_entry_left + 1 + dst_width_board > dist_to_entry_right + 1 + (_width_board - dst_width_board)){
                                if (dist_to_entry_right == 0){
                                    next_node = dst_board_entry_right;
                                }
                                else if (dist_to_entry_right == 1){
                                    next_node = dst_fat_tree_entry_right;
                                }
                                else { // dist_to_entry_right == 2
                                    next_node = dst_fat_tree_root;
                                }
                            }
                            else{ // dist_to_entry_left + 1 + dst_width_board == dist_to_entry_right + 1 + (_width_board - dst_width_board)
                                if (dist_to_entry_left == 0){
                                    next_node = dst_board_entry_left;
                                }
                                else if (dist_to_entry_left == 1){
                                    next_node = dst_fat_tree_entry_left;
                                }
                                else { // dist_to_entry_left == 2
                                    next_node = dst_fat_tree_root;
                                }
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);

                                r = new Route();

                                if (dist_to_entry_right == 0){
                                    next_node = dst_board_entry_right;
                                }
                                else if (dist_to_entry_right == 1){
                                    next_node = dst_fat_tree_entry_right;
                                }
                                else { // dist_to_entry_right == 2
                                    next_node = dst_fat_tree_root;
                                }
                            }
                            r->push_back(_ht->queues_switch_switch[_id][next_node]);
                            r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                            r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                            _fib->addRoute(pkt.dst(), r, 1, DOWN);
                        }
                        else{
                            uint32_t next_node;
                            if (dst_width_board < _width_board - 1 - dst_width_board){ // Send left.
                                next_node = dst_board_entry_left;
                            }
                            else if (dst_height_board > _height_board - 1 - dst_height_board){ // Send right.
                                next_node = dst_board_entry_right;
                            }
                            else{ // dst_width_board == _width_board - 1 - dst_width_board
                                next_node = dst_board_entry_left;
                                r->push_back(_ht->queues_switch_switch[_id][next_node]);
                                r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                                r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                                _fib->addRoute(pkt.dst(), r, 1, DOWN);

                                r = new Route();
                                next_node = dst_board_entry_right;
                            }
                            r->push_back(_ht->queues_switch_switch[_id][next_node]);
                            r->push_back(_ht->pipes_switch_switch[_id][next_node]);
                            r->push_back(_ht->queues_switch_switch[_id][next_node]->getRemoteEndpoint());
                            _fib->addRoute(pkt.dst(), r, 1, DOWN);
                        }
                    }
                }

                break;
            }
        }
        
        assert(_fib->getRoutes(pkt.dst()));
        //printf("Debug: Ok.\tid: %d\tdst: %d\n", _id, pkt.dst());

        return getNextHop(pkt, ingress_port);
    }
};
