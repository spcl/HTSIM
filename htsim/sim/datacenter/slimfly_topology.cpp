// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "slimfly_topology.h"
#include <stdexcept>
#include <vector>
#include "compositequeue.h"
#include "connection_matrix.h"
#include "main.h"
#include "slimfly_switch.h"
#include "string.h"
#include "switch.h"

// Use tokenize from connection matrix
extern void tokenize(std::string const& str, const char delim, std::vector<std::string>& out);

// Default: ECN disabled
bool SlimFlyTopology::_enable_ecn = false;
mem_b SlimFlyTopology::_ecn_low = 0;
mem_b SlimFlyTopology::_ecn_high = 0;

// Default: all links 200 Gbps
linkspeed_bps SlimFlyTopology::_link_speed_global = 200ULL * 1000000000ULL;
linkspeed_bps SlimFlyTopology::_link_speed_local = 200ULL * 1000000000ULL;
linkspeed_bps SlimFlyTopology::_link_speed_host = 200ULL * 1000000000ULL;

// Default: global links 1000 ns, local links 500 ns, host links 200 ns
simtime_picosec SlimFlyTopology::_link_latency_global = 1000 * 1000;
simtime_picosec SlimFlyTopology::_link_latency_local = 500 * 1000;
simtime_picosec SlimFlyTopology::_link_latency_host = 200 * 1000;

// Default: all switches 0 ns
simtime_picosec SlimFlyTopology::_switch_latency = 0 * 1000;

// Default: no link failures
double SlimFlyTopology::_fail_percentage = 0.0;
int SlimFlyTopology::_fail_packet_rate = 0;

std::string ntoa(double n);
std::string itoa(uint64_t n);

SlimFlyTopology::SlimFlyTopology(std::string base_path,
                                 queue_type queue_type,
                                 mem_b queue_size,
                                 QueueLoggerFactory* logger_factory,
                                 EventList* event_list) {
    _base_path = base_path;
    _queue_type = queue_type;
    _queue_size = queue_size;
    _logger_factory = logger_factory;
    _event_list = event_list;

    _p = 0;
    _q = 0;
}

void SlimFlyTopology::construct_topology(uint32_t p, uint32_t q) {
    assert((q - 1) % 4 == 0 || (q + 1) % 4 == 0 || q % 4 == 0);

    _p = p;
    _q = q;

    _no_switches = 2 * _q * _q;
    _no_hosts = _p * _no_switches;

    init_link_latencies();
    init_pipes_queues();
    init_network();
}

void SlimFlyTopology::load_topology(std::string path) {
    ifstream file(path + "/slimfly.topo");
    if (!file.is_open()) {
        cerr << "ERROR: Could not open: " << path << endl;
        exit(-1);
    }
    uint32_t count = 0;
    std::string line;
    while (count < 10 && std::getline(file, line)) {
        std::vector<std::string> tokens;
        tokenize(line, ' ', tokens);
        if (tokens.size() == 0 || tokens[0][0] == '#')
            continue;

        count++;

        if (tokens[0] == "p")
            _p = stoi(tokens[1]);
        else if (tokens[0] == "q")
            _q = stoi(tokens[1]);
        else if (tokens[0] == "Switch_latency_ns")
            _switch_latency = timeFromNs(stoi(tokens[1]));
        else if (tokens[0] == "Link_speed_global_Gbps")
            _link_speed_global = speedFromGbps(stoi(tokens[1]));
        else if (tokens[0] == "Link_speed_local_Gbps")
            _link_speed_local = speedFromGbps(stoi(tokens[1]));
        else if (tokens[0] == "Link_speed_host_Gbps")
            _link_speed_host = speedFromGbps(stoi(tokens[1]));
        else if (tokens[0] == "Link_latency_global_ns")
            _link_latency_global = timeFromNs(stoi(tokens[1]));
        else if (tokens[0] == "Link_latency_local_ns")
            _link_latency_local = timeFromNs(stoi(tokens[1]));
        else if (tokens[0] == "Link_latency_host_ns")
            _link_latency_host = timeFromNs(stoi(tokens[1]));
        else
            throw std::logic_error("Unexpected case");
    }

    assert((_q - 1) % 4 == 0 || (_q + 1) % 4 == 0 || _q % 4 == 0);
    _no_switches = 2 * _q * _q;
    _no_hosts = _p * _no_switches;
    init_link_latencies();

    while (std::getline(file, line)) {
        std::vector<std::string> tokens;
        tokenize(line, ' ', tokens);
        if (tokens.size() == 0 || tokens[0][0] == '#')
            continue;

        if (tokens[0] == "Link_latency_ns") {
            uint32_t i = stoi(tokens[1]);
            uint32_t j = stoi(tokens[2]);
            simtime_picosec latency = timeFromNs(stoi(tokens[3]));
            set_link_latency(i, j, latency);
        } else
            throw std::logic_error("Unexpected case");
    }

    init_pipes_queues();
    init_network();
}

void SlimFlyTopology::init_link_latencies() {
    _link_latencies.resize(_no_switches, std::unordered_map<uint32_t, simtime_picosec>());
}

void SlimFlyTopology::init_pipes_queues() {
    switches.resize(_no_switches, NULL);

    queues_host_switch.resize(_no_hosts, std::vector<Queue*>(_no_switches));
    queues_switch_switch.resize(_no_switches, std::vector<Queue*>(_no_switches));
    queues_switch_host.resize(_no_switches, std::vector<Queue*>(_no_hosts));

    pipes_host_switch.resize(_no_hosts, std::vector<Pipe*>(_no_switches));
    pipes_switch_switch.resize(_no_switches, std::vector<Pipe*>(_no_switches));
    pipes_switch_host.resize(_no_switches, std::vector<Pipe*>(_no_hosts));

    // Initializes all queues and pipes to NULL
    for (uint32_t i = 0; i < _no_switches; i++) {
        for (uint32_t j = 0; j < _no_switches; j++) {
            queues_switch_switch[i][j] = NULL;
            pipes_switch_switch[i][j] = NULL;
        }

        for (uint32_t j = 0; j < _no_hosts; j++) {
            queues_switch_host[i][j] = NULL;
            queues_host_switch[j][i] = NULL;
            pipes_switch_host[i][j] = NULL;
            pipes_host_switch[j][i] = NULL;
        }
    }
}

void SlimFlyTopology::init_network() {
    QueueLogger* queue_logger;

    ifstream file(_base_path + "/slimfly.adjlist");
    if (!file.is_open()) {
        cerr << "ERROR: Could not open: " << _base_path << endl;
        exit(-1);
    }

    // Create the switches
    for (uint32_t i = 0; i < _no_switches; i++)
        switches[i] = new SlimFlySwitch(_base_path + "/fib", *_event_list, "Switch_" + ntoa(i),
                                        SlimFlySwitch::GENERAL, i, _switch_latency, this);

    // Create all links between Switches and Hosts
    // i = each switch
    // j = each host per switch
    for (uint32_t i = 0; i < _no_switches; i++) {
        for (uint32_t ip = 0; ip < _p; ip++) {
            uint32_t j = i * _p + ip;

            // Switch to Host
            queue_logger = _logger_factory == NULL ? NULL : _logger_factory->createQueueLogger();

            queues_switch_host[i][j] =
                alloc_switch_queue(queue_logger, _link_speed_host, _queue_size);
            queues_switch_host[i][j]->setName("SW" + ntoa(i) + "->DST" + ntoa(j));

            pipes_switch_host[i][j] = new Pipe(_link_latency_host, *_event_list);
            pipes_switch_host[i][j]->setName("Pipe-SW" + ntoa(i) + "->DST" + ntoa(j));

            // Host to Switch
            queue_logger = _logger_factory == NULL ? NULL : _logger_factory->createQueueLogger();

            queues_host_switch[j][i] = alloc_host_queue(queue_logger, _link_speed_host);
            queues_host_switch[j][i]->setName("SRC" + ntoa(j) + "->SW" + ntoa(i));

            pipes_host_switch[j][i] = new Pipe(_link_latency_host, *_event_list);
            pipes_host_switch[j][i]->setName("Pipe-SRC" + ntoa(j) + "->SW" + ntoa(i));

            // Add ports and set remote endpoints
            switches[i]->addPort(queues_switch_host[i][j]);
            switches[i]->addPort(queues_host_switch[j][i]);
            queues_host_switch[j][i]->setRemoteEndpoint(switches[i]);
        }
    }

    int total_queues = 0;

    // Create all links between Switches and Switches
    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> tokens;
        tokenize(line, ' ', tokens);
        if (tokens.size() == 0 || tokens[0][0] == '#')
            continue;

        uint32_t i = std::stoi(tokens[0]);

        for (size_t idx = 1; idx < tokens.size(); idx++) {
            uint32_t j = std::stoi(tokens[idx]);
            simtime_picosec link_latency = 0;
            linkspeed_bps link_speed = 0;
            std::string name = "";

            if (i / _q == j / _q) {
                // Local link
                link_latency = _link_latency_local;
                link_speed = _link_speed_local;
                name = "I";

            } else {
                // Global link
                link_latency = _link_latency_global;
                link_speed = _link_speed_global;
                name = "G";
            }

            // Set link latency - if not already set from .topo file
            std::unordered_map<uint32_t, simtime_picosec>& map = _link_latencies[min(i, j)];
            if (map.find(max(i, j)) == map.end())
                map[max(i, j)] = link_latency;

            // i → j
            queue_logger = _logger_factory == NULL ? NULL : _logger_factory->createQueueLogger();

            queues_switch_switch[i][j] =
                alloc_switch_queue(queue_logger, link_speed, _queue_size);
            queues_switch_switch[i][j]->setName("SW" + ntoa(i) + "-" + name + "->SW" + ntoa(j));
            total_queues++;

            pipes_switch_switch[i][j] = new Pipe(get_link_latency(i, j), *_event_list);
            pipes_switch_switch[i][j]->setName("Pipe-SW" + ntoa(i) + "-" + name + "->SW" + ntoa(j));

            // j → i
            queue_logger = _logger_factory == NULL ? NULL : _logger_factory->createQueueLogger();

            queues_switch_switch[j][i] =
                alloc_switch_queue(queue_logger, link_speed, _queue_size);
            queues_switch_switch[j][i]->setName("SW" + ntoa(j) + "-" + name + "->SW" + ntoa(i));
            total_queues++;

            pipes_switch_switch[j][i] = new Pipe(get_link_latency(i, j), *_event_list);
            pipes_switch_switch[j][i]->setName("Pipe-SW" + ntoa(j) + "-" + name + "->SW" + ntoa(i));

            // Add ports and set remote endpoints
            switches[i]->addPort(queues_switch_switch[i][j]);
            switches[j]->addPort(queues_switch_switch[j][i]);
            queues_switch_switch[i][j]->setRemoteEndpoint(switches[j]);
            queues_switch_switch[j][i]->setRemoteEndpoint(switches[i]);
        }
    }

    // Init neighbours
    for (uint32_t i = 0; i < _no_switches; i++)
        ((SlimFlySwitch*)switches[i])->init_neighbours();

    // Link failures
    int no_failures = total_queues * _fail_percentage;
    std::unordered_set<int> failed_queues;

    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, total_queues - 1);

    while (failed_queues.size() < static_cast<size_t>(no_failures))
        failed_queues.insert(dist(gen));

    int queue_index = 0;
    for (uint32_t i = 0; i < _no_switches; i++) {
        for (uint32_t j = 0; j < _no_switches; j++) {
            if (queues_switch_switch[i][j] == NULL)
                continue;
            bool fail_queue = failed_queues.find(queue_index) != failed_queues.end();    
            if (fail_queue) {
                CompositeQueue* cq = dynamic_cast<CompositeQueue*>(queues_switch_switch[i][j]);
                if (cq)
                    cq->set_fail_rate(_fail_packet_rate);
            }
            queue_index++;
        }
    }
}

std::vector<const Route*>* SlimFlyTopology::get_bidir_paths(uint32_t src,
                                                            uint32_t dest,
                                                            bool reverse) {
    // TODO: implement (if needed)
    throw std::logic_error("Not implemented");
}

std::vector<uint32_t>* SlimFlyTopology::get_neighbours(uint32_t src) {
    std::vector<uint32_t>* neighbours = new std::vector<uint32_t>();
    for (uint32_t dst = 0; dst < _no_switches; dst++) {
        if (queues_switch_switch[src][dst] != NULL)
            neighbours->push_back(dst);
    }
    return neighbours;
}

simtime_picosec SlimFlyTopology::get_max_rtt(SlimFlySwitch::RoutingStrategy routing_strategy,
                                             uint32_t data_packet_size,
                                             uint32_t ack_packet_size) {
    // WARNING: does not support arbitrary specific link latencies
    // Transmission delays
    simtime_picosec t_delay_data_global =
        timeFromNs(((double)data_packet_size * 8) / speedAsGbps(_link_speed_global));
    simtime_picosec t_delay_data_local =
        timeFromNs(((double)data_packet_size * 8) / speedAsGbps(_link_speed_local));
    simtime_picosec t_delay_data_host =
        timeFromNs(((double)data_packet_size * 8) / speedAsGbps(_link_speed_host));

    simtime_picosec t_delay_ack_global =
        timeFromNs(((double)ack_packet_size * 8) / speedAsGbps(_link_speed_global));
    simtime_picosec t_delay_ack_local =
        timeFromNs(((double)ack_packet_size * 8) / speedAsGbps(_link_speed_local));
    simtime_picosec t_delay_ack_host =
        timeFromNs(((double)ack_packet_size * 8) / speedAsGbps(_link_speed_host));

    simtime_picosec data, ack;

    switch (routing_strategy) {
        case SlimFlySwitch::MINIMAL:
        case SlimFlySwitch::SOURCE:
        case SlimFlySwitch::VALIANT:
        case SlimFlySwitch::UGAL_L: {
            // Worst case routing
            // Data: (0,4), Ack: (0,2)
            data = (1 * (t_delay_data_host + _link_latency_host)) +
                   (0 * (_switch_latency + t_delay_data_local + _link_latency_local)) +
                   (4 * (_switch_latency + t_delay_data_global + _link_latency_global)) +
                   (1 * (_switch_latency + t_delay_data_host + _link_latency_host));

            ack = (1 * (t_delay_ack_host + _link_latency_host)) +
                  (0 * (_switch_latency + t_delay_ack_local + _link_latency_local)) +
                  (2 * (_switch_latency + t_delay_ack_global + _link_latency_global)) +
                  (1 * (_switch_latency + t_delay_ack_host + _link_latency_host));

            return data + ack;
        }

        default:
            throw std::logic_error("Unexpected case");
    }
}

linkspeed_bps SlimFlyTopology::get_linkspeed() {
    // WARNING: same linkspeed assumed for now for CC
    assert(_link_speed_global == _link_speed_local);
    assert(_link_speed_local == _link_speed_host);
    return _link_speed_global;
}

std::string SlimFlyTopology::get_link_latencies() {
    return std::to_string(_link_latency_global) + ":" + std::to_string(_link_latency_local) + ":" +
           std::to_string(_link_latency_host);
}

inline void SlimFlyTopology::set_link_latency(uint32_t src_switch,
                                              uint32_t dst_switch,
                                              simtime_picosec latency) {
    _link_latencies[min(src_switch, dst_switch)][max(src_switch, dst_switch)] = latency;
}

inline simtime_picosec SlimFlyTopology::get_link_latency(uint32_t src_switch, uint32_t dst_switch) {
    return _link_latencies[min(src_switch, dst_switch)][max(src_switch, dst_switch)];
}

Queue* SlimFlyTopology::alloc_host_queue(QueueLogger* queue_logger, linkspeed_bps linkspeed) {
    mem_b max_size = memFromPkt(FEEDER_BUFFER);
    return new FairPriorityQueue(linkspeed, max_size, *_event_list, queue_logger);
}

Queue* SlimFlyTopology::alloc_switch_queue(QueueLogger* queue_logger,
                                           linkspeed_bps linkspeed,
                                           mem_b queue_size) {
    switch (_queue_type) {
        case RANDOM: {
            mem_b max_size = _queue_size;
            mem_b drop = memFromPkt(RANDOM_BUFFER);
            return new RandomQueue(linkspeed, max_size, *_event_list, queue_logger, drop);
        }

        case COMPOSITE: {
            mem_b max_size = _queue_size;
            bool trim_disable = SlimFlySwitch::get_trim_disable();
            uint16_t trim_size = SlimFlySwitch::get_trim_size();

            CompositeQueue* q = new CompositeQueue(linkspeed, max_size, *_event_list, queue_logger,
                                                   trim_size, trim_disable);
            if (_enable_ecn)
                q->set_ecn_thresholds(_ecn_low, _ecn_high);

            return q;
        }

        default:
            throw std::logic_error("Not implemented");
    }
}
