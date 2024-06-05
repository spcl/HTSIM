// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-

#include "failuregenerator.h"
#include "network.h"
#include "pipe.h"
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
        std::geometric_distribution<> dis(0.1);
        int64_t val = dis(gen);
        val += 360;
        if (val > 18000) {
            val = 18000;
        }
        return dis(gen) * 1e+12;
    }
}

int64_t generateTimeCable() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0, 1);
    double random_value = dis(gen);
    if (random_value < 0.8) {
        std::uniform_int_distribution<> dis(0, 300);
        return dis(gen) * 1e+12;
    } else {
        std::geometric_distribution<> dis(0.1);
        int64_t val = dis(gen);
        val += 300;
        if (val > 3600) {
            val = 3600;
        }
        return dis(gen) * 1e+12;
    }
}

int64_t generateTimeNIC() {
    std::random_device rd;
    std::mt19937 gen(rd());

    std::geometric_distribution<> dis(0.1);
    int64_t val = dis(gen);
    val += 600;
    if (val > 1800) {
        val = 1800;
    }
    return dis(gen) * 1e+12;
}

// Map that keeps track of failing switches. Maps from switch id to a pair of time of failure and time of recovery

bool failuregenerator::simSwitchFailures(Packet &pkt, Switch *sw, Queue q) {
    return (switchFail(sw) || switchBER(pkt, sw, q) || switchDegradation(sw) || switchWorstCase(sw));
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
        if (GLOBAL_TIME < switch_fail_next_fail || switch_name.find("UpperPod") == std::string::npos) {
            return false;
        }

        uint64_t failureTime = GLOBAL_TIME;
        uint64_t recoveryTime = GLOBAL_TIME + generateTimeSwitch();
        switch_fail_next_fail = GLOBAL_TIME + switch_fail_period;
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

    if (GLOBAL_TIME < switch_ber_next_fail) {
        return false;
    } else {
        corrupted_packets.insert(pkt_id);
        switch_ber_next_fail = GLOBAL_TIME + switch_ber_period;
        return false;
    }
}
bool failuregenerator::switchDegradation(Switch *sw) {

    if (!switch_degradation) {
        return false;
    }
    uint32_t switch_id = sw->getID();

    if (degraded_switches.find(switch_id) != degraded_switches.end()) {
        bool decision = trueWithProb(0.1);
        if (decision) {
            std::cout << "Packet dropped at SwitchDegradation" << std::endl;
            return true;
        } else {
            return false;
        }
    } else {
        if (GLOBAL_TIME < switch_degradation_next_fail) {
            return false;
        } else {
            int port_nrs = sw->portCount();
            for (int i = 0; i < port_nrs; i++) {
                BaseQueue *q = sw->getPort(i);
                q->update_bit_rate(400000000000);
                switch_degradation_next_fail = GLOBAL_TIME + switch_degradation_period;
                std::cout << "New Switch degraded at:" << GLOBAL_TIME << "Switch_name: " << q->_name
                          << " degraded Queue bitrate now 1000bps " << std::endl;
            }
            degraded_switches.insert(switch_id);

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
        if (GLOBAL_TIME < switch_worst_case_next_fail || switch_name.find("UpperPod") == std::string::npos) {
            return false;
        }

        uint64_t failureTime = GLOBAL_TIME;
        uint64_t recoveryTime = GLOBAL_TIME + generateTimeSwitch();
        switch_worst_case_next_fail = GLOBAL_TIME + switch_worst_case_period;
        failuregenerator::failingSwitches[switch_id] = std::make_pair(failureTime, recoveryTime);
        std::cout << "Failed a new Switch name: " << switch_name << " at " << std::to_string(failureTime) << " for "
                  << std::to_string((recoveryTime - failureTime) / 1e+12) << " seconds" << std::endl;
        return true;
    }
    return false;
}

bool failuregenerator::simCableFailures(Pipe *p, Packet &pkt) {
    return (cableFail(p, pkt) || cableBER(pkt) || cableDegradation(p, pkt) || cableWorstCase(p, pkt));
}

bool failuregenerator::cableFail(Pipe *p, Packet &pkt) {
    if (!cable_fail) {
        return false;
    }

    uint32_t cable_id = p->getID();
    if (failingCables.find(cable_id) != failingCables.end()) {
        std::pair<uint64_t, uint64_t> curCable = failingCables[cable_id];
        uint64_t recoveryTime = curCable.second;

        if (GLOBAL_TIME > recoveryTime) {
            std::cout << "Recovered from Fail" << std::endl;
            failingCables.erase(cable_id);
            return false;
        } else {
            std::cout << "Packet dropped at CableFail" << std::endl;
            return true;
        }
    } else {
        string from_queue = pkt.route()->at(0)->nodename();
        if (GLOBAL_TIME < cable_fail_next_fail || from_queue.find("DST") != std::string::npos ||
            from_queue.find("SRC") != std::string::npos) {
            return false;
        }

        uint64_t failureTime = GLOBAL_TIME;
        uint64_t recoveryTime = GLOBAL_TIME + generateTimeCable();
        cable_fail_next_fail = GLOBAL_TIME + cable_fail_period;
        failuregenerator::failingCables[cable_id] = std::make_pair(failureTime, recoveryTime);
        std::cout << "Failed a new Cable name: " << cable_id << " at " << std::to_string(failureTime) << " for "
                  << std::to_string((recoveryTime - failureTime) / 1e+12) << " seconds" << std::endl;
        return true;
    }
    return false;
}

bool failuregenerator::cableBER(Packet &pkt) {
    if (!cable_ber) {
        return false;
    }

    uint32_t pkt_id = pkt.id();
    string from_queue = pkt.route()->at(0)->nodename();

    if (from_queue.find("DST") != std::string::npos) {
        if (corrupted_packets.find(pkt_id) != corrupted_packets.end()) {
            corrupted_packets.erase(pkt_id);
            std::cout << "Packet dropped at cableBER" << std::endl;
            return true;
        }
    }

    if (GLOBAL_TIME < cable_ber_next_fail) {
        return false;
    } else {
        corrupted_packets.insert(pkt_id);
        cable_ber_next_fail = GLOBAL_TIME + cable_ber_period;
        return false;
    }

    return false;
}

bool failuregenerator::cableDegradation(Pipe *p, Packet &pkt) {
    if (!cable_degradation) {
        return false;
    }
    uint32_t cable_id = p->getID();

    if (degraded_cables.find(cable_id) != degraded_cables.end()) {
        uint32_t degradation_type = degraded_cables[cable_id];
        bool decision = false;
        switch (degradation_type) {
        case 1:
            decision = trueWithProb(0.33);
            break;
        case 2:
            decision = trueWithProb(0.66);
            break;
        case 3:
            decision = true;
            break;

        default:
            decision = false;
            break;
        }
        if (decision) {
            std::cout << "Packet dropped at CableDegradation" << std::endl;
            return true;
        } else {
            return false;
        }
    } else {
        string from_queue = pkt.route()->at(0)->nodename();
        if (GLOBAL_TIME < cable_degradation_next_fail || from_queue.find("DST") != std::string::npos ||
            from_queue.find("SRC") != std::string::npos) {
            return false;
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dist(0.0, 1.0);

        double randomValue = dist(gen);

        int decided_type = 0;

        if (randomValue < 0.9) {
            decided_type = 1;
        } else if (randomValue < 0.99) {
            decided_type = 2;
        } else {
            decided_type = 3;
        }

        cable_degradation_next_fail = GLOBAL_TIME + cable_degradation_period;
        failuregenerator::degraded_cables[cable_id] = decided_type;
        std::cout << "Degraded a new Cable name: " << p->nodename() << std::endl;
        return true;
    }

    return false;
}

bool failuregenerator::cableWorstCase(Pipe *p, Packet &pkt) {
    if (!cable_worst_case) {
        return false;
    }

    uint32_t cable_id = p->getID();
    if (failingCables.find(cable_id) != failingCables.end()) {
        std::pair<uint64_t, uint64_t> curCable = failingCables[cable_id];
        uint64_t recoveryTime = curCable.second;

        if (GLOBAL_TIME > recoveryTime) {
            std::cout << "Recovered from Fail" << std::endl;
            failingCables.erase(cable_id);
            return false;
        } else {
            std::cout << "Packet dropped at cableWorstCase" << std::endl;
            return true;
        }
    } else {
        string from_queue = pkt.route()->at(0)->nodename();
        if (GLOBAL_TIME < cable_worst_case_next_fail || from_queue.find("DST") != std::string::npos ||
            from_queue.find("SRC") != std::string::npos) {
            return false;
        }

        uint64_t failureTime = GLOBAL_TIME;
        uint64_t recoveryTime = GLOBAL_TIME + generateTimeCable();
        cable_worst_case_next_fail = GLOBAL_TIME + cable_worst_case_period;
        failuregenerator::failingCables[cable_id] = std::make_pair(failureTime, recoveryTime);
        std::cout << "Failed a new Cable name: " << cable_id << " at " << std::to_string(failureTime) << " for "
                  << std::to_string((recoveryTime - failureTime) / 1e+12) << " seconds" << std::endl;
        return true;
    }
    return false;
}

bool failuregenerator::simNICFailures(Queue q, Packet &pkt) {
    return (nicFail(q, pkt) || nicDegradation(q, pkt) || nicWorstCase(q, pkt));
}
bool failuregenerator::nicFail(Queue q, Packet &pkt) {

    if (!nic_fail) {
        return false;
    }

    uint32_t packet_from = pkt.from;
    uint32_t packet_to = pkt.to;
    uint32_t nic_id = q.getSwitch()->getID();

    // Am i at a NIC? TODO
    if (!(nic_id == packet_from || nic_id == packet_to)) {
        return false;
    }

    if (failingNICs.find(nic_id) != failingNICs.end()) {
        std::pair<uint64_t, uint64_t> curNic = failingNICs[nic_id];
        uint64_t recoveryTime = curNic.second;
        if (GLOBAL_TIME > recoveryTime) {
            std::cout << "Recovered from Fail" << std::endl;
            failingNICs.erase(nic_id);
            return false;
        } else {
            std::cout << "Packet dropped at nicFail" << std::endl;
            return true;
        }
    } else {
        if (GLOBAL_TIME < nic_fail_next_fail) {
            return false;
        }

        uint64_t failureTime = GLOBAL_TIME;
        uint64_t recoveryTime = GLOBAL_TIME + generateTimeNIC();
        nic_fail_next_fail = GLOBAL_TIME + nic_fail_period;
        failuregenerator::failingNICs[nic_id] = std::make_pair(failureTime, recoveryTime);
        std::cout << "Failed a new NIC name: " << q.nodename() << " at " << std::to_string(failureTime) << " for "
                  << std::to_string((recoveryTime - failureTime) / 1e+12) << " seconds" << std::endl;
        return true;
    }

    return false;
}
bool failuregenerator::nicDegradation(Queue q, Packet &pkt) {
    if (!nic_degradation) {
        return false;
    }

    uint32_t packet_from = pkt.from;
    uint32_t packet_to = pkt.to;
    uint32_t nic_id = q.getSwitch()->getID();

    // Am i at a NIC? TODO
    if (!(nic_id == packet_from || nic_id == packet_to)) {
        return false;
    }

    if (degraded_NICs.find(nic_id) != degraded_NICs.end()) {
        bool decision = trueWithProb(0.05);
        if (decision) {
            std::cout << "Packet dropped at nicDegradation" << std::endl;
            return true;
        } else {
            return false;
        }
    } else {
        if (GLOBAL_TIME < nic_degradation_next_fail) {
            return false;
        } else {
            int port_nrs = q.getSwitch()->portCount();
            for (int i = 0; i < port_nrs; i++) {
                BaseQueue *curq = q.getSwitch()->getPort(i);
                curq->_bitrate = 1000;
            }
            degraded_NICs.insert(nic_id);
            nic_degradation_next_fail = GLOBAL_TIME + nic_degradation_period;
            std::cout << "New NIC degraded Queue bitrate now 1000bps " << std::endl;
            bool decision = trueWithProb(0.05);
            if (decision) {
                std::cout << "Packet dropped at nicDegradation" << std::endl;
                return true;
            } else {
                return false;
            }
        }
    }

    return false;
}
bool failuregenerator::nicWorstCase(Queue q, Packet &pkt) {
    if (!nic_worst_case) {
        return false;
    }

    uint32_t packet_from = pkt.from;
    uint32_t packet_to = pkt.to;
    uint32_t nic_id = q.getSwitch()->getID();

    // Am i at a NIC? TODO
    if (!(nic_id == packet_from || nic_id == packet_to)) {
        return false;
    }

    if (failingNICs.find(nic_id) != failingNICs.end()) {
        std::pair<uint64_t, uint64_t> curNic = failingNICs[nic_id];
        uint64_t recoveryTime = curNic.second;
        if (GLOBAL_TIME > recoveryTime) {
            std::cout << "Recovered from Fail" << std::endl;
            failingNICs.erase(nic_id);
            return false;
        } else {
            std::cout << "Packet dropped at nicWorstCase" << std::endl;
            return true;
        }
    } else {
        if (GLOBAL_TIME < nic_worst_case_next_fail) {
            return false;
        }

        uint64_t failureTime = GLOBAL_TIME;
        uint64_t recoveryTime = GLOBAL_TIME + generateTimeNIC();
        nic_worst_case_next_fail = GLOBAL_TIME + nic_worst_case_period;
        failuregenerator::failingNICs[nic_id] = std::make_pair(failureTime, recoveryTime);
        std::cout << "Failed a new NIC name: " << q.nodename() << " at " << std::to_string(failureTime) << " for "
                  << std::to_string((recoveryTime - failureTime) / 1e+12) << " seconds" << std::endl;
        return true;
    }

    return false;
}

void failuregenerator::parseinputfile() {

    std::cout << "Parsing failuregenerator_input file" << failures_input_file_path << std::endl;

    std::ifstream inputFile(failures_input_file_path);
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
                switch_fail_next_fail = switch_fail_start;
            } else if (key == "Switch-Fail-Period:") {
                switch_fail_period = std::stoll(value);
            } else if (key == "Switch-BER:") {
                switch_ber = (value == "ON");
            } else if (key == "Switch-BER-Start-After:") {
                switch_ber_start = std::stoll(value);
                switch_ber_next_fail = switch_ber_start;
            } else if (key == "Switch-BER-Period:") {
                switch_ber_period = std::stoll(value);
            } else if (key == "Switch-Degradation:") {
                switch_degradation = (value == "ON");
            } else if (key == "Switch-Degradation-Start-After:") {
                switch_degradation_start = std::stoll(value);
                switch_degradation_next_fail = switch_degradation_start;
            } else if (key == "Switch-Degradation-Period:") {
                switch_degradation_period = std::stoll(value);
            } else if (key == "Switch-Worst-Case:") {
                switch_worst_case = (value == "ON");
            } else if (key == "Switch-Worst-Case-Start-After:") {
                switch_worst_case_start = std::stoll(value);
                switch_worst_case_next_fail = switch_worst_case_start;
            } else if (key == "Switch-Worst-Case-Period:") {
                switch_worst_case_period = std::stoll(value);
            } else if (key == "Cable-Fail:") {
                cable_fail = (value == "ON");
            } else if (key == "Cable-Fail-Start-After:") {
                cable_fail_start = std::stoll(value);
                cable_fail_next_fail = cable_fail_start;
            } else if (key == "Cable-Fail-Period:") {
                cable_fail_period = std::stoll(value);
            } else if (key == "Cable-BER:") {
                cable_ber = (value == "ON");
            } else if (key == "Cable-BER-Start-After:") {
                cable_ber_start = std::stoll(value);
                cable_ber_next_fail = cable_ber_start;
            } else if (key == "Cable-BER-Period:") {
                cable_ber_period = std::stoll(value);
            } else if (key == "Cable-Degradation:") {
                cable_degradation = (value == "ON");
            } else if (key == "Cable-Degradation-Start-After:") {
                cable_degradation_start = std::stoll(value);
                cable_degradation_next_fail = cable_degradation_start;
            } else if (key == "Cable-Degradation-Period:") {
                cable_degradation_period = std::stoll(value);
            } else if (key == "Cable-Worst-Case:") {
                cable_worst_case = (value == "ON");
            } else if (key == "Cable-Worst-Case-Start-After:") {
                cable_worst_case_start = std::stoll(value);
                cable_worst_case_next_fail = cable_worst_case_start;
            } else if (key == "Cable-Worst-Case-Period:") {
                cable_worst_case_period = std::stoll(value);
            } else if (key == "NIC-Fail:") {
                nic_fail = (value == "ON");
            } else if (key == "NIC-Fail-Start-After:") {
                nic_fail_start = std::stoll(value);
                nic_fail_next_fail = nic_fail_start;
            } else if (key == "NIC-Fail-Period:") {
                nic_fail_period = std::stoll(value);
            } else if (key == "NIC-Degradation:") {
                nic_degradation = (value == "ON");
            } else if (key == "NIC-Degradation-Start-After:") {
                nic_degradation_start = std::stoll(value);
                nic_degradation_next_fail = nic_degradation_start;
            } else if (key == "NIC-Degradation-Period:") {
                nic_degradation_period = std::stoll(value);
            } else if (key == "NIC-Worst-Case:") {
                nic_worst_case = (value == "ON");
            } else if (key == "NIC-Worst-Case-Start-After:") {
                nic_worst_case_start = std::stoll(value);
                nic_worst_case_next_fail = nic_worst_case_start;
            } else if (key == "NIC-Worst-Case-Period:") {
                nic_worst_case_period = std::stoll(value);
            }
        }
        inputFile.close();
    } else {
        std::cout << "Could not open failuregenerator_input file, all failures are turned off" << std::endl;
    }
}
