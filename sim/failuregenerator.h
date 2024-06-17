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
    uint32_t path_nr = 0;
    std::set<UecSrc *> all_srcs;
    std::set<UecSink *> all_sinks;
    // map from path to switches in this path
    std::unordered_map<uint32_t, std::set<uint64_t>> path_switches;
    // map from path to cables in this path
    std::unordered_map<uint32_t, std::set<uint64_t>> path_cables;

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

    // Switch
    bool fail_new_switch(Switch *sw);
    std::set<uint32_t> all_switches;
    bool switchFail(Switch *sw);
    bool switch_fail = false;
    std::unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> failingSwitches;
    std::set<uint32_t> neededSwitches;
    simtime_picosec switch_fail_start = 0;
    simtime_picosec switch_fail_period = 0;
    simtime_picosec switch_fail_next_fail = 0;
    float switch_fail_max_percent = 1;

    bool switchBER(Packet &pkt, Switch *sw, Queue q);
    bool dropPacketsSwichtBER(Packet &pkt);
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
    bool switch_degradation = false;
    simtime_picosec switch_degradation_start = 0;
    simtime_picosec switch_degradation_period = 0;
    simtime_picosec switch_degradation_next_fail = 0;
    float switch_degradation_max_percent = 1;

    bool switchWorstCase(Switch *sw);
    bool switch_worst_case = false;
    simtime_picosec switch_worst_case_start = 0;
    simtime_picosec switch_worst_case_period = 0;
    simtime_picosec switch_worst_case_next_fail = 0;

    // Cable
    bool fail_new_cable(Pipe *p);

    std::set<uint32_t> all_cables;

    bool cableFail(Pipe *p, Packet &pkt);
    unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> failingCables;
    bool cable_fail = false;
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
    simtime_picosec cable_degradation_start = 0;
    simtime_picosec cable_degradation_period = 0;
    simtime_picosec cable_degradation_next_fail = 0;
    float cable_degradation_max_percent = 1;

    bool cableWorstCase(Pipe *p, Packet &pkt);
    bool cable_worst_case = false;
    simtime_picosec cable_worst_case_start = 0;
    simtime_picosec cable_worst_case_period = 0;
    simtime_picosec cable_worst_case_next_fail = 0;

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

    bool nic_worst_case = false;
    bool nicWorstCase(UecSrc *src, UecSink *sink, Packet &pkt);
    simtime_picosec nic_worst_case_start = 0;
    simtime_picosec nic_worst_case_period = 0;
    simtime_picosec nic_worst_case_next_fail = 0;

  private:
};

#endif
