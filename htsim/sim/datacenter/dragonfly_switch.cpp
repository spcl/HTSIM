// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "dragonfly_switch.h"
#include <random>
#include <stdexcept>
#include "config.h"
#include "dragonfly_topology.h"

bool DragonflySwitch::_trim_disable = false;
uint16_t DragonflySwitch::_trim_size = 0;
DragonflySwitch::RoutingStrategy DragonflySwitch::_routing_strategy = DragonflySwitch::MINIMAL;

string ntoa(double n);
string itoa(uint64_t n);

DragonflySwitch::DragonflySwitch(EventList& event_list,
                                 string name,
                                 SwitchType type,
                                 uint32_t id,
                                 simtime_picosec delay,
                                 DragonflyTopology* topo)
    : Switch(event_list, name) {
    _id = id;
    _fib = new RouteTable();

    _type = type;
    _topo = topo;
    _a = topo->get_a();
    _h = topo->get_h();
    _no_groups = topo->get_no_groups();

    _pipe = new CallbackPipe(delay, event_list, this);

    _generator = std::mt19937(33);
    _dist_a = std::uniform_int_distribution<>(0, _a - 1);
    _dist_h = std::uniform_int_distribution<>(0, _h - 1);

    for (uint32_t n = (_id / _a) * _a; n < ((_id / _a) * _a) + _a; n++)
        _neighbours.insert(n);
    for (uint32_t global_link = 0; global_link < _h; global_link++)
        _neighbours.insert(topo->get_target_switch(_id, global_link));
}

void DragonflySwitch::receivePacket(Packet& pkt) {
    if (_packets.find(&pkt) == _packets.end()) {
        _packets[&pkt] = true;
        const Route* next_hop = getNextHop(pkt, NULL);
        pkt.set_route(*next_hop);
        _pipe->receivePacket(pkt);
    } else {
        _packets.erase(&pkt);
        pkt.sendOn();
    }
}

void DragonflySwitch::addHostPort(int addr, int flowid, PacketSink* transport) {
    uint32_t i = _topo->get_host_switch(addr);
    uint32_t j = addr;
    Route* route = new Route();
    route->push_back(_topo->queues_switch_host[i][j]);
    route->push_back(_topo->pipes_switch_host[i][j]);
    route->push_back(transport);
    _fib->addHostRoute(addr, route, flowid);
}

void DragonflySwitch::permute_paths(vector<FibEntry*>* routes) {
    throw std::logic_error("Not implemented");
}

uint32_t DragonflySwitch::get_next_switch_minimal(uint32_t this_switch, uint32_t dst_switch) {
    uint32_t this_group = this_switch / _a;
    uint32_t dst_group = dst_switch / _a;

    // Next: switch within the same group as this switch
    if (this_group == dst_group)
        return dst_switch;

    // Next: different group - 2 options

    // Get switch within this group that connects to the dst group
    uint32_t group_switch = _topo->get_group_switch(this_group, dst_group);

    // Option 1: route within group to the switch that connects to the dst group
    if (this_switch != group_switch)
        return group_switch;

    // Option 2: this switch directly connects to the dst group
    uint32_t dst_group_switch = _topo->get_group_switch(dst_group, this_group);
    return dst_group_switch;
}

uint32_t DragonflySwitch::get_next_switch_valiant(uint32_t this_switch,
                                                  uint32_t src_switch,
                                                  uint32_t dst_switch) {
    uint32_t this_group = this_switch / _a;
    uint32_t src_group = src_switch / _a;
    uint32_t dst_group = dst_switch / _a;

    uint32_t next_switch;

    // Next: route to intermediate group G_i
    if (this_group == src_group && src_group != dst_group) {
        // Select random switch within this group [0, a)
        if (this_switch == src_switch) {
            next_switch = (this_group * _a) + _dist_a(_generator);
            if (this_switch == next_switch)
                next_switch = _topo->get_target_switch(this_switch, _dist_h(_generator));
            return next_switch;
        }

        // Select random global link [0, h) from this switch
        next_switch = _topo->get_target_switch(this_switch, _dist_h(_generator));
        return next_switch;
    }

    // Next: route to intermediate switch within G_s (G_i = G_s = G_d) => RVAL improvement
    if (this_group == src_group && src_group == dst_group) {
        if (this_switch == src_switch) {
            do {
                next_switch = (this_group * _a) + _dist_a(_generator);
            } while (next_switch == this_switch || next_switch == dst_switch);

            if (_a == 2)
                next_switch = (this_switch + 1) % _a;
            return next_switch;
        }
        return dst_switch;
    }

    // Next: switch within the same group as this switch
    if (this_group == dst_group)
        return dst_switch;

    // Get switch within this group that connects to the dst group
    uint32_t group_switch = _topo->get_group_switch(this_group, dst_group);

    // Option 1: route within group to the switch that connects to the dst group
    if (this_switch != group_switch)
        return group_switch;

    // Option 2: this switch directly connects to the dst group
    uint32_t dst_group_switch = _topo->get_group_switch(dst_group, this_group);
    return dst_group_switch;
}

uint32_t DragonflySwitch::get_next_switch_source(uint32_t this_switch,
                                                 uint32_t src_switch,
                                                 uint32_t dst_switch,
                                                 uint32_t hop_one_switch,
                                                 uint32_t hop_two_switch) {
    uint32_t this_group = this_switch / _a;
    uint32_t src_group = src_switch / _a;
    uint32_t dst_group = dst_switch / _a;

    // Next: route to intermediate group G_i
    if (this_group == src_group && src_group != dst_group) {
        // Hop 1 choice (with or outside G_s)
        if (this_switch == src_switch) {
            if (_neighbours.find(hop_one_switch) == _neighbours.end())
                throw std::logic_error("Incorrect 'path_id' - hop_one_switch");
            return hop_one_switch;
        }
        // Hop 2 choice
        if (_neighbours.find(hop_two_switch) == _neighbours.end())
            throw std::logic_error("Incorrect 'path_id' - hop_two_switch");
        return hop_two_switch;
    }

    // Next: route to intermediate switch within G_s (G_i = G_s = G_d) => RVAL improvement
    if (this_group == src_group && src_group == dst_group) {
        // Hop 1 choice (intermediate hop within src_group or direct dst_switch)
        if (this_switch == src_switch) {
            if (_neighbours.find(hop_one_switch) == _neighbours.end())
                throw std::logic_error("Incorrect 'path_id' - hop_one_switch");
            return hop_one_switch;
        }
        return dst_switch;
    }

    // Next: switch within the same group as this switch
    if (this_group == dst_group)
        return dst_switch;

    // Get switch within this group that connects to the dst group
    uint32_t group_switch = _topo->get_group_switch(this_group, dst_group);

    // Option 1: route within group to the switch that connects to the dst group
    if (this_switch != group_switch)
        return group_switch;

    // Option 2: this switch directly connects to the dst group
    uint32_t dst_group_switch = _topo->get_group_switch(dst_group, this_group);
    return dst_group_switch;
}

FibEntry* DragonflySwitch::get_fib_entry(uint32_t next_switch) {
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

Route* DragonflySwitch::getNextHop(Packet& pkt, BaseQueue* ingress_port) {
    uint32_t this_switch = _id;
    uint32_t src_switch = _topo->get_host_switch(pkt.src());
    uint32_t dst_switch = _topo->get_host_switch(pkt.dst());

    pkt.set_direction(DOWN);

    // Next: host directly connected to this switch
    if (this_switch == dst_switch) {
        HostFibEntry* host_entry = _fib->getHostRoute(pkt.dst(), pkt.flow_id());
        assert(host_entry);
        return host_entry->getEgressPort();
    }

    // ACK / NACK routed through minimal routes using priority queues
    if (pkt.type() == UECACK || pkt.type() == UECNACK) {
        uint32_t next_switch = get_next_switch_minimal(this_switch, dst_switch);
        return get_fib_entry(next_switch)->getEgressPort();
    }

    switch (_routing_strategy) {
        case MINIMAL: {
            uint32_t next_switch = get_next_switch_minimal(this_switch, dst_switch);
            return get_fib_entry(next_switch)->getEgressPort();
        }

        case VALIANT: {
            uint32_t next_switch = get_next_switch_valiant(this_switch, src_switch, dst_switch);
            return get_fib_entry(next_switch)->getEgressPort();
        }

        case UGAL_L: {
            uint32_t next_switch_min = get_next_switch_minimal(this_switch, dst_switch);
            uint32_t next_switch_val = get_next_switch_valiant(this_switch, src_switch, dst_switch);

            if (next_switch_min == next_switch_val)
                return get_fib_entry(next_switch_min)->getEgressPort();

            Queue* q_min = _topo->queues_switch_switch[this_switch][next_switch_min];
            Queue* q_val = _topo->queues_switch_switch[this_switch][next_switch_val];

            // Calculate path hop-count
            uint32_t tmp_switch;
            uint32_t h_min = 1;
            uint32_t h_val = 1;

            tmp_switch = next_switch_min;
            while (tmp_switch != dst_switch) {
                tmp_switch = get_next_switch_minimal(tmp_switch, dst_switch);
                h_min++;
            }

            tmp_switch = next_switch_val;
            while (tmp_switch != dst_switch) {
                tmp_switch = get_next_switch_valiant(tmp_switch, src_switch, dst_switch);
                h_val++;
            }

            if (q_min->queuesize() * h_min > q_val->queuesize() * h_val)
                return get_fib_entry(next_switch_val)->getEgressPort();

            return get_fib_entry(next_switch_min)->getEgressPort();
        }

        case SOURCE: {
            uint32_t hop_one_switch = (pkt.pathid() >> 16) & 0xFFFF;  // Upper 16 bits
            uint32_t hop_two_switch = pkt.pathid() & 0xFFFF;          // Lower 16 bits
            uint32_t next_switch = get_next_switch_source(this_switch, src_switch, dst_switch,
                                                          hop_one_switch, hop_two_switch);
            return get_fib_entry(next_switch)->getEgressPort();
        }

        default:
            throw std::logic_error("Unexpected case");
    }

    throw std::logic_error("Unexpected case");
}
