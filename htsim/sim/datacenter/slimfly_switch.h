// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef _SLIMFLY_SWITCH_H
#define _SLIMFLY_SWITCH_H

#include <random>
#include <unordered_map>
#include <unordered_set>
#include "callback_pipe.h"
#include "switch.h"

class SlimFlyTopology;

class SlimFlySwitch : public Switch {
public:
    enum SwitchType { NONE = 0, GENERAL = 1 };
    enum RoutingStrategy { MINIMAL = 0, VALIANT = 1, UGAL_L = 2, SOURCE = 3 };

    SlimFlySwitch(std::string path_fib,
                  EventList& event_list,
                  std::string name,
                  SwitchType type,
                  uint32_t id,
                  simtime_picosec delay,
                  SlimFlyTopology* topo);

    virtual void receivePacket(Packet& pkt);
    virtual Route* getNextHop(Packet& pkt, BaseQueue* ingress_port);
    virtual void addHostPort(int addr, int flowid, PacketSink* transport_port);
    virtual void permute_paths(vector<FibEntry*>* uproutes);
    virtual uint32_t getType() { return _type; }

    void init_neighbours();
    uint32_t get_next_switch_minimal(uint32_t dst_switch);
    uint32_t get_next_switch_valiant(uint32_t dst_switch, uint32_t hop_count);

    static void set_config(RoutingStrategy routing_strategy,
                           bool trim_disable,
                           uint16_t trim_size) {
        _routing_strategy = routing_strategy;
        _trim_disable = trim_disable;
        _trim_size = trim_size;
    }
    static uint16_t get_trim_disable() { return _trim_disable; }
    static uint16_t get_trim_size() { return _trim_size; }
    // -1 keeps the legacy behaviour (every switch seeded with 33).
    static void set_rng_seed(int64_t seed) { _rng_seed = seed; }

private:
    std::string _path_fib;
    SwitchType _type;
    SlimFlyTopology* _topo;
    uint32_t _q;

    Pipe* _pipe;
    std::vector<std::pair<simtime_picosec, uint64_t>> _list_sent;
    std::unordered_map<Packet*, bool> _packets;

    mt19937 _generator;
    uniform_int_distribution<> _dist_neighbour;

    std::vector<uint32_t>* _neighbours;
    std::unordered_map<uint32_t, uint32_t> _fib_min_paths;
    std::unordered_map<uint32_t, FibEntry*> _fib_entries;

    void init_fib_min_paths();

    uint32_t get_next_switch_source(uint32_t dst_switch,
                                    uint32_t hop_count,
                                    uint32_t hop_one_switch,
                                    uint32_t hop_two_switch);
    FibEntry* get_fib_entry(uint32_t next_switch);

    static bool _trim_disable;
    static uint16_t _trim_size;
    static RoutingStrategy _routing_strategy;
    static int64_t _rng_seed;
};

#endif