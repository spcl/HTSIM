// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef FAILURE_GENERATOR_H
#define FAILURE_GENERATOR_H

#include "network.h"
#include "pipe.h"
#include "switch.h"
#include "uec.h"
#include <chrono>
#include <set>
#include <unordered_map>

class failuregenerator {

  public:
    simtime_picosec stop_failures_after = UINT64_MAX;
    uint64_t nr_total_packets = 0;
    uint64_t nr_dropped_packets = 0;
    uint32_t path_nr = 0;
    std::unordered_map<UecSink *, vector<uint64_t>> map_list_packet_seq;
    std::set<UecSrc *> all_srcs;
    std::set<UecSink *> all_sinks;
    // map from path to switches in this path
    std::unordered_map<uint32_t, std::set<uint64_t>> path_switches;
    // map from path to cables in this path
    std::unordered_map<uint32_t, std::set<uint64_t>> path_cables;
    static void setUseTimeouts(bool use_timeouts) { _use_timeouts = use_timeouts; }
    static bool _use_timeouts;

    void addSrc(UecSrc *src) { all_srcs.insert(src); }
    void addDst(UecSink *sink) { all_sinks.insert(sink); }

    std::pair<std::pair<std::set<uint32_t>, std::set<uint32_t>>, std::string> get_path_switches_cables(uint32_t path_id,
                                                                                                       UecSrc *src);

    bool check_connectivity();
    void parseinputfile();
    string failures_input_file_path;
    void setInputFile(string file_path) {
        failures_input_file_path = file_path;
        parseinputfile();
    };

    bool simSwitchFailures(Packet &pkt, Switch *sw, Queue q);
    bool simCableFailures(Pipe *p, Packet &pkt);
    bool simNICFailures(UecSrc *src, UecSink *sink, Packet &pkt);

    // Random Packet drop
    bool randomPacketDrop = false;
    int numberOfRandomDroppedPackets = 0;
    int numberOfRandomDroppedPacketsTest = 0;
    void addRandomPacketDrop(Packet &pkt, UecSrc *src);
    void dropRandomPacket(Packet &pkt);
    void dropRandomPacketSink(Packet &pkt);
    std::unordered_map<uint32_t, UecSrc *> randomDroppedPackets;
    uint32_t dropRate = 0;

    // Switch
    bool fail_new_switch(Switch *sw);
    std::set<uint32_t> all_switches;
    bool switchFail(Switch *sw);
    bool switch_fail = false;
    bool pause_fail_switch = false;
    bool switch_fail_duration = false;
    int switch_fail_duration_time = 0;
    std::unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> failingSwitches;
    std::unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> temp_failingSwitches;
    std::set<uint32_t> neededSwitches;
    simtime_picosec switch_fail_start = 0;
    simtime_picosec switch_fail_period = 0;
    simtime_picosec switch_fail_next_fail = 0;
    float switch_fail_max_percent = 1;

    bool switchBER(Packet &pkt, Switch *sw, Queue q);
    bool dropPacketsSwitchBER(Packet &pkt);
    int num_corrupted_packets_switchBER = 0;
    int all_packets_switchBER = 0;
    std::set<uint32_t> corrupted_packets;
    bool switch_ber = false;
    simtime_picosec switch_ber_start = 0;
    simtime_picosec switch_ber_period = 0;
    simtime_picosec switch_ber_next_fail = 0;
    float switch_ber_max_percent = 1;

    bool switchDegradation(Switch *sw);
    std::set<uint32_t> degraded_switches;
    std::set<uint32_t> temp_degraded_switches;
    std::set<uint32_t> needed_degraded_switches;
    bool switch_degradation = false;
    simtime_picosec switch_degradation_start = 0;
    simtime_picosec switch_degradation_period = 0;
    simtime_picosec switch_degradation_next_fail = 0;
    float switch_degradation_max_percent = 1;

    bool switchPeriodicDrop(Switch *sw);
    bool fail_new_periodic_switch(Switch *sw);
    std::unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> periodicfailingSwitches;
    std::unordered_map<uint32_t, uint32_t> SwitchToNrDrops;
    std::unordered_map<uint32_t, uint32_t> SwitchToNextDropTime;

    std::unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> temp_periodicfailingSwitches;
    bool switch_periodic_loss = false;
    bool pause_switch_periodic_loss = false;
    simtime_picosec switch_periodic_loss_start = 0;
    simtime_picosec switch_periodic_loss_period = 0;
    simtime_picosec switch_periodic_loss_next_fail = 0;
    float switch_periodic_loss_max_percent = 1;
    uint32_t switch_periodic_loss_pkt_amount = 0;
    uint32_t switch_periodic_loss_drop_period = 0;

    // Cable
    bool fail_new_cable(Pipe *p);

    std::set<uint32_t> all_cables;

    bool cableFail(Pipe *p, Packet &pkt);
    unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> failingCables;
    unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> temp_failingCables;
    bool cable_fail = false;
    bool only_us_cs = false;
    bool pause_fail_cable = false;
    bool cable_fail_per_switch = false;
    bool cable_fail_duration = false;
    int cable_fail_duration_time = 0;
    // Map from switch to cables going away fromt this switch
    unordered_map<uint32_t, std::set<uint32_t>> CablesPerSwitch;
    void addCablePerSwitch(uint32_t switch_id, uint32_t cable_id);
    std::set<uint32_t> neededCables;
    simtime_picosec cable_fail_start = 0;
    simtime_picosec cable_fail_period = 0;
    simtime_picosec cable_fail_next_fail = 0;
    float cable_fail_max_percent = 1;

    bool cableBER(Packet &pkt);
    bool cable_ber;
    int all_packets_cableBER = 0;
    int num_corrupted_packets_cableBER = 0;
    simtime_picosec cable_ber_start = 0;
    simtime_picosec cable_ber_period = 0;
    simtime_picosec cable_ber_next_fail = 0;
    float cable_ber_max_percent = 1;

    bool cableDegradation(Pipe *p, Packet &pkt);
    bool cable_degradation = false;
    std::unordered_map<uint32_t, uint32_t> degraded_cables;
    std::unordered_map<uint32_t, uint32_t> temp_degraded_cables;
    std::set<uint32_t> needed_degraded_cables;
    simtime_picosec cable_degradation_start = 0;
    simtime_picosec cable_degradation_period = 0;
    simtime_picosec cable_degradation_next_fail = 0;
    float cable_degradation_max_percent = 1;

    bool cablePeriodicDrop(Pipe *p, Packet &pkt);
    bool fail_new_periodic_cable(Pipe *p);
    unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> periodicFailingCables;
    unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> temp_periodicFailingCables;
    std::unordered_map<uint32_t, uint32_t> CableToNrDrops;
    std::unordered_map<uint32_t, simtime_picosec> CableToNextDropTime;
    bool pause_cable_periodic_loss = false;
    bool cable_periodic_loss = false;
    simtime_picosec cable_periodic_loss_start = 0;
    simtime_picosec cable_periodic_loss_period = 0;
    simtime_picosec cable_periodic_loss_next_fail = 0;
    float cable_periodic_loss_max_percent = 1;
    uint32_t cable_periodic_loss_pkt_amount = 0;
    uint32_t cable_periodic_loss_drop_period = 0;

    // NIC
    bool nic_fail = false;

    bool fail_new_nic(u_int32_t nic_id);

    bool nicFail(UecSrc *src, UecSink *sink, Packet &pkt);
    std::unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> failingNICs;
    simtime_picosec nic_fail_start = 0;
    simtime_picosec nic_fail_period = 0;
    simtime_picosec nic_fail_next_fail = 0;
    float nic_fail_max_percent = 1;

    bool nic_degradation = false;
    bool nicDegradation(UecSrc *src, UecSink *sink, Packet &pkt);
    std::set<uint32_t> degraded_NICs;
    simtime_picosec nic_degradation_start = 0;
    simtime_picosec nic_degradation_period = 0;
    simtime_picosec nic_degradation_next_fail = 0;
    float nic_degradation_max_percent = 1;

    // For logging
    void createLoggingData();
    vector<simtime_picosec> _list_random_packet_drops;
    vector<simtime_picosec> _list_switch_packet_drops;
    vector<simtime_picosec> _list_cable_packet_drops;
    vector<pair<simtime_picosec, simtime_picosec>> _list_switch_failures;
    vector<simtime_picosec> _list_switch_degradations;
    vector<simtime_picosec> _list_cable_degradations;
    vector<pair<simtime_picosec, simtime_picosec>> _list_cable_failures;
    vector<simtime_picosec> _list_routed_failing_switches;
    vector<simtime_picosec> _list_routed_failing_cables;

  private:
};

#endif
