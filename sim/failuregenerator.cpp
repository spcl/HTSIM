// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-

#include "failuregenerator.h"
#include "network.h"
#include "pipe.h"
#include "switch.h"
#include "uec.h"
#include <ctime>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>

std::random_device rd;
std::mt19937 gen(rd());

// returns true with probability prob
bool trueWithProb(double prob) {
    std::uniform_real_distribution<> dis(0, 1);
    return dis(gen) < prob;
}

int64_t generateTimeSwitch() {
    std::uniform_real_distribution<> dis(0, 1);
    if (dis(gen) < 0.5) {
        std::uniform_int_distribution<> dis(0, 360);
        return dis(gen) * 1e+12;
    } else {
        std::geometric_distribution<> dis(0.1);
        int64_t val = dis(gen) + 360;
        return std::min(val, int64_t(18000)) * 1e12;
    }
}

int64_t generateTimeCable() {
    std::uniform_real_distribution<> dis(0, 1);
    if (dis(gen) < 0.8) {
        std::uniform_int_distribution<> dis(0, 300);
        return dis(gen) * 1e+12;
    } else {
        std::geometric_distribution<> dis(0.1);
        int64_t val = dis(gen) + 300;
        return std::min(val, int64_t(3600)) * 1e12;
    }
}

int64_t generateTimeNIC() {
    std::geometric_distribution<> dis(0.1);
    int64_t val = dis(gen) + 600;
    return std::min(val, int64_t(1800)) * 1e12;
}

// Map that keeps track of failing switches. Maps from switch id to a pair of time of failure and time of recovery

bool failuregenerator::simSwitchFailures(Packet &pkt, Switch *sw, Queue q) {
    return (switchFail(sw) || switchBER(pkt, sw, q) || switchDegradation(sw) || switchWorstCase(sw));
}

bool failuregenerator::fail_new_switch(Switch *sw) {

    uint32_t switch_id = sw->getID();
    std::string switch_name = sw->nodename();

    if (neededSwitches.find(switch_id) != neededSwitches.end()) {
        std::cout << "Did not fail critical Switch name: " << switch_name << std::endl;
        return false;
    }

    uint64_t failureTime = GLOBAL_TIME;
    uint64_t recoveryTime = GLOBAL_TIME + generateTimeSwitch();
    failingSwitches[switch_id] = std::make_pair(failureTime, recoveryTime);

    if (!check_connectivity()) {
        failingSwitches.erase(switch_id);
        neededSwitches.insert(switch_id);
        std::cout << "Did not fail critical Switch name: " << switch_name << std::endl;
        return false;
    }

    switch_fail_next_fail = GLOBAL_TIME + switch_fail_period;

    std::cout << "Failed a new Switch name: " << switch_name << " at " << std::to_string(failureTime) << " for "
              << std::to_string((recoveryTime - failureTime) / 1e+12) << " seconds" << std::endl;

    return true;
}

bool failuregenerator::switchFail(Switch *sw) {
    if (!switch_fail) {
        return false;
    }
    uint32_t switch_id = sw->getID();
    std::string switch_name = sw->nodename();

    if (failingSwitches.find(switch_id) != failingSwitches.end()) {
        std::pair<uint64_t, uint64_t> curSwitch = failingSwitches[switch_id];
        uint64_t recoveryTime = curSwitch.second;

        if (GLOBAL_TIME > recoveryTime) {
            std::cout << "Recovered from Fail" << std::endl;
            neededSwitches.clear();
            failingSwitches.erase(switch_id);
            return false;
        } else {
            std::cout << "Packet dropped at SwitchFail" << std::endl;
            return true;
        }
    } else {
        // if (GLOBAL_TIME < switch_fail_next_fail || switch_name.find("UpperPod") == std::string::npos) {
        //     return false;
        // }
        if (GLOBAL_TIME < switch_fail_next_fail) {
            return false;
        }

        return fail_new_switch(sw);
    }
}

bool failuregenerator::switchBER(Packet &pkt, Switch *sw, Queue q) {

    if (!switch_ber) {
        return false;
    }

    uint32_t pkt_id = pkt.id();

    if (GLOBAL_TIME < switch_ber_next_fail) {
        return false;
    } else {
        corrupted_packets.insert(pkt_id);
        switch_ber_next_fail = GLOBAL_TIME + switch_ber_period;
        return false;
    }
}

bool failuregenerator::dropPacketsSwichtBER(Packet &pkt) {
    uint32_t pkt_id = pkt.id();
    if (corrupted_packets.find(pkt_id) != corrupted_packets.end()) {
        corrupted_packets.erase(pkt_id);
        std::cout << "Packet dropped at SwitchBER" << std::endl;
        return true;
    }
    return false;
}

bool failuregenerator::switchDegradation(Switch *sw) {

    if (!switch_degradation) {
        return false;
    }
    uint32_t switch_id = sw->getID();

    if (degraded_switches.find(switch_id) != degraded_switches.end()) {
        if (trueWithProb(0.1)) {
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

            if (trueWithProb(0.1)) {
                std::cout << "Packet dropped at SwitchDegradation" << std::endl;
                return true;
            } else {
                return false;
            }
        }
    }
}

bool failuregenerator::switchWorstCase(Switch *sw) {
    if (!switch_worst_case) {
        return false;
    }
    uint32_t switch_id = sw->getID();
    std::string switch_name = sw->nodename();

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

        return fail_new_switch(sw);
    }
}

bool failuregenerator::simCableFailures(Pipe *p, Packet &pkt) {
    return (cableFail(p, pkt) || cableBER(pkt) || cableDegradation(p, pkt) || cableWorstCase(p, pkt));
}

bool failuregenerator::fail_new_cable(Pipe *p) {

    uint32_t cable_id = p->getID();
    std::string cable_name = p->nodename();

    if (cable_name.find("callbackpipe") != std::string::npos) {
        return false;
    }

    if (neededCables.find(cable_id) != neededCables.end()) {
        std::cout << "Did not fail critical Cable name: " << cable_name << std::endl;
        return false;
    }

    uint64_t failureTime = GLOBAL_TIME;
    uint64_t recoveryTime = GLOBAL_TIME + generateTimeCable();

    failingCables[cable_id] = std::make_pair(failureTime, recoveryTime);

    if (!check_connectivity()) {
        failingCables.erase(cable_id);
        neededCables.insert(cable_id);
        std::cout << "Did not fail critical Cable name: " << cable_name << std::endl;
        return false;
    }

    cable_fail_next_fail = GLOBAL_TIME + cable_fail_period;

    std::cout << "Failed a new Cable name: " << cable_name << " at " << std::to_string(failureTime) << " for "
              << std::to_string((recoveryTime - failureTime) / 1e+12) << " seconds" << std::endl;
    return true;
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
            neededCables.clear();
            failingCables.erase(cable_id);
            return false;
        } else {
            std::cout << "Packet dropped at CableFail" << std::endl;
            return true;
        }
    } else {
        std::string from_queue = pkt.route()->at(0)->nodename();
        // if (GLOBAL_TIME < cable_fail_next_fail || from_queue.find("DST") != std::string::npos ||
        //     from_queue.find("SRC") != std::string::npos) {

        if (GLOBAL_TIME < cable_fail_next_fail) {
            return false;
        }

        return fail_new_cable(p);
    }
}

bool failuregenerator::cableBER(Packet &pkt) {
    if (!cable_ber) {
        return false;
    }

    uint32_t pkt_id = pkt.id();
    std::string from_queue = pkt.route()->at(0)->nodename();

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
        std::string from_queue = pkt.route()->at(0)->nodename();
        if (GLOBAL_TIME < cable_degradation_next_fail || from_queue.find("DST") != std::string::npos ||
            from_queue.find("SRC") != std::string::npos) {
            return false;
        }
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
        degraded_cables[cable_id] = decided_type;
        std::cout << "Degraded a new Cable name: " << p->nodename() << std::endl;
        return true;
    }
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
        std::string from_queue = pkt.route()->at(0)->nodename();
        if (GLOBAL_TIME < cable_worst_case_next_fail || from_queue.find("DST") != std::string::npos ||
            from_queue.find("SRC") != std::string::npos) {
            return false;
        }

        return fail_new_cable(p);
    }
    return false;
}

bool failuregenerator::simNICFailures(UecSrc *src, UecSink *sink, Packet &pkt) {
    return (nicFail(src, sink, pkt) || nicDegradation(src, sink, pkt) || nicWorstCase(src, sink, pkt));
}

bool failuregenerator::fail_new_nic(u_int32_t nic_id) {
    uint64_t failureTime = GLOBAL_TIME;
    uint64_t recoveryTime = GLOBAL_TIME + generateTimeNIC();
    nic_fail_next_fail = GLOBAL_TIME + nic_fail_period;
    failingNICs[nic_id] = std::make_pair(failureTime, recoveryTime);
    std::cout << "Failed a new NIC name: " << nic_id << " at " << std::to_string(failureTime) << " for "
              << std::to_string((recoveryTime - failureTime) / 1e+12) << " seconds" << std::endl;
    return true;
}

bool failuregenerator::nicFail(UecSrc *src, UecSink *sink, Packet &pkt) {

    if (!nic_fail) {
        return false;
    }

    uint32_t nic_id = -1;
    if (src == NULL) {
        nic_id = sink->to;
    } else {
        nic_id = src->from;
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
        return fail_new_nic(nic_id);
    }
}
bool failuregenerator::nicDegradation(UecSrc *src, UecSink *sink, Packet &pkt) {
    if (!nic_degradation) {
        return false;
    }

    uint32_t nic_id = -1;
    if (src == NULL) {
        nic_id = sink->to;
    } else {
        nic_id = src->from;
    }

    if (degraded_NICs.find(nic_id) != degraded_NICs.end()) {
        if (trueWithProb(0.05)) {
            std::cout << "Packet dropped at nicDegradation" << std::endl;
            return true;
        } else {
            return false;
        }
    } else {
        if (GLOBAL_TIME < nic_degradation_next_fail) {
            return false;
        } else {
            // int port_nrs = q.getSwitch()->portCount();
            // for (int i = 0; i < port_nrs; i++) {
            //     BaseQueue *curq = q.getSwitch()->getPort(i);
            //     curq->_bitrate = 1000;
            // }
            degraded_NICs.insert(nic_id);
            nic_degradation_next_fail = GLOBAL_TIME + nic_degradation_period;
            std::cout << "New NIC degraded Queue bitrate now 1000bps " << std::endl;

            if (trueWithProb(0.05)) {
                std::cout << "Packet dropped at nicDegradation" << std::endl;
                return true;
            } else {
                return false;
            }
        }
    }
}
bool failuregenerator::nicWorstCase(UecSrc *src, UecSink *sink, Packet &pkt) {
    if (!nic_worst_case) {
        return false;
    }

    uint32_t nic_id = -1;
    if (src == NULL) {
        nic_id = sink->to;
    } else {
        nic_id = src->from;
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

        return fail_new_nic(nic_id);
    }
}

std::pair<std::pair<std::set<uint32_t>, std::set<uint32_t>>, std::string>
failuregenerator::get_path_switches_cables(uint32_t path_id, UecSrc *src) {

    std::vector<int> path_ids = src->get_path_ids();
    PacketFlow flow = src->get_flow();
    Route r = *src->get_route();

    std::string path;

    std::set<uint32_t> switches;
    std::set<uint32_t> cables;

    UecPacket *packet = UecPacket::newpkt(flow, r, uint64_t(0), uint64_t(0), uint16_t(0), false, src->to);
    packet->set_route(*src->get_route());
    packet->set_pathid(path_ids[path_id]);
    packet->from = src->from;
    packet->to = src->to;
    packet->tag = src->tag;

    while (true) {
        Queue *next_queue = dynamic_cast<Queue *>(packet->getNextHopOfPacket());
        Pipe *next_pipe = dynamic_cast<Pipe *>(packet->getNextHopOfPacket());
        Switch *next_switch = dynamic_cast<Switch *>(packet->getNextHopOfPacket());
        if (next_switch == nullptr) {
            if (next_queue) {
                path += " -> " + next_queue->nodename();
            } else {
                std::cout << "Error: last next_queue is nullptr" << std::endl;
            }
            if (next_pipe) {
                path += " -> " + next_pipe->nodename();
                cables.insert(next_pipe->getID());
            } else {
                std::cout << "Error: last next_pipe is nullptr" << std::endl;
            }
            break;
        }
        if (next_queue) {
            path += " -> " + next_queue->nodename();
        } else {
            std::cout << "Error: next_queue is nullptr" << std::endl;
        }
        if (next_pipe) {
            path += " -> " + next_pipe->nodename();
            cables.insert(next_pipe->getID());
        } else {
            std::cout << "Error: next_pipe is nullptr" << std::endl;
        }
        if (next_switch) {
            path += " -> " + next_switch->nodename();
            switches.insert(next_switch->getID());

            r = *next_switch->getNextHop(*packet, NULL);
            packet->set_route(r);
            r = *next_switch->getNextHop(*packet, NULL);
            packet->set_route(r);
        } else {
            std::cout << "Error: next_switch is nullptr" << std::endl;
        }
    }

    return std::make_pair(std::make_pair(switches, cables), path);
}

bool failuregenerator::check_connectivity() {
    for (UecSrc *src : all_srcs) {

        int route_len = src->get_paths().size();

        bool src_dst_connected = false;

        for (int i = 0; i < route_len; i++) {
            std::pair<std::pair<std::set<uint32_t>, std::set<uint32_t>>, std::string> switches_cables_on_path_string =
                    get_path_switches_cables(i, src);
            std::pair<std::set<uint32_t>, std::set<uint32_t>> switches_cables_on_path =
                    switches_cables_on_path_string.first;
            std::string found_path = switches_cables_on_path_string.second;
            std::set<uint32_t> switches_on_path = switches_cables_on_path.first;
            std::set<uint32_t> cables_on_path = switches_cables_on_path.second;

            bool all_switches_active = true;
            bool all_cables_active = true;

            for (uint32_t sw : switches_on_path) {
                if (failingSwitches.find(sw) != failingSwitches.end()) {
                    all_switches_active = false;
                    break;
                }
            }

            for (uint32_t cable : cables_on_path) {

                if (failingCables.find(cable) != failingCables.end()) {
                    all_cables_active = false;
                    break;
                }
            }
            if (all_switches_active && all_cables_active) {
                std::cout << "Path " << src->nodename() << " is connected by" << found_path << ::endl;
                src_dst_connected = true;
                break;
            }
        }
        if (!src_dst_connected) {
            return false;
        }
    }

    return true;
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
