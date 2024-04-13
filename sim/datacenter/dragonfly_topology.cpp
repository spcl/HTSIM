// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "dragonfly_topology.h"
#include "string.h"
#include <sstream>
#include <vector>

#include "compositequeue.h"
#include "compositequeuebts.h"
#include "dragonfly_switch.h"
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

// Convertes: Number (double) TO Ascii (string); Integer (uint64_t) TO Ascii (string).
string ntoa(double n);
string itoa(uint64_t n);

// Creates a new instance of a Dragonfly topology.
DragonflyTopology::DragonflyTopology(uint32_t p, uint32_t a, uint32_t h, mem_b queuesize, EventList *ev, queue_type q) {
    //  p = number of hosts per router.
    //  a = number of routers per group.
    //  h = number of links used to connect to other groups per router.
    // qt = queue type.

    if (p < 1 || a < 1 || h < 1) {
        cerr << "p, a, h all have to be positive." << endl;
    }

    logfile = NULL;

    _p = p;
    _a = a;
    _h = h;

    _queuesize = queuesize;
    _eventlist = ev;
    _qt = q;

    _no_of_groups = _a *_h + 1;
    _no_of_switches = _no_of_groups * _a;
    _no_of_nodes = _no_of_switches * _p;

    std::cout << "DragonFly topology with " << _p << " hosts per router, " << _a << " routers per group and "
         << _no_of_groups << " groups, total nodes " << _no_of_nodes << endl;
    std::cout << "Queue type " << _qt << endl;

    init_pipes_queues();
    init_network();
    printf("Dragonfly1::_p = %u\n", _p);
}

DragonflyTopology::DragonflyTopology(uint32_t p, uint32_t a, uint32_t h, mem_b queuesize, EventList *ev, queue_type q, uint32_t strat) {
    _df_routing_strategy = strat;
    //  p = number of hosts per router.
    //  a = number of routers per group.
    //  h = number of links used to connect to other groups.
    // qt = queue type.

    if (p < 1 || a < 1 || h < 1) {
        cerr << "p, a, h all have to be positive." << endl;
    }

    logfile = NULL;

    _p = p;
    _a = a;
    _h = h;

    _queuesize = queuesize;
    _eventlist = ev;
    _qt = q;

    _no_of_groups = _a *_h + 1;
    _no_of_switches = _no_of_groups * _a;
    _no_of_nodes = _no_of_switches * _p;

    std::cout << "DragonFly topology with " << _p << " hosts per router, " << _a << " routers per group and "
         << _no_of_groups << " groups, total nodes " << _no_of_nodes << endl;
    std::cout << "Queue type " << _qt << endl;

    init_pipes_queues();
    init_network();
    // printf("Dragonfly2::_p = %u\n", _p);
}

// Initializes all pipes and queues for the switches.
void DragonflyTopology::init_pipes_queues() {
    switches.resize(_no_of_switches, NULL);
    
    pipes_host_switch.resize(_no_of_nodes, vector<Pipe *>(_no_of_switches));
    pipes_switch_switch.resize(_no_of_switches, vector<Pipe *>(_no_of_switches));
    pipes_switch_host.resize(_no_of_switches, vector<Pipe *>(_no_of_nodes));
    
    queues_host_switch.resize(_no_of_nodes, vector<Queue *>(_no_of_switches));
    queues_switch_switch.resize(_no_of_switches, vector<Queue *>(_no_of_switches));
    queues_switch_host.resize(_no_of_switches, vector<Queue *>(_no_of_nodes));
}

vector<const Route *> *DragonflyTopology::get_bidir_paths(uint32_t src, uint32_t dest, bool reverse){return NULL;}

// Creates an instance of a fair priority queue. It is used for packet input.
Queue *DragonflyTopology::alloc_src_queue(QueueLogger *queueLogger) {
    return new FairPriorityQueue(speedFromMbps((uint64_t)HOST_NIC), memFromPkt(FEEDER_BUFFER), *_eventlist,
                                 queueLogger);
}

// Allocates a queue with a default link speed set to HOST_NIC.
Queue *DragonflyTopology::alloc_queue(QueueLogger *queueLogger, mem_b queuesize, bool tor = false) {
    return alloc_queue(queueLogger, HOST_NIC, queuesize, tor);
}

// Creates an instance of a queue based on the type which is needed.
Queue *DragonflyTopology::alloc_queue(QueueLogger *queueLogger, uint64_t speed, mem_b queuesize, bool tor) {
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
        return new LosslessOutputQueue(speedFromMbps(speed), memFromPkt(10000), *_eventlist, queueLogger, 1,
                                       memFromPkt(16));
    else if (_qt == COMPOSITE_ECN) {
        if (tor)
            return new CompositeQueue(speedFromMbps(speed), queuesize, *_eventlist, queueLogger);
        else
            return new ECNQueue(speedFromMbps(speed), memFromPkt(2 * SWITCH_BUFFER), *_eventlist, queueLogger,
                                memFromPkt(15));
    }
    assert(0);
}

// Connects all nodes to a dragonfly network.
void DragonflyTopology::init_network() {
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

    // Creates switches if it is a lossless operation.
    // ???
    for (uint32_t j = 0; j < _no_of_switches; j++) {
        switches[j] = new DragonflySwitch(*_eventlist, "Switch_" + ntoa(j), DragonflySwitch::GENERAL, j, timeFromUs((uint32_t)0), this, _df_routing_strategy);
    }

    // Creates all links between Switches and Hosts.
    for (uint32_t j = 0; j < _no_of_switches; j++) {
        for (uint32_t l = 0; l < _p; l++) {
            uint32_t k = j * _p + l;

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
            queues_switch_host[j][k]->setRemoteEndpoint(switches[k]);

            switches[j]->addPort(queues_switch_host[j][k]);
            switches[k]->addPort(queues_host_switch[k][j]);

            // ???
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
    }

    // Creates all links between Switches and Switches.
    for (uint32_t j = 0; j < _no_of_switches; j++) {
        uint32_t groupid = j / _a;

        // Connect the switch to other switches in the same group, with higher
        // IDs (full mesh within group).
        for (uint32_t k = j + 1; k < (groupid + 1) * _a; k++) {
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

            // ???
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

        // Connect the switch to switches from other groups. Global links.
        for (uint32_t l = 0; l < _h; l++) {
            uint32_t targetgroupid = (j % _a) * _h + l + 1;

            // Compute target switch ID; only create links to groups with ID
            // larger than ours. if the ID is larger than ours, effective target
            // group is +1 (we skip our own group for global links).
            if (targetgroupid <= groupid){
                continue;
            }

            uint32_t k = targetgroupid * _a + groupid / _h;

            // Downlink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *_eventlist);
            //logfile->addLogger(*queueLogger);

            queues_switch_switch[k][j] = alloc_queue(queueLogger, _queuesize);
            queues_switch_switch[k][j]->setName("SW" + ntoa(k) + "-G->SW" + ntoa(j));
            //logfile->writeName(*(queues_switch_switch[k][j]));

            pipes_switch_switch[k][j] = new Pipe(timeFromUs(RTT), *_eventlist);
            pipes_switch_switch[k][j]->setName("Pipe-SW" + ntoa(k) + "-G->SW" + ntoa(j));
            //logfile->writeName(*(pipes_switch_switch[k][j]));

            // Uplink
            queueLogger = new QueueLoggerSampling(timeFromMs(1000), *_eventlist);
            //logfile->addLogger(*queueLogger);
            
            queues_switch_switch[j][k] = alloc_queue(queueLogger, _queuesize, true);
            queues_switch_switch[j][k]->setName("SW" + ntoa(j) + "-G->SW" + ntoa(k));
            //logfile->writeName(*(queues_switch_switch[j][k]));

            switches[k]->addPort(queues_switch_switch[k][j]);
            switches[j]->addPort(queues_switch_switch[j][k]);
            queues_switch_switch[k][j]->setRemoteEndpoint(switches[j]);
            queues_switch_switch[j][k]->setRemoteEndpoint(switches[k]);

            // ???
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
            pipes_switch_switch[j][k]->setName("Pipe-SW" + ntoa(j) + "-G->SW" + ntoa(k));
            //logfile->writeName(*(pipes_switch_switch[j][k]));
        }
    }

    // Initialize thresholds for lossless operations.
    if (_qt == LOSSLESS) {
        for (uint32_t j = 0; j < _no_of_switches; j++) {
            switches[j]->configureLossless();
        }
    }

/*     for (int i = 0; i < _no_of_switches; i++){
        for (int j = i+1; j < _no_of_switches; j++){
            if(!pipes_switch_switch[i][j] == NULL){
                printf("%d <-> %d\n", i, j);
            }
        }
        printf("\n");
    } */
}