// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef FAILURE_GENERATOR_H
#define FAILURE_GENERATOR_H

#include "network.h"
#include "switch.h"
#include <chrono>
#include <set>
#include <unordered_map>

class Pipe; // Forward declaration
class failuregenerator {

  public:
    failuregenerator();
    void parseinputfile();

    bool simSwitchFailures(Packet &pkt, Switch *sw, Queue q);
    bool simCableFailures(Pipe *p, Packet &pkt);
    bool simNICFailures(Queue q, Packet &pkt);

    // Switch
    bool switchFail(Switch *sw);
    static bool switch_fail;
    static std::unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> failingSwitches;
    static simtime_picosec switch_fail_start;
    static simtime_picosec switch_fail_period;
    static simtime_picosec switch_fail_last_fail;

    bool switchBER(Packet &pkt, Switch *sw, Queue q);
    static std::set<uint32_t> corrupted_packets;
    static bool switch_ber;
    static simtime_picosec switch_ber_start;
    static simtime_picosec switch_ber_period;
    static simtime_picosec switch_ber_last_fail;

    bool switchDegradation(Switch *sw);
    static std::set<uint32_t> degraded_switches;
    static bool switch_degradation;
    static simtime_picosec switch_degradation_start;
    static simtime_picosec switch_degradation_period;
    static simtime_picosec switch_degradation_last_fail;

    bool switchWorstCase(Switch *sw);
    static bool switch_worst_case;
    static simtime_picosec switch_worst_case_start;
    static simtime_picosec switch_worst_case_period;
    static simtime_picosec switch_worst_case_last_fail;

    bool cableFail(Pipe *p, Packet &pkt);
    static unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> failingCables;
    static bool cable_fail;
    static simtime_picosec cable_fail_start;
    static simtime_picosec cable_fail_period;
    static simtime_picosec cable_fail_last_fail;

    bool cableBER(Packet &pkt);
    static bool cable_ber;
    static simtime_picosec cable_ber_start;
    static simtime_picosec cable_ber_period;
    static simtime_picosec cable_ber_last_fail;

    bool cableDegradation(Pipe *p, Packet &pkt);
    static bool cable_degradation;
    static std::unordered_map<uint32_t, uint32_t> degraded_cables;
    static simtime_picosec cable_degradation_start;
    static simtime_picosec cable_degradation_period;
    static simtime_picosec cable_degradation_last_fail;

    bool cableWorstCase(Pipe *p, Packet &pkt);
    static bool cable_worst_case;
    static simtime_picosec cable_worst_case_start;
    static simtime_picosec cable_worst_case_period;
    static simtime_picosec cable_worst_case_last_fail;

    // NIC
    static bool nic_fail;
    bool nicFail(Queue q, Packet &pkt);
    static std::unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> failingNICs;
    static simtime_picosec nic_fail_start;
    static simtime_picosec nic_fail_period;
    static simtime_picosec nic_fail_last_fail;

    static bool nic_degradation;
    bool nicDegradation(Queue q, Packet &pkt);
    static std::set<uint32_t> degraded_NICs;
    static simtime_picosec nic_degradation_start;
    static simtime_picosec nic_degradation_period;
    static simtime_picosec nic_degradation_last_fail;

    static bool nic_worst_case;
    bool nicWorstCase(Queue q, Packet &pkt);
    static simtime_picosec nic_worst_case_start;
    static simtime_picosec nic_worst_case_period;
    static simtime_picosec nic_worst_case_last_fail;

  private:
};

#endif
