// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "b-cube_topology.h"
#include "string.h"
#include <sstream>
#include <vector>

#include "compositequeue.h"
#include "compositequeuebts.h"
#include "b-cube_switch.h"
#include "ecnqueue.h"
#include "main.h"
#include "prioqueue.h"
#include "queue.h"
#include "queue_lossless.h"
#include "queue_lossless_input.h"
#include "queue_lossless_output.h"
#include "swift_scheduler.h"
#include <iostream>

/* All operations with loggerfiles have been commented out because they weren't used
    but haven't been removed to keep the possibility to easely work with them if needed. */

// RoundTrip Time.
extern uint32_t RTT;

// Convertes: Number (double) TO Ascii (string); Integer (uint64_t) TO Ascii (string).
string ntoa(double n);
string itoa(uint64_t n);

// Creates a new instance of a Dragonfly topology.
BCubeTopology::BCubeTopology(uint32_t n, uint32_t k, mem_b queuesize, EventList *ev, queue_type q) {
    //  n = number of hosts per router.
    //  k = number of levels in the B-Cube.
    // qt = queue type.

    if (n < 1 || k < 1) {
        cerr << "n, k all have to be positive." << endl;
    }

    logfile = NULL;

    _n = n;
    _k = k;

    _queuesize = queuesize;
    _eventlist = ev;
    _qt = q;

    _no_of_nodes = (uint32_t)pow(_n, _k);
    _no_of_switches = _k * (uint32_t)pow(_n, _k-1) + _no_of_nodes;

    std::cout << "BCube topology with " << _n << " hosts per router, " << _k << " levels, "
         << _no_of_switches << " switches and " << _no_of_nodes << " nodes." << endl;
    std::cout << "Queue type " << _qt << endl;

    init_pipes_queues();
    init_network();
}

BCubeTopology::BCubeTopology(uint32_t n, uint32_t k, mem_b queuesize, EventList *ev, queue_type q, uint32_t strat) {
    _bc_routing_strategy = strat;
    //  n = number of hosts per router.
    //  k = number of levels in the B-Cube.
    // qt = queue type.

    if (n < 1 || k < 1) {
        cerr << "n, k all have to be positive." << endl;
    }

    logfile = NULL;

    _n = n;
    _k = k;

    _queuesize = queuesize;
    _eventlist = ev;
    _qt = q;

    _no_of_nodes = (uint32_t)pow(_n, _k);
    _no_of_switches = _k * (uint32_t)pow(_n, _k-1) + _no_of_nodes;

    std::cout << "BCube topology with " << _n << " hosts per router, " << _k << " levels, "
         << _no_of_switches << " switches and " << _no_of_nodes << " nodes." << endl;
    std::cout << "Queue type " << _qt << endl;

    init_pipes_queues();
    init_network();
}

// Initializes all pipes and queues for the switches.
void BCubeTopology::init_pipes_queues() {
    switches.resize(_no_of_switches, NULL);
    
    pipes_host_switch.resize(_no_of_nodes, vector<Pipe *>(_no_of_switches));
    pipes_switch_switch.resize(_no_of_switches, vector<Pipe *>(_no_of_switches));
    pipes_switch_host.resize(_no_of_switches, vector<Pipe *>(_no_of_nodes));
    
    queues_host_switch.resize(_no_of_nodes, vector<Queue *>(_no_of_switches));
    queues_switch_switch.resize(_no_of_switches, vector<Queue *>(_no_of_switches));
    queues_switch_host.resize(_no_of_switches, vector<Queue *>(_no_of_nodes));
}

vector<const Route *> *BCubeTopology::get_bidir_paths(uint32_t src, uint32_t dest, bool reverse){return NULL;}

// Creates an instance of a fair priority queue. It is used for packet input.
Queue *BCubeTopology::alloc_src_queue(QueueLogger *queueLogger) {
    return new FairPriorityQueue(speedFromMbps((uint64_t)HOST_NIC), memFromPkt(FEEDER_BUFFER), *_eventlist, queueLogger);
}

// Allocates a queue with a default link speed set to HOST_NIC.
Queue *BCubeTopology::alloc_queue(QueueLogger *queueLogger, mem_b queuesize, bool tor = false) {
    return alloc_queue(queueLogger, HOST_NIC, queuesize, tor);
}

// Creates an instance of a queue based on the type which is needed.
Queue *BCubeTopology::alloc_queue(QueueLogger *queueLogger, uint64_t speed, mem_b queuesize, bool tor) {
    if (_qt == RANDOM)
        return new RandomQueue(speedFromMbps(speed), queuesize, *_eventlist, queueLogger, memFromPkt(RANDOM_BUFFER));
    else if (_qt == COMPOSITE)
        return new CompositeQueue(speedFromMbps(speed), queuesize, *_eventlist, queueLogger);
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

// Connects all nodes to a b-cube network.
void BCubeTopology::init_network() {
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

    // Creates the switches.
    for (uint32_t j = 0; j < _no_of_switches; j++) {
        switches[j] = new BCubeSwitch(*_eventlist, "Switch_" + ntoa(j), BCubeSwitch::GENERAL, j, timeFromUs((uint32_t)0), this, _bc_routing_strategy);
    }

    // Creates all links between Switches and Hosts.
    for (uint32_t j = 0; j < _no_of_nodes; j++) {
        // Switch to Host (Downlink):
        queueLogger = new QueueLoggerSampling(timeFromUs(10u), *_eventlist);
        //logfile->addLogger(*queueLogger);

        queues_switch_host[j][j] = alloc_queue(queueLogger, _queuesize, true);
        queues_switch_host[j][j]->setName("SW" + ntoa(j) + "->DST" + ntoa(j));
        //logfile->writeName(*(queues_switch_host[j][j]));

        pipes_switch_host[j][j] = new Pipe(timeFromUs(RTT), *_eventlist);
        pipes_switch_host[j][j]->setName("Pipe-SW" + ntoa(j) + "->DST" + ntoa(j));
        //logfile->writeName(*(pipes_switch_host[j][j]));

        // Host to Switch (Uplink):
        queueLogger = new QueueLoggerSampling(timeFromMs(1000), *_eventlist);
        //logfile->addLogger(*queueLogger);

        queues_host_switch[j][j] = alloc_src_queue(queueLogger);
        queues_host_switch[j][j]->setName("SRC" + ntoa(j) + "->SW" + ntoa(j));
        //logfile->writeName(*(queues_host_switch[j][j]));
        
        queues_host_switch[j][j]->setRemoteEndpoint(switches[j]);
        // queues_switch_host[j][j]->setRemoteEndpoint(switches[j]);

        switches[j]->addPort(queues_switch_host[j][j]);
        switches[j]->addPort(queues_host_switch[j][j]);

        if (_qt == LOSSLESS) {
            switches[j]->addPort(queues_switch_host[j][j]);
            ((LosslessQueue *)queues_switch_host[j][j])->setRemoteEndpoint(queues_host_switch[j][j]);
        } else if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN) {
            // No virtual queue is needed at the server.
            new LosslessInputQueue(*_eventlist, queues_host_switch[j][j]);
        }

        pipes_host_switch[j][j] = new Pipe(timeFromUs(RTT), *_eventlist);
        pipes_host_switch[j][j]->setName("Pipe-SRC" + ntoa(j) + "->SW" + ntoa(j));
        //logfile->writeName(*(pipes_host_switch[j][j]));
    }

    uint32_t no_switches_per_layer = (uint32_t)pow(_n, _k-1);

    // Creates all links between Switches and Switches.
    for (uint32_t i = 0; i < _k; i++) {
        for (uint32_t j = 0; j < no_switches_per_layer; j++){
            uint32_t a = _no_of_nodes + i * no_switches_per_layer + j;
            uint32_t level_size = (uint32_t)pow(_n, i);
            uint32_t block_number = j / level_size;
            uint32_t intra_block_number = j % level_size;
            for (uint32_t k = 0; k < _n; k++){
                uint32_t b = block_number * level_size * _n + k * level_size + intra_block_number;

                // Downlink
                queueLogger = new QueueLoggerSampling(timeFromMs(1000), *_eventlist);
                //logfile->addLogger(*queueLogger);

                queues_switch_switch[b][a] = alloc_queue(queueLogger, _queuesize);
                queues_switch_switch[b][a]->setName("SW" + ntoa(b) + "-I->SW" + ntoa(a));
                //logfile->writeName(*(queues_switch_switch[b][a]));

                pipes_switch_switch[b][a] = new Pipe(timeFromUs(RTT), *_eventlist);
                pipes_switch_switch[b][a]->setName("Pipe-SW" + ntoa(b) + "-I->SW" + ntoa(a));
                //logfile->writeName(*(pipes_switch_switch[b][a]));

                // Uplink
                queueLogger = new QueueLoggerSampling(timeFromMs(1000), *_eventlist);
                //logfile->addLogger(*queueLogger);

                queues_switch_switch[a][b] = alloc_queue(queueLogger, _queuesize, true);
                queues_switch_switch[a][b]->setName("SW" + ntoa(a) + "-I->SW" + ntoa(b));
                //logfile->writeName(*(queues_switch_switch[a][b]));

                switches[a]->addPort(queues_switch_switch[a][b]);
                switches[b]->addPort(queues_switch_switch[b][a]);
                queues_switch_switch[a][b]->setRemoteEndpoint(switches[b]);
                queues_switch_switch[b][a]->setRemoteEndpoint(switches[a]);

                if (_qt == LOSSLESS) {
                    switches[a]->addPort(queues_switch_switch[a][b]);
                    ((LosslessQueue *)queues_switch_switch[a][b])->setRemoteEndpoint(queues_switch_switch[b][a]);
                    switches[b]->addPort(queues_switch_switch[b][a]);
                    ((LosslessQueue *)queues_switch_switch[b][a])->setRemoteEndpoint(queues_switch_switch[a][b]);
                } else if (_qt == LOSSLESS_INPUT || _qt == LOSSLESS_INPUT_ECN) {
                    new LosslessInputQueue(*_eventlist, queues_switch_switch[a][b]);
                    new LosslessInputQueue(*_eventlist, queues_switch_switch[b][a]);
                }

                pipes_switch_switch[a][b] = new Pipe(timeFromUs(RTT), *_eventlist);
                pipes_switch_switch[a][b]->setName("Pipe-SW" + ntoa(a) + "-I->SW" + ntoa(b));
                //logfile->writeName(*(pipes_switch_switch[a][b]));
            }
        }
    }

    // Initialize thresholds for lossless operations.
    if (_qt == LOSSLESS) {
        for (uint32_t j = 0; j < _no_of_switches; j++) {
            switches[j]->configureLossless();
        }
    }

// Prints out all connections in the topology. Debugging use only.
    for (uint32_t i = 0; i < _no_of_switches; i++){
        for (uint32_t j = i+1; j < _no_of_switches; j++){
            if(!pipes_switch_switch[i][j] == NULL){
                printf("%u <-> %u\n", i, j);
            }
        }
        printf("\n");
    }
}