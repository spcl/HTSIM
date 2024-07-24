// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "slimfly_topology.h"
#include "string.h"
#include <sstream>
#include <vector>

#include "compositequeue.h"
#include "compositequeuebts.h"
#include "slimfly_switch.h"
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
bool SlimflyTopology::_enable_ecn = false;
mem_b SlimflyTopology::_ecn_low = 0;
mem_b SlimflyTopology::_ecn_high = 0;

// Convertes: Number (double) TO Ascii (string); Integer (uint64_t) TO Ascii (string).
string ntoa(double n);
string itoa(uint64_t n);

// Creates a new instance of a Dragonfly topology.
SlimflyTopology::SlimflyTopology(uint32_t p, uint32_t q_base, uint32_t q_exp, mem_b queuesize, EventList *ev, queue_type qt, simtime_picosec hop_latency, simtime_picosec short_hop_latency) {
    //  p       = number of hosts per router.
    //  q_base  = base of q (must be a prime).
    //  q_exp   = exponent of q (> 0; and for q_base = 2: > 1).
    //  qt      = queue type.

    if (p < 1 || q_base < 1 || q_exp < 1) {
        cerr << "p, q_base, q_exp all have to be positive." << endl;
    }
    if (q_base == 2 && q_exp == 1){
        cerr << "For q_base == 2 q_exp must be > 1." << endl;
    }
    if (!is_prime(q_base)){
        cerr << "q_base must be a prime number." << endl;
    }
    if (q_exp > 1){
        cerr << "The extended Galois fields are not implemented yet." << endl;
    }

    logfile = NULL;

    _p = p;
    _q_base = q_base;
    _q_exp = q_exp;
    
    _q = pow(q_base, q_exp);
    _xi = get_generator(_q);

    uint32_t q_half;

    uint32_t l = _q / 4;
    int _delta;
    if (4 * l == _q){
        _delta = 0;

        q_half = _q / 2;
        _X.resize(q_half, 0);
        _Xp.resize(q_half, 0);
        uint32_t x = 1;
        uint32_t xp = _xi;
        for (uint32_t i = 0; i < q_half; i++) {
            _X[i] = x;
            _Xp[i] = xp;
            x = (x * (_xi * _xi)) % _q;
            xp = (xp * (_xi * _xi)) % _q;
        }
    }
    else if (4 * l + 1 == _q){
        _delta = 1;

        q_half = _q / 2;
        _X.resize(q_half, 0);
        _Xp.resize(q_half, 0);
        uint32_t x = 1;
        uint32_t xp = _xi;
        for (uint32_t i = 0; i < q_half; i++) {
            _X[i] = x;
            _Xp[i] = xp;
            x = (x * (_xi * _xi)) % _q;
            xp = (xp * (_xi * _xi)) % _q;
        }
    }
    else{
        _delta = -1;
        l++;

        q_half = (_q - 1) / 2;
        _X.resize(q_half, 0);
        _Xp.resize(q_half, 0);
        uint32_t x = 1;
        uint32_t xp = _xi;
        for (uint32_t i = 0; i < l; i++) {
            _X[i] = x;
            _Xp[i] = xp;
            x = (x * (_xi * _xi)) % _q;
            xp = (xp * (_xi * _xi)) % _q;
        }
        x = (_X[l-1] * _xi) % _q;
        xp = (_Xp[l-1] * _xi) % _q;
        for (uint32_t i = l; i < q_half; i++) {
            _X[i] = x;
            _Xp[i] = xp;
            x = (x * (_xi * _xi)) % _q;
            xp = (xp * (_xi * _xi)) % _q;
        }
    }

    _queuesize = queuesize;
    _eventlist = ev;
    _qt = qt;

    _no_of_switches = 2 * pow(_q, 2);
    _no_of_nodes = _no_of_switches * _p;

    _hop_latency = hop_latency;
    if(short_hop_latency == 0){
        _short_hop_latency = hop_latency;
    }
    else{
        _short_hop_latency = short_hop_latency;
    }

    std::cout << "SlimFly topology with " << _p << " hosts per switch and " << _no_of_switches << " switches, total nodes " << _no_of_nodes << "." << endl;
    std::cout << "Queue type " << _qt << endl;

    init_pipes_queues();
    init_network();
}

SlimflyTopology::SlimflyTopology(uint32_t p, uint32_t q_base, uint32_t q_exp, mem_b queuesize, EventList *ev, queue_type qt, simtime_picosec hop_latency, simtime_picosec short_hop_latency, uint32_t strat) {
    _sf_routing_strategy = strat;
    printf("_sf_routing_strategy = %u\n", _sf_routing_strategy);
    //  p       = number of hosts per router.
    //  q_base  = base of q (must be a prime).
    //  q_exp   = exponent of q (> 0; and for q_base = 2: > 1).
    //  qt      = queue type.

    if (p < 1 || q_base < 1 || q_exp < 1) {
        cerr << "p, q_base, q_exp all have to be positive." << endl;
    }
    if (q_base == 2 && q_exp == 1){
        cerr << "For q_base == 2 q_exp must be > 1." << endl;
    }
    if (!is_prime(q_base)){
        cerr << "q_base must be a prime number." << endl;
    }

    logfile = NULL;

    _p = p;
    _q_base = q_base;
    _q_exp = q_exp;
    
    _q = pow(q_base, q_exp);
    _xi = get_generator(_q);

    uint32_t q_half;

    uint32_t l = _q / 4;
    int _delta;
    if (4 * l == _q){
        _delta = 0;

        q_half = _q / 2;
        _X.resize(q_half, 0);
        _Xp.resize(q_half, 0);
        uint32_t x = 1;
        uint32_t xp = _xi;
        for (uint32_t i = 0; i < q_half; i++) {
            _X[i] = x;
            _Xp[i] = xp;
            x = (x * (_xi * _xi)) % _q;
            xp = (xp * (_xi * _xi)) % _q;
        }
    }
    else if (4 * l + 1 == _q){
        _delta = 1;

        q_half = _q / 2;
        _X.resize(q_half, 0);
        _Xp.resize(q_half, 0);
        uint32_t x = 1;
        uint32_t xp = _xi;
        for (uint32_t i = 0; i < q_half; i++) {
            _X[i] = x;
            _Xp[i] = xp;
            x = (x * (_xi * _xi)) % _q;
            xp = (xp * (_xi * _xi)) % _q;
        }
    }
    else{
        _delta = -1;
        l++;

        q_half = (_q + 1) / 2;
        _X.resize(q_half, 0);
        _Xp.resize(q_half, 0);
        uint32_t x = 1;
        uint32_t xp = _xi;
        for (uint32_t i = 0; i < l; i++) {
            _X[i] = x;
            _Xp[i] = xp;
            x = (x * (_xi * _xi)) % _q;
            xp = (xp * (_xi * _xi)) % _q;
        }
        x = (_X[l-1] * _xi) % _q;
        xp = (_Xp[l-1] * _xi) % _q;
        for (uint32_t i = l; i < q_half; i++) {
            _X[i] = x;
            _Xp[i] = xp;
            x = (x * (_xi * _xi)) % _q;
            xp = (xp * (_xi * _xi)) % _q;
        }
    }

    printf("_X = [");
    for (uint32_t i = 0; i < q_half-1; i++) {
        printf("%u, ", _X[i]);
    }
    printf("%u]\n", _X[q_half-1]);
    printf("_Xp = [");
    for (uint32_t i = 0; i < q_half-1; i++) {
        printf("%u, ", _Xp[i]);
    }
    printf("%u]\n", _Xp[q_half-1]);

    _queuesize = queuesize;
    _eventlist = ev;
    _qt = qt;

    _no_of_switches = 2 * pow(_q, 2);
    _no_of_nodes = _no_of_switches * _p;

    _hop_latency = hop_latency;
    if(short_hop_latency == 0){
        _short_hop_latency = hop_latency;
    }
    else{
        _short_hop_latency = short_hop_latency;
    }

    std::cout << "SlimFly topology with " << _p << " hosts per switch and " << _no_of_switches << " switches, total nodes " << _no_of_nodes << "." << endl;
    std::cout << "Queue type " << _qt << endl;

    init_pipes_queues();
    init_network();
}

// Given a prime field defined by q this function gets a generator for the field.
// That means a number which when multiplied by itself (modulo) can span the whole field.
uint32_t SlimflyTopology::get_generator(uint32_t q) {
    if (q == 2){ return 1; }
    if (q == 3){ return 2; }
    for (uint32_t i = 2; i < (q - 1); i++){
        uint32_t gen = i;
        uint32_t r = i;
        for (uint32_t j = 2; j < (q - 1); j++){
            r = (r * gen) % q;
            if (r == 1){ break; }
        }
        r = (r * gen) % q;
        if (r == 1){ printf("gen = %u\n", gen); return gen; }
    }
    cerr << "Wasn't able to find a generator for the group." << endl;
    return 0;
}

bool SlimflyTopology::is_prime(uint32_t q_base){
    if (q_base < 2){ return false; }
    if (q_base == 2){ return true; }
    uint32_t r = sqrt(q_base);
    for(uint32_t i = 3; i <= r; i++){
        if(q_base % i == 0){ return false; }
    }
    return true;
}

bool SlimflyTopology::is_element(vector<uint32_t> arr, uint32_t el){
    int size = arr.size();
    for (int i = 0; i < size; i++){
        if(arr[i] == el) {return true;}
    }
    return false;
}

// In contrast to the standard modulo (%) this one always gives positive results.
uint32_t SlimflyTopology::modulo (int x, int y){
    int res = x % y;
    if (res < 0){
        res += y;
    }
    return res;
}

// Initializes all pipes and queues for the switches.
void SlimflyTopology::init_pipes_queues() {
    switches.resize(_no_of_switches, NULL);
    
    pipes_host_switch.resize(_no_of_nodes, vector<Pipe *>(_no_of_switches));
    pipes_switch_switch.resize(_no_of_switches, vector<Pipe *>(_no_of_switches));
    pipes_switch_host.resize(_no_of_switches, vector<Pipe *>(_no_of_nodes));
    
    queues_host_switch.resize(_no_of_nodes, vector<Queue *>(_no_of_switches));
    queues_switch_switch.resize(_no_of_switches, vector<Queue *>(_no_of_switches));
    queues_switch_host.resize(_no_of_switches, vector<Queue *>(_no_of_nodes));
}

// Creates an instance of a fair priority queue. It is used for packet input.
Queue *SlimflyTopology::alloc_src_queue(QueueLogger *queueLogger) {
    return new FairPriorityQueue(speedFromMbps((uint64_t)HOST_NIC), memFromPkt(FEEDER_BUFFER), *_eventlist,
                                 queueLogger);
}

// Allocates a queue with a default link speed set to HOST_NIC.
Queue *SlimflyTopology::alloc_queue(QueueLogger *queueLogger, mem_b queuesize, bool tor = false) {
    return alloc_queue(queueLogger, HOST_NIC, queuesize, tor);
}

// Creates an instance of a queue based on the type which is needed.
Queue *SlimflyTopology::alloc_queue(QueueLogger *queueLogger, uint64_t speed, mem_b queuesize, bool tor) {
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

void SlimflyTopology::create_switch_switch_link(uint32_t k, uint32_t j, QueueLoggerSampling *queueLogger, simtime_picosec hop_latency){
    // Downlink
    queueLogger = new QueueLoggerSampling(timeFromMs(1000), *_eventlist);
    //logfile->addLogger(*queueLogger);

    queues_switch_switch[k][j] = alloc_queue(queueLogger, _queuesize);
    queues_switch_switch[k][j]->setName("SW" + ntoa(k) + "-I->SW" + ntoa(j));
    //logfile->writeName(*(queues_switch_switch[k][j]));

    pipes_switch_switch[k][j] = new Pipe(_hop_latency, *_eventlist);
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

    pipes_switch_switch[j][k] = new Pipe(_hop_latency, *_eventlist);
    pipes_switch_switch[j][k]->setName("Pipe-SW" + ntoa(j) + "-I->SW" + ntoa(k));
    //logfile->writeName(*(pipes_switch_switch[j][k]));
}

// Connects all nodes to a slimfly network.
void SlimflyTopology::init_network() {
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

    // Creates all switches.
    for (uint32_t j = 0; j < _no_of_switches; j++) {
        switches[j] = new SlimflySwitch(*_eventlist, "Switch_" + ntoa(j), SlimflySwitch::GENERAL, j, timeFromUs((uint32_t)0), this, _sf_routing_strategy);
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

            pipes_switch_host[j][k] = new Pipe(_hop_latency, *_eventlist);
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

            pipes_host_switch[k][j] = new Pipe(_hop_latency, *_eventlist);
            pipes_host_switch[k][j]->setName("Pipe-SRC" + ntoa(k) + "->SW" + ntoa(j));
            //logfile->writeName(*(pipes_host_switch[k][j]));
        }
    }

    uint32_t q2 = pow(_q, 2);

    // Creates all links between Switches and Switches.
    // Create all local links.
    for (int y = 0; y < ((int) _q - 1); y++) {
        for (int yp = (y + 1); yp < (int) _q; yp++) {
            int diff = yp - y;
            // int q_diff = _q - diff;            
            if (is_element(_X, diff)){
                // printf("_X:\ty = %d;\typ = %d\n", y, yp);
                for (uint32_t x = 0; x < _q; x++) {
                    uint32_t k = _q * x + (uint32_t) y;
                    uint32_t j = _q * x + (uint32_t) yp;

                    create_switch_switch_link(k, j, queueLogger, _short_hop_latency);
                }
            }
            if (is_element(_Xp, diff)){
                for (uint32_t x = 0; x < _q; x++) {
                    uint32_t k = q2 + _q * x + y;
                    uint32_t j = q2 + _q * x + yp;

                    create_switch_switch_link(k, j, queueLogger, _short_hop_latency);
                }
            }
        }
    }

    // Create all global links.
    for (uint32_t x = 0; x < _q; x++) {
        for (uint32_t y = 0; y < _q; y++) {
            for (uint32_t m = 0; m < _q; m++) {
                for (uint32_t c = 0; c < _q; c++){
                    if(y == (m * x + c) % _q){
                        uint32_t k = _q * x + y;
                        uint32_t j = q2 + _q * m + c;

                        create_switch_switch_link(k, j, queueLogger, _hop_latency);
                    }
                }
            }
        }
    }

    // Initialize thresholds for lossless operations.
    if (_qt == LOSSLESS) {
        for (uint32_t j = 0; j < _no_of_switches; j++) {
            switches[j]->configureLossless();
        }
    }

    // Prints all connections in the network.
    /* for (uint32_t i = 0; i < _no_of_switches; i++){
        for (uint32_t j = i+1; j < _no_of_switches; j++){
            if(!pipes_switch_switch[i][j] == NULL){
                printf("%d <-> %d\n", i, j);
            }
        }
        printf("\n");
    } */
}