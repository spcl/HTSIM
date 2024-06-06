// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef FAILURE_GENERATOR_H
#define FAILURE_GENERATOR_H

#include "network.h"
#include "switch.h"
#include <chrono>
#include <set>
#include <unordered_map>

class Pipe;
class UecSrc;
class UecSink;
class failuregenerator {

  public:
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
    bool switchFail(Switch *sw);
    bool switch_fail = false;
    std::unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> failingSwitches;
    simtime_picosec switch_fail_start = 0;
    simtime_picosec switch_fail_period = 0;
    simtime_picosec switch_fail_next_fail = 0;

    bool switchBER(Packet &pkt, Switch *sw, Queue q);
    std::set<uint32_t> corrupted_packets;
    bool switch_ber = false;
    simtime_picosec switch_ber_start = 0;
    simtime_picosec switch_ber_period = 0;
    simtime_picosec switch_ber_next_fail = 0;

    bool switchDegradation(Switch *sw);
    std::set<uint32_t> degraded_switches;
    bool switch_degradation = false;
    simtime_picosec switch_degradation_start = 0;
    simtime_picosec switch_degradation_period = 0;
    simtime_picosec switch_degradation_next_fail = 0;

    bool switchWorstCase(Switch *sw);
    bool switch_worst_case = false;
    simtime_picosec switch_worst_case_start = 0;
    simtime_picosec switch_worst_case_period = 0;
    simtime_picosec switch_worst_case_next_fail = 0;

    bool cableFail(Pipe *p, Packet &pkt);
    unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> failingCables;
    bool cable_fail = false;
    simtime_picosec cable_fail_start = 0;
    simtime_picosec cable_fail_period = 0;
    simtime_picosec cable_fail_next_fail = 0;

    bool cableBER(Packet &pkt);
    bool cable_ber;
    simtime_picosec cable_ber_start = 0;
    simtime_picosec cable_ber_period = 0;
    simtime_picosec cable_ber_next_fail = 0;

    bool cableDegradation(Pipe *p, Packet &pkt);
    bool cable_degradation = false;
    std::unordered_map<uint32_t, uint32_t> degraded_cables;
    simtime_picosec cable_degradation_start = 0;
    simtime_picosec cable_degradation_period = 0;
    simtime_picosec cable_degradation_next_fail = 0;

    bool cableWorstCase(Pipe *p, Packet &pkt);
    bool cable_worst_case = false;
    simtime_picosec cable_worst_case_start = 0;
    simtime_picosec cable_worst_case_period = 0;
    simtime_picosec cable_worst_case_next_fail = 0;

    // NIC
    bool nic_fail = false;
    bool nicFail(UecSrc *src, UecSink *sink, Packet &pkt);
    std::unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> failingNICs;
    simtime_picosec nic_fail_start = 0;
    simtime_picosec nic_fail_period = 0;
    simtime_picosec nic_fail_next_fail = 0;

    bool nic_degradation = false;
    bool nicDegradation(UecSrc *src, UecSink *sink, Packet &pkt);
    std::set<uint32_t> degraded_NICs;
    simtime_picosec nic_degradation_start = 0;
    simtime_picosec nic_degradation_period = 0;
    simtime_picosec nic_degradation_next_fail = 0;

    bool nic_worst_case = false;
    bool nicWorstCase(UecSrc *src, UecSink *sink, Packet &pkt);
    simtime_picosec nic_worst_case_start = 0;
    simtime_picosec nic_worst_case_period = 0;
    simtime_picosec nic_worst_case_next_fail = 0;

  private:
};

#endif
