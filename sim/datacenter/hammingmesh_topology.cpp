// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "hammingmesh_topology.h"
#include "string.h"
#include <sstream>
#include <vector>

#include "compositequeue.h"
#include "compositequeuebts.h"
#include "hammingmesh_switch.h"
#include "ecnqueue.h"
#include "main.h"
#include "prioqueue.h"
#include "queue.h"
#include "queue_lossless.h"
#include "queue_lossless_input.h"
#include "queue_lossless_output.h"
#include "swift_scheduler.h"
#include <iostream>

// RoundTrip Time.
extern uint32_t RTT;

// Explicit Congestion Notification (ECN)
bool HammingmeshTopology::_enable_ecn = false;
mem_b HammingmeshTopology::_ecn_low = 0;
mem_b HammingmeshTopology::_ecn_high = 0;

// Convertes: Number (double) TO Ascii (string); Integer (uint64_t) TO Ascii (string).
string ntoa(double n);
string itoa(uint64_t n);

// Creates a new instance of a Hammingmesh topology.
HammingmeshTopology::HammingmeshTopology(uint32_t height, uint32_t width, uint32_t height_board, uint32_t width_board, mem_b queuesize, EventList *ev, queue_type qt) {

    if (height < 1 || width < 1 || height_board < 1 || width_board < 1) {
        cerr << "height, width, height_board and width_board all have to be positive." << endl;
    }

    logfile = NULL;

    _height = height;
    _width = width;
    _height_board = height_board;
    _width_board =width_board;

    _queuesize = queuesize;
    _eventlist = ev;
    _qt = qt;

    _no_of_groups = _height * _width;
    _no_of_switches = (_height_board * _width_board) * _no_of_groups;
    _no_of_nodes = _no_of_switches;

    uint32_t total_height = _height * _height_board;
    uint32_t total_width = _width * _width_board;
    if(2 * _height <= 64){ _no_of_switches += total_width; }
    else if(2 * _height <= 64*63){ _no_of_switches += total_width * (((2 * _height)/63) + 1 + 1); }
    else{ abort(); }
    if(2 * _width <= 64){ _no_of_switches += total_height; }
    else if(2 * _width <= 64*63){ _no_of_switches += total_height * (((2 * _width)/63) + 1 + 1); }
    else{ abort(); }

    std::cout << "Hammingmesh topology with " << _no_of_switches << " switches, total nodes " << _no_of_nodes << "." << endl;
    std::cout << "Queue type " << _qt << endl;

    init_pipes_queues();
    init_network();
}

HammingmeshTopology::HammingmeshTopology(uint32_t height, uint32_t width, uint32_t height_board, uint32_t width_board, mem_b queuesize, EventList *ev, queue_type qt, uint32_t strat) {
    _hm_routing_strategy = strat;

    if (height < 1 || width < 1 || height_board < 1 || width_board < 1) {
        cerr << "height, width, height_board and width_board all have to be positive." << endl;
    }

    logfile = NULL;

    _height = height;
    _width = width;
    _height_board = height_board;
    _width_board = width_board;

    _queuesize = queuesize;
    _eventlist = ev;
    _qt = qt;

    _no_of_groups = _height * _width;
    _no_of_switches = (_height_board * _width_board) * _no_of_groups;
    _no_of_nodes = _no_of_switches;

    uint32_t total_height = _height * _height_board;
    uint32_t total_width = _width * _width_board;
    if(2 * _height <= 64){ _no_of_switches += total_width; }
    else if(2 * _height <= 64*63){ _no_of_switches += total_width * (((2 * _height)/63) + 1 + 1); }
    else{ abort(); }
    if(2 * _width <= 64){ _no_of_switches += total_height; }
    else if(2 * _width <= 64*63){ _no_of_switches += total_height * (((2 * _width)/63) + 1 + 1); }
    else{ abort(); }

    std::cout << "Hammingmesh topology with " << _no_of_switches << " switches, total nodes " << _no_of_nodes << "." << endl;
    std::cout << "Queue type " << _qt << endl;

    init_pipes_queues();
    init_network();
}

bool HammingmeshTopology::is_element(vector<uint32_t> arr, uint32_t el){
    int size = arr.size();
    for (int i = 0; i < size; i++){
        if(arr[i] == el) {return true;}
    }
    return false;
}

// Initializes all pipes and queues for the switches.
void HammingmeshTopology::init_pipes_queues() {
    switches.resize(_no_of_switches, NULL);
    
    pipes_host_switch.resize(_no_of_nodes, vector<Pipe *>(_no_of_switches));
    pipes_switch_switch.resize(_no_of_switches, vector<Pipe *>(_no_of_switches));
    pipes_switch_host.resize(_no_of_switches, vector<Pipe *>(_no_of_nodes));
    
    queues_host_switch.resize(_no_of_nodes, vector<Queue *>(_no_of_switches));
    queues_switch_switch.resize(_no_of_switches, vector<Queue *>(_no_of_switches));
    queues_switch_host.resize(_no_of_switches, vector<Queue *>(_no_of_nodes));
}

// Creates an instance of a fair priority queue. It is used for packet input.
Queue *HammingmeshTopology::alloc_src_queue(QueueLogger *queueLogger) {
    return new FairPriorityQueue(speedFromMbps((uint64_t)HOST_NIC), memFromPkt(FEEDER_BUFFER), *_eventlist,
                                 queueLogger);
}

// Allocates a queue with a default link speed set to HOST_NIC.
Queue *HammingmeshTopology::alloc_queue(QueueLogger *queueLogger, mem_b queuesize, bool tor = false) {
    return alloc_queue(queueLogger, HOST_NIC, queuesize, tor);
}

// Creates an instance of a queue based on the type which is needed.
Queue *HammingmeshTopology::alloc_queue(QueueLogger *queueLogger, uint64_t speed, mem_b queuesize, bool tor) {
    if (_qt == RANDOM)
        return new RandomQueue(speedFromMbps(speed), queuesize, *_eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
    else if (_qt == COMPOSITE){
        CompositeQueue *q = new CompositeQueue(speedFromMbps(speed), queuesize, *_eventlist, queueLogger);
        if (_enable_ecn) {
            q->set_ecn_thresholds(_ecn_low, _ecn_high);
        }
        return q;
    }
    else if (_qt == CTRL_PRIO)
        return new CtrlPrioQueue(speedFromMbps(speed), queuesize, *_eventlist, queueLogger);
    else if (_qt == ECN)
        return new ECNQueue(speedFromMbps(speed), memFromPkt(queuesize), *_eventlist, queueLogger, memFromPkt(15));
    else if (_qt == LOSSLESS)
        return new LosslessQueue(speedFromMbps(speed), memFromPkt(50), *_eventlist, queueLogger, NULL);
    else if (_qt == LOSSLESS_INPUT)
        return new LosslessOutputQueue(speedFromMbps(speed), memFromPkt(200), *_eventlist, queueLogger);
    else if (_qt == LOSSLESS_INPUT_ECN)
        return new LosslessOutputQueue(speedFromMbps(speed), memFromPkt(10000), *_eventlist, queueLogger, 1, memFromPkt(16));
    else if (_qt == COMPOSITE_ECN) {
        if (tor)
            return new CompositeQueue(speedFromMbps(speed), queuesize, *_eventlist, queueLogger);
        else
            return new ECNQueue(speedFromMbps(speed), memFromPkt(2 * SWITCH_BUFFER), *_eventlist, queueLogger, memFromPkt(15));
    }
    assert(0);
}

// Creates the links between switches.
void HammingmeshTopology::create_switch_switch_link(uint32_t k, uint32_t j, QueueLoggerSampling *queueLogger){
    // Downlink
    queueLogger = new QueueLoggerSampling(timeFromMs(1000), *_eventlist);
    //logfile->addLogger(*queueLogger);

    queues_switch_switch[k][j] = alloc_queue(queueLogger, _queuesize);
    queues_switch_switch[k][j]->setName("SW" + ntoa(k) + "-I->SW" + ntoa(j));
    //logfile->writeName(*(queues_switch_switch[k][j]));

    pipes_switch_switch[k][j] = new Pipe(timeFromUs(RTT), *_eventlist);
    pipes_switch_switch[k][j]->setName("Pipe-SW" + ntoa(k) + "-I->SW" + ntoa(j));
    //logfile->writeName(*(pipes_switch_switch[k][j]));

    // Uplink
    queueLogger = new QueueLoggerSampling(timeFromMs(1000), *_eventlist);
    //logfile->addLogger(*queueLogger);

    queues_switch_switch[j][k] = alloc_queue(queueLogger, _queuesize, true);
    queues_switch_switch[j][k]->setName("SW" + ntoa(j) + "-I->SW" + ntoa(k));
    //logfile->writeName(*(queues_switch_switch[j][k]));

    switches[j]->addPort(queues_switch_switch[j][k]);
    switches[k]->addPort(queues_switch_switch[k][j]);
    queues_switch_switch[j][k]->setRemoteEndpoint(switches[k]);
    queues_switch_switch[k][j]->setRemoteEndpoint(switches[j]);

    if (_qt == LOSSLESS) {
        switches[j]->addPort(queues_switch_switch[j][k]);
        ((LosslessQueue *)queues_switch_switch[j][k])->setRemoteEndpoint(queues_switch_switch[k][j]);
        switches[k]->addPort(queues_switch_switch[k][j]);
        ((LosslessQueue *)queues_switch_switch[k][j])->setRemoteEndpoint(queues_switch_switch[j][k]);
    } else if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN) {
        new LosslessInputQueue(*_eventlist, queues_switch_switch[j][k]);
        new LosslessInputQueue(*_eventlist, queues_switch_switch[k][j]);
    }

    pipes_switch_switch[j][k] = new Pipe(timeFromUs(RTT), *_eventlist);
    pipes_switch_switch[j][k]->setName("Pipe-SW" + ntoa(j) + "-I->SW" + ntoa(k));
    //logfile->writeName(*(pipes_switch_switch[j][k]));
}

// Connects all nodes to a hammingmesh network.
void HammingmeshTopology::init_network() {
    QueueLoggerSampling *queueLogger;

    // Initializes all queues and pipes to NULL.
    for (uint32_t j = 0; j < _no_of_switches; j++) {
        for (uint32_t k = 0; k < _no_of_switches; k++) {
            queues_switch_switch[j][k] = NULL;
            pipes_switch_switch[j][k] = NULL;
        }

        for (uint32_t k = 0; k < _no_of_nodes; k++) {
            queues_switch_host[j][k] = NULL;
            pipes_switch_host[j][k] = NULL;
            queues_host_switch[k][j] = NULL;
            pipes_host_switch[k][j] = NULL;
        }
    }

    for (uint32_t j = 0; j < _no_of_switches; j++) {
        switches[j] = new HammingmeshSwitch(*_eventlist, "Switch_" + ntoa(j), HammingmeshSwitch::GENERAL, j, timeFromUs((uint32_t)0), this, _hm_routing_strategy);
    }

    // Creates all links between Switches and Hosts.
    for (uint32_t j = 0; j < _no_of_nodes; j++) {
        uint32_t k = j;

        // Switch to Host (Downlink):
        queueLogger = new QueueLoggerSampling(timeFromUs(10u), *_eventlist);
        //logfile->addLogger(*queueLogger);

        queues_switch_host[j][k] = alloc_queue(queueLogger, _queuesize, true);
        queues_switch_host[j][k]->setName("SW" + ntoa(j) + "->DST" + ntoa(k));
        //logfile->writeName(*(queues_switch_host[j][k]));

        pipes_switch_host[j][k] = new Pipe(timeFromUs(RTT), *_eventlist);
        pipes_switch_host[j][k]->setName("Pipe-SW" + ntoa(j) + "->DST" + ntoa(k));
        //logfile->writeName(*(pipes_switch_host[j][k]));

        // Host to Switch (Uplink):
        queueLogger = new QueueLoggerSampling(timeFromMs(1000), *_eventlist);
        //logfile->addLogger(*queueLogger);

        queues_host_switch[k][j] = alloc_src_queue(queueLogger);
        queues_host_switch[k][j]->setName("SRC" + ntoa(k) + "->SW" + ntoa(j));
        //logfile->writeName(*(queues_host_switch[k][j]));
        
        queues_host_switch[k][j]->setRemoteEndpoint(switches[j]);
        // queues_switch_host[j][k]->setRemoteEndpoint(switches[k]);

        switches[j]->addPort(queues_switch_host[j][k]);
        switches[j]->addPort(queues_host_switch[k][j]);

        if (_qt == LOSSLESS) {
            switches[j]->addPort(queues_switch_host[j][k]);
            ((LosslessQueue *)queues_switch_host[j][k])->setRemoteEndpoint(queues_host_switch[k][j]);
        } else if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN) {
            // No virtual queue is needed at the server.
            new LosslessInputQueue(*_eventlist, queues_host_switch[k][j]);
        }

        pipes_host_switch[k][j] = new Pipe(timeFromUs(RTT), *_eventlist);
        pipes_host_switch[k][j]->setName("Pipe-SRC" + ntoa(k) + "->SW" + ntoa(j));
        //logfile->writeName(*(pipes_host_switch[k][j]));
    }

    // Creates all links between Switches and Switches.
    // Creates all on-board links between Switches and Switches.
    uint32_t board_size = _height_board * _width_board;
    for (uint32_t i = 0; i < board_size; i++){
        if (i % _width_board != 0){
            for (uint32_t j = 0; j < _no_of_groups; j++){
                uint32_t k = j * board_size + i;
                uint32_t l = k - 1;
                create_switch_switch_link(k, l, queueLogger);
            }
        }
        if (i % _width_board != _width_board - 1){
            for (uint32_t j = 0; j < _no_of_groups; j++){
                uint32_t k = j * board_size + i;
                uint32_t l = k + 1;
                create_switch_switch_link(k, l, queueLogger);
            }
        }
        if (i / _width_board != 0){
            for (uint32_t j = 0; j < _no_of_groups; j++){
                uint32_t k = j * board_size + i;
                uint32_t l = k - _width_board;
                create_switch_switch_link(k, l, queueLogger);
            }
        }
        if (i / _width_board != _height_board - 1){
            for (uint32_t j = 0; j < _no_of_groups; j++){
                uint32_t k = j * board_size + i;
                uint32_t l = k + _width_board;
                create_switch_switch_link(k, l, queueLogger);
            }
        }
    }

    // Creates all global links between Switches and Switches.
    // uint32_t total_height = _height * _height_board;
    uint32_t total_width = _width * _width_board;
    uint32_t group_size = _height_board * _width_board;
    
    int height_fat_tree_l1;
    int height_fat_tree_l2;
    if(2 * _height <= 64){
        height_fat_tree_l1 = 1;
        height_fat_tree_l2 = 0;
        for(u_int32_t i = 0; i < _width; i++){
            for(u_int32_t j = 0; j < _width_board; j++){
                for(u_int32_t k = 0; k < _height; k++){
                    uint32_t group_base = group_size * _width * k + group_size * i;
                    uint32_t north = group_base + j;
                    uint32_t south = group_base + (_height_board - 1) * _width_board + j;
                    uint32_t tree = _no_of_nodes + i * _width_board + j;
                    create_switch_switch_link(north, tree, queueLogger);
                    create_switch_switch_link(south, tree, queueLogger);
                }
            }
        }
    }
    else if(2 * _height <= 64 * 63){
        if((2 * _height) % 63 == 0){
            height_fat_tree_l1 = (2 * _height) / 63;
        }
        else{
            height_fat_tree_l1 = ((2 * _height) / 63) + 1;
        }
        height_fat_tree_l2 = 1;
        for(u_int32_t i = 0; i < _width; i++){
            for(uint32_t j = 0; j < _width_board; j++){
                for(uint32_t k = 0; k < _height; k++){
                    uint32_t group_base = group_size * _width * k + group_size * i;
                    uint32_t north = group_base + j;
                    uint32_t south = group_base + (_height_board - 1) * _width_board + j;
                    uint32_t tree_north = _no_of_nodes + (i * _width_board + j) * (height_fat_tree_l1 + height_fat_tree_l2) + (2 * k) / 63;
                    uint32_t tree_south = _no_of_nodes + (i * _width_board + j) * (height_fat_tree_l1 + height_fat_tree_l2) + ((2 * k) + 1) / 63;
                    create_switch_switch_link(north, tree_north, queueLogger);
                    create_switch_switch_link(south, tree_south, queueLogger);
                }
                
                uint32_t tree_base = _no_of_nodes + (i * _width_board + j) * (height_fat_tree_l1 + height_fat_tree_l2);
                uint32_t l2_node = tree_base + height_fat_tree_l1;
                for(int k = 0; k < height_fat_tree_l1; k++){
                    uint32_t l1_node = tree_base + k;
                    create_switch_switch_link(l1_node, l2_node, queueLogger);
                }
            }
        }
    }
    else{
        abort();
    }

    uint32_t width_tree_base = _no_of_nodes + total_width * (height_fat_tree_l1 + height_fat_tree_l2);
    int width_fat_tree_l1;
    int width_fat_tree_l2;
    if(2 * _width <= 64){
        width_fat_tree_l1 = 1;
        width_fat_tree_l2 = 0;
        for(uint32_t i = 0; i < _height; i++){
            for(uint32_t j = 0; j < _height_board; j++){
                for(uint32_t k = 0; k < _width; k++){
                    uint32_t group_base = group_size * _width * i + group_size * k;
                    uint32_t west = group_base + _width_board * j;
                    uint32_t east = group_base + _width_board * j + (_width_board - 1);
                    uint32_t tree = width_tree_base + i * _height_board + j;
                    create_switch_switch_link(west, tree, queueLogger);
                    create_switch_switch_link(east, tree, queueLogger);
                }
            }
        }
    }
    else if(2 * _width <= 64 * 63){
        if((2 * _width) % 63 == 0){
            width_fat_tree_l1 = (2 * _width) / 63;
        }
        else{
            width_fat_tree_l1 = ((2 * _width) / 63) + 1;
        }
        width_fat_tree_l2 = 1;
        for(uint32_t i = 0; i < _height; i++){
            for(uint32_t j = 0; j < _height_board; j++){
                for(uint32_t k = 0; k < _width; k++){
                    uint32_t group_base = group_size * _width * i + group_size * k;
                    uint32_t west = group_base + _width_board * j;
                    uint32_t east = group_base + _width_board * j + (_width_board - 1);
                    uint32_t tree_west = width_tree_base + (i * _height_board + j) * (width_fat_tree_l1 + width_fat_tree_l2) + (2 * k) / 63;
                    uint32_t tree_east = width_tree_base + (i * _height_board + j) * (width_fat_tree_l1 + width_fat_tree_l2) + ((2 * k) + 1) / 63;
                    create_switch_switch_link(west, tree_west, queueLogger);
                    create_switch_switch_link(east, tree_east, queueLogger);
                }
                
                uint32_t tree_base = width_tree_base + (i * _height_board + j) * (width_fat_tree_l1 + width_fat_tree_l2);
                uint32_t l2_node = tree_base + width_fat_tree_l1;
                for(int k = 0; k < width_fat_tree_l1; k++){
                    uint32_t l1_node = tree_base + k;
                    create_switch_switch_link(l1_node, l2_node, queueLogger);
                }
            }
        }
    }
    else{
        abort();
    }

    // Initialize thresholds for lossless operations.
    if (_qt == LOSSLESS) {
        for (uint32_t j = 0; j < _no_of_switches; j++) {
            switches[j]->configureLossless();
        }
    }

    // Prints all connections in the topology.
    /* for (int i = 0; i < _no_of_switches; i++){
        for (int j = i+1; j < _no_of_switches; j++){
            if(!pipes_switch_switch[i][j] == NULL){
                printf("%d <-> %d\n", i, j);
            }
        }
        printf("\n");
    } */
}