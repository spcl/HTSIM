// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "slimfly_switch.h"
#include <random>
#include <stdexcept>
#include "slimfly_topology.h"

// Use tokenize from connection matrix
extern void tokenize(std::string const& str, const char delim, std::vector<std::string>& out);

bool SlimFlySwitch::_trim_disable = false;
uint16_t SlimFlySwitch::_trim_size = 0;
SlimFlySwitch::RoutingStrategy SlimFlySwitch::_routing_strategy = SlimFlySwitch::MINIMAL;

string ntoa(double n);
string itoa(uint64_t n);

SlimFlySwitch::SlimFlySwitch(std::string path_fib,
                             EventList& event_list,
                             string name,
                             SwitchType type,
                             uint32_t id,
                             simtime_picosec delay,
                             SlimFlyTopology* topo)
    : Switch(event_list, name) {
    _id = id;
    _fib = new RouteTable();

    _path_fib = path_fib;
    _type = type;
    _topo = topo;
    _q = topo->get_q();

    _pipe = new CallbackPipe(delay, event_list, this);

    init_fib_min_paths();
}

void SlimFlySwitch::init_neighbours() {
    _neighbours = _topo->get_neighbours(_id);
    _generator = mt19937(33);
    _dist_neighbour = uniform_int_distribution<>(0, _neighbours->size() - 1);
}

void SlimFlySwitch::init_fib_min_paths() {
    ifstream file(_path_fib + "/" + std::to_string(_id) + ".fib");
    if (!file.is_open())
        throw std::invalid_argument("ERROR: Could not open: " + _path_fib);

    uint32_t dst_switch, next_hop;
    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> tokens;
        tokenize(line, ' ', tokens);
        if (tokens.size() == 0 || tokens[0][0] == '#')
            continue;
        if (tokens.size() != 2)
            throw std::logic_error("Invalid FIB file content format: dst_switch next_hop");
        dst_switch = std::stoi(tokens[0]);
        next_hop = std::stoi(tokens[1]);
        _fib_min_paths[dst_switch] = next_hop;
    }
}

void SlimFlySwitch::receivePacket(Packet& pkt) {
    if (_packets.find(&pkt) == _packets.end()) {
        _packets[&pkt] = true;
        const Route* next_hop = getNextHop(pkt, NULL);
        pkt.set_route(*next_hop);
        _pipe->receivePacket(pkt);
    } else {
        _packets.erase(&pkt);
        // TODO: check understanding (missing code from HTSIM - Packet signature changed)
        pkt.sendOn();
    }
}

void SlimFlySwitch::addHostPort(int addr, int flowid, PacketSink* transport) {
    uint32_t i = _topo->get_host_switch(addr);
    uint32_t j = addr;
    Route* route = new Route();
    route->push_back(_topo->queues_switch_host[i][j]);
    route->push_back(_topo->pipes_switch_host[i][j]);
    route->push_back(transport);
    _fib->addHostRoute(addr, route, flowid);
}

void SlimFlySwitch::permute_paths(vector<FibEntry*>* routes) {
    throw std::logic_error("Not implemented");
}

uint32_t SlimFlySwitch::get_next_switch_minimal(uint32_t dst_switch) {
    return _fib_min_paths[dst_switch];
}

uint32_t SlimFlySwitch::get_next_switch_valiant(uint32_t dst_switch, uint32_t hop_count) {
    if (hop_count > 2)
        return get_next_switch_minimal(dst_switch);
    return _neighbours->at(_dist_neighbour(_generator));
}

uint32_t SlimFlySwitch::get_next_switch_source(uint32_t dst_switch,
                                               uint32_t hop_count,
                                               uint32_t hop_one_switch,
                                               uint32_t hop_two_switch) {
    if (hop_count == 1)
        return hop_one_switch;
    else if (hop_count == 2)
        return hop_two_switch;
    else
        return get_next_switch_minimal(dst_switch);
}

FibEntry* SlimFlySwitch::get_fib_entry(uint32_t next_switch) {
    auto it = _fib_entries.find(next_switch);
    if (it != _fib_entries.end())
        return it->second;

    Route* route = new Route();
    route->push_back(_topo->queues_switch_switch[_id][next_switch]);
    route->push_back(_topo->pipes_switch_switch[_id][next_switch]);
    route->push_back(_topo->queues_switch_switch[_id][next_switch]->getRemoteEndpoint());
    FibEntry* fib_entry = new FibEntry(route, 1, DOWN);
    _fib_entries[next_switch] = fib_entry;

    return fib_entry;
}

Route* SlimFlySwitch::getNextHop(Packet& pkt, BaseQueue* ingress_port) {
    uint32_t this_switch = _id;
    uint32_t dst_switch = _topo->get_host_switch(pkt.dst());

    pkt.set_direction(DOWN);
    pkt.increment_hop_count();

    // Next: host directly connected to this switch
    if (this_switch == dst_switch) {
        HostFibEntry* host_entry = _fib->getHostRoute(pkt.dst(), pkt.flow_id());
        assert(host_entry);
        return host_entry->getEgressPort();
    }

    // ACK / NACK routed through minimal routes using priority queues
    if (pkt.type() == UECACK || pkt.type() == UECNACK) {
        uint32_t next_switch = get_next_switch_minimal(dst_switch);
        return get_fib_entry(next_switch)->getEgressPort();
    }

    switch (_routing_strategy) {
        case MINIMAL: {
            uint32_t next_switch = get_next_switch_minimal(dst_switch);
            return get_fib_entry(next_switch)->getEgressPort();
        }

        case VALIANT: {
            uint32_t next_switch = get_next_switch_valiant(dst_switch, pkt.hop_count());
            return get_fib_entry(next_switch)->getEgressPort();
        }

        case UGAL_L: {
            uint32_t next_switch_min = get_next_switch_minimal(dst_switch);
            uint32_t next_switch_val = get_next_switch_valiant(dst_switch, pkt.hop_count());

            if (next_switch_min == next_switch_val)
                return get_fib_entry(next_switch_min)->getEgressPort();

            Queue* q_min = _topo->queues_switch_switch[this_switch][next_switch_min];
            Queue* q_val = _topo->queues_switch_switch[this_switch][next_switch_val];

            // Calculate path hop-count
            SlimFlySwitch* tmp_switch;
            uint32_t h_min = 1;
            uint32_t h_val = 1;

            tmp_switch = (SlimFlySwitch*)_topo->switches.at(next_switch_min);
            while (tmp_switch->getID() != dst_switch) {
                tmp_switch = (SlimFlySwitch*)_topo->switches.at(
                    tmp_switch->get_next_switch_minimal(dst_switch));
                h_min++;
            }

            tmp_switch = (SlimFlySwitch*)_topo->switches.at(next_switch_val);
            while (tmp_switch->getID() != dst_switch) {
                tmp_switch = (SlimFlySwitch*)_topo->switches.at(
                    tmp_switch->get_next_switch_valiant(dst_switch, pkt.hop_count() + h_val));
                h_val++;
            }

            if (q_min->queuesize() * h_min > q_val->queuesize() * h_val)
                return get_fib_entry(next_switch_val)->getEgressPort();

            return get_fib_entry(next_switch_min)->getEgressPort();
        }

        case SOURCE: {
            uint32_t hop_one_switch = (pkt.pathid() >> 16) & 0xFFFF;  // Upper 16 bits
            uint32_t hop_two_switch = pkt.pathid() & 0xFFFF;          // Lower 16 bits
            uint32_t next_switch =
                get_next_switch_source(dst_switch, pkt.hop_count(), hop_one_switch, hop_two_switch);
            return get_fib_entry(next_switch)->getEgressPort();
        }

        default:
            throw std::logic_error("Unexpected case");
    }

    throw std::logic_error("Unexpected case");
}
