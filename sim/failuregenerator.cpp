// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-

#include "failuregenerator.h"
#include "network.h"
#include "switch.h"
#include <ctime>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>

// returns true with probability prob
bool trueWithProb(double prob) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0, 1);
    double random_value = dis(gen);
    if (random_value < prob) {
        return true;
    } else {
        return false;
    }
}

int64_t generateTimeSwitch() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0, 1);
    double random_value = dis(gen);
    if (random_value < 0.5) {
        std::uniform_int_distribution<> dis(0, 360);
        return dis(gen) * 1e+12;
    } else {
        std::geometric_distribution<> dis(0.01);
        int64_t val = dis(gen);
        val += 360;
        return dis(gen) * 1e+12;
    }
}

// Map that keeps track of failing switches. Maps from switch id to a pair of time of failure and time of recovery
std::unordered_map<uint32_t, std::pair<uint64_t, uint64_t>> failuregenerator::failingSwitches;
bool failuregenerator::switch_fail = false;
simtime_picosec failuregenerator::switch_fail_start = 0;
simtime_picosec failuregenerator::switch_fail_period = 0;
simtime_picosec failuregenerator::switch_fail_last_fail = 0;

std::set<uint32_t> failuregenerator::corrupted_packets;
bool failuregenerator::switch_ber = false;
simtime_picosec failuregenerator::switch_ber_start = 0;
simtime_picosec failuregenerator::switch_ber_period = 0;
simtime_picosec failuregenerator::switch_ber_last_fail = 0;

std::set<uint32_t> failuregenerator::degraded_switches;
bool failuregenerator::switch_degradation = false;
simtime_picosec failuregenerator::switch_degradation_start = 0;
simtime_picosec failuregenerator::switch_degradation_period = 0;
simtime_picosec failuregenerator::switch_degradation_last_fail = 0;

bool failuregenerator::switch_worst_case = false;
simtime_picosec failuregenerator::switch_worst_case_start = 0;
simtime_picosec failuregenerator::switch_worst_case_period = 0;
simtime_picosec failuregenerator::switch_worst_case_last_fail = 0;
bool failuregenerator::cable_fail = false;
simtime_picosec failuregenerator::cable_fail_start = 0;
simtime_picosec failuregenerator::cable_fail_period = 0;
simtime_picosec failuregenerator::cable_fail_last_fail = 0;

bool failuregenerator::cable_ber = false;
simtime_picosec failuregenerator::cable_ber_start = 0;
simtime_picosec failuregenerator::cable_ber_period = 0;
simtime_picosec failuregenerator::cable_ber_last_fail = 0;

bool failuregenerator::cable_degradation = false;
simtime_picosec failuregenerator::cable_degradation_start = 0;
simtime_picosec failuregenerator::cable_degradation_period = 0;
simtime_picosec failuregenerator::cable_degradation_last_fail = 0;

bool failuregenerator::cable_worst_case = false;
simtime_picosec failuregenerator::cable_worst_case_start = 0;
simtime_picosec failuregenerator::cable_worst_case_period = 0;
simtime_picosec failuregenerator::cable_worst_case_last_fail = 0;

bool failuregenerator::nic_fail = false;
simtime_picosec failuregenerator::nic_fail_start = 0;
simtime_picosec failuregenerator::nic_fail_period = 0;
simtime_picosec failuregenerator::nic_fail_last_fail = 0;

bool failuregenerator::nic_degradation = false;
simtime_picosec failuregenerator::nic_degradation_start = 0;
simtime_picosec failuregenerator::nic_degradation_period = 0;
simtime_picosec failuregenerator::nic_degradation_last_fail = 0;

bool failuregenerator::nic_worst_case = false;
simtime_picosec failuregenerator::nic_worst_case_start = 0;
simtime_picosec failuregenerator::nic_worst_case_period = 0;
simtime_picosec failuregenerator::nic_worst_case_last_fail = 0;

failuregenerator::failuregenerator() { parseinputfile(); }

bool failuregenerator::simSwitchFailures(Packet &pkt, Switch *sw, Queue q) {
    return (switchFail(sw) || switchBER(pkt, sw, q) || switchDegradation(sw, q) || switchWorstCase(sw));
}

bool failuregenerator::switchFail(Switch *sw) {
    if (!switch_fail) {
        return false;
    }
    uint32_t switch_id = sw->getID();
    string switch_name = sw->nodename();

    if (failingSwitches.find(switch_id) != failingSwitches.end()) {
        std::pair<uint64_t, uint64_t> curSwitch = failingSwitches[switch_id];
        uint64_t recoveryTime = curSwitch.second;

        if (GLOBAL_TIME > recoveryTime) {
            std::cout << "Recovered from Fail" << std::endl;
            failingSwitches.erase(switch_id);
            return false;
        } else {
            std::cout << "Packet dropped at SwitchFail" << std::endl;
            return true;
        }
    } else {
        if (GLOBAL_TIME < switch_fail_start || GLOBAL_TIME < switch_fail_last_fail + switch_fail_period ||
            switch_name.find("UpperPod") == std::string::npos) {
            return false;
        }

        uint64_t failureTime = GLOBAL_TIME;
        uint64_t recoveryTime = GLOBAL_TIME + generateTimeSwitch();
        switch_fail_last_fail = GLOBAL_TIME;
        failuregenerator::failingSwitches[switch_id] = std::make_pair(failureTime, recoveryTime);
        std::cout << "Failed a new Switch name: " << switch_name << " at " << std::to_string(failureTime) << " for "
                  << std::to_string((recoveryTime - failureTime) / 1e+12) << " seconds" << std::endl;
        return true;
    }
    return false;
}

bool failuregenerator::switchBER(Packet &pkt, Switch *sw, Queue q) {

    if (!switch_ber) {
        return false;
    }

    uint32_t pkt_id = pkt.id();

    if (q.getRemoteEndpoint() == NULL) {
        if (corrupted_packets.find(pkt_id) != corrupted_packets.end()) {
            corrupted_packets.erase(pkt_id);
            std::cout << "Packet dropped at SwitchBER" << std::endl;
            return true;
        }
    }

    if (GLOBAL_TIME < switch_ber_start || GLOBAL_TIME < switch_ber_last_fail + switch_ber_period) {
        return false;
    } else {
        corrupted_packets.insert(pkt_id);
        switch_ber_last_fail = GLOBAL_TIME;
        return false;
    }
}
bool failuregenerator::switchDegradation(Switch *sw, Queue q) {

    if (!switch_degradation) {
        return false;
    }
    uint32_t switch_id = sw->getID();

    if (failingSwitches.find(switch_id) != failingSwitches.end()) {
        bool decision = trueWithProb(0.1);
        if (decision) {
            std::cout << "Packet dropped at SwitchDegradation" << std::endl;
            return true;
        } else {
            return false;
        }
    } else {
        if (GLOBAL_TIME < switch_ber_start || GLOBAL_TIME < switch_ber_last_fail + switch_ber_period) {
            return false;
        } else {
            int port_nrs = sw->portCount();
            for (int i = 0; i < port_nrs; i++) {
                BaseQueue *q = sw->getPort(i);
                q->_bitrate = 1000;
            }
            degraded_switches.insert(switch_id);
            std::cout << "New Switch degraded Queue bitrate now 1000bps " << std::endl;
            bool decision = trueWithProb(0.1);
            if (decision) {
                std::cout << "Packet dropped at SwitchDegradation" << std::endl;
                return true;
            } else {
                return false;
            }
        }
    }

    return false;
}

bool failuregenerator::switchWorstCase(Switch *sw) {
    if (!switch_worst_case) {
        return false;
    }
    uint32_t switch_id = sw->getID();
    string switch_name = sw->nodename();

    if (failingSwitches.find(switch_id) != failingSwitches.end()) {
        std::pair<uint64_t, uint64_t> curSwitch = failingSwitches[switch_id];
        uint64_t recoveryTime = curSwitch.second;

        if (GLOBAL_TIME > recoveryTime) {
            std::cout << "Recovered from Fail" << std::endl;
            failingSwitches.erase(switch_id);
            return false;
        } else {
            std::cout << "Packet dropped at switchWorstCase" << std::endl;
            return true;
        }
    } else {
        if (GLOBAL_TIME < switch_worst_case_start ||
            GLOBAL_TIME < switch_worst_case_last_fail + switch_worst_case_period ||
            switch_name.find("UpperPod") == std::string::npos) {
            return false;
        }

        uint64_t failureTime = GLOBAL_TIME;
        uint64_t recoveryTime = GLOBAL_TIME + generateTimeSwitch();
        switch_worst_case_last_fail = GLOBAL_TIME;
        failuregenerator::failingSwitches[switch_id] = std::make_pair(failureTime, recoveryTime);
        std::cout << "Failed a new Switch name: " << switch_name << " at " << std::to_string(failureTime) << " for "
                  << std::to_string((recoveryTime - failureTime) / 1e+12) << " seconds" << std::endl;
        return true;
    }
    return false;
}

bool failuregenerator::simCableFailures() {
    if (trueWithProb(0.1)) {
        return true;
    } else {
        return false;
    }
}

bool failuregenerator::simNICFailures() { return false; }

void failuregenerator::parseinputfile() {

    std::ifstream inputFile("../failuregenerator_input.txt");
    std::string line;

    if (inputFile.is_open()) {
        while (getline(inputFile, line)) {
            std::istringstream iss(line);
            std::string key, value;
            if (!(iss >> key >> value)) {
                break;
            } // error

            if (key == "Switch-Fail:") {
                switch_fail = (value == "ON");
            } else if (key == "Switch-Fail-Start-After:") {
                switch_fail_start = std::stoll(value);
            } else if (key == "Switch-Fail-Period:") {
                switch_fail_period = std::stoll(value);
            } else if (key == "Switch-BER:") {
                switch_ber = (value == "ON");
            } else if (key == "Switch-BER-Start-After:") {
                switch_ber_start = std::stoll(value);
            } else if (key == "Switch-BER-Period:") {
                switch_ber_period = std::stoll(value);
            } else if (key == "Switch-Degradation:") {
                switch_degradation = (value == "ON");
            } else if (key == "Switch-Degradation-Start-After:") {
                switch_degradation_start = std::stoll(value);
            } else if (key == "Switch-Degradation-Period:") {
                switch_degradation_period = std::stoll(value);
            } else if (key == "Switch-Worst-Case:") {
                switch_worst_case = (value == "ON");
            } else if (key == "Switch-Worst-Case-Start-After:") {
                switch_worst_case_start = std::stoll(value);
            } else if (key == "Switch-Worst-Case-Period:") {
                switch_worst_case_period = std::stoll(value);
            } else if (key == "Cable-Fail:") {
                cable_fail = (value == "ON");
            } else if (key == "Cable-Fail-Start-After:") {
                cable_fail_start = std::stoll(value);
            } else if (key == "Cable-Fail-Period:") {
                cable_fail_period = std::stoll(value);
            } else if (key == "Cable-BER:") {
                cable_ber = (value == "ON");
            } else if (key == "Cable-BER-Start-After:") {
                cable_ber_start = std::stoll(value);
            } else if (key == "Cable-BER-Period:") {
                cable_ber_period = std::stoll(value);
            } else if (key == "Cable-Degradation:") {
                cable_degradation = (value == "ON");
            } else if (key == "Cable-Degradation-Start-After:") {
                cable_degradation_start = std::stoll(value);
            } else if (key == "Cable-Degradation-Period:") {
                cable_degradation_period = std::stoll(value);
            } else if (key == "Cable-Worst-Case:") {
                cable_worst_case = (value == "ON");
            } else if (key == "Cable-Worst-Case-Start-After:") {
                cable_worst_case_start = std::stoll(value);
            } else if (key == "Cable-Worst-Case-Period:") {
                cable_worst_case_period = std::stoll(value);
            } else if (key == "NIC-Fail:") {
                nic_fail = (value == "ON");
            } else if (key == "NIC-Fail-Start-After:") {
                nic_fail_start = std::stoll(value);
            } else if (key == "NIC-Fail-Period:") {
                nic_fail_period = std::stoll(value);
            } else if (key == "NIC-Degradation:") {
                nic_degradation = (value == "ON");
            } else if (key == "NIC-Degradation-Start-After:") {
                nic_degradation_start = std::stoll(value);
            } else if (key == "NIC-Degradation-Period:") {
                nic_degradation_period = std::stoll(value);
            } else if (key == "NIC-Worst-Case:") {
                nic_worst_case = (value == "ON");
            } else if (key == "NIC-Worst-Case-Start-After:") {
                nic_worst_case_start = std::stoll(value);
            } else if (key == "NIC-Worst-Case-Period:") {
                nic_worst_case_period = std::stoll(value);
            }
        }
        inputFile.close();
    } else {
        std::cout << "Error opening failuregenerator_input file" << std::endl;
    }
}
