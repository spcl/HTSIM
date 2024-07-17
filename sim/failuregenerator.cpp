// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-

#include "failuregenerator.h"
#include "network.h"
#include "pipe.h"
#include "switch.h"
#include "uec.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>

std::random_device rd;
std::mt19937 gen(rd());
bool failuregenerator::_use_timeouts = false;

// returns true with probability prob
bool trueWithProb(double prob) {
    std::uniform_real_distribution<> dis(0, 1);
    return dis(gen) < prob;
}

int64_t generateTimeSwitch() {
    if (FAILURE_GENERATOR->switch_fail_duration) {
        return FAILURE_GENERATOR->switch_fail_duration_time;
    } else {
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
}

int64_t generateTimeCable() {
    if (FAILURE_GENERATOR->cable_fail_duration) {
        return FAILURE_GENERATOR->cable_fail_duration_time;
    } else {
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
}

int64_t generateTimeNIC() {
    std::geometric_distribution<> dis(0.1);
    int64_t val = dis(gen) + 600;
    return std::min(val, int64_t(1800)) * 1e12;
}

void failuregenerator::addRandomPacketDrop(Packet &pkt, UecSrc *src) {

    if (!randomPacketDrop || GLOBAL_TIME > stop_failures_after) {
        return;
    }
    // return true with probability 10^-rate
    std::uniform_real_distribution<> dis(0, 1);
    if (dis(gen) < std::pow(10, -(float)dropRate)) {
        uint32_t pkt_id = pkt.id();
        randomDroppedPackets[pkt_id] = src;
    }
}

void failuregenerator::dropRandomPacketSink(Packet &pkt) {
    if (GLOBAL_TIME > stop_failures_after) {
        return;
    }
    uint32_t pkt_id = pkt.id();
    if (FAILURE_GENERATOR->randomDroppedPackets.find(pkt_id) != FAILURE_GENERATOR->randomDroppedPackets.end()) {
        FAILURE_GENERATOR->randomDroppedPackets.erase(pkt_id);
        FAILURE_GENERATOR->nr_dropped_packets++;
        FAILURE_GENERATOR->_list_random_packet_drops.push_back(GLOBAL_TIME);

        // Return if using a timeout, otherwise transform it in a trim and mark it as failed
        if (_use_timeouts) {
            pkt.free();
            return;
        } else {
            if (!pkt.header_only()) {
                pkt.strip_payload();
            }
            pkt.is_failed = true;
        }
    }
}

void failuregenerator::dropRandomPacket(Packet &pkt) {
    if (GLOBAL_TIME > stop_failures_after) {
        return;
    }
    uint32_t pkt_id = pkt.id();
    if (FAILURE_GENERATOR->randomDroppedPackets.find(pkt_id) != FAILURE_GENERATOR->randomDroppedPackets.end()) {
        UecSrc *src = FAILURE_GENERATOR->randomDroppedPackets[pkt_id];

        std::pair<std::pair<std::set<uint32_t>, std::set<uint32_t>>, std::string> switches_cables_on_path_string =
                get_path_switches_cables(pkt.pathid(), src);
        std::pair<std::set<uint32_t>, std::set<uint32_t>> switches_cables_on_path =
                switches_cables_on_path_string.first;
        std::string found_path = switches_cables_on_path_string.second;
        std::set<uint32_t> switches_on_path = switches_cables_on_path.first;
        std::set<uint32_t> cables_on_path = switches_cables_on_path.second;

        int numberOfDevices = switches_on_path.size() + cables_on_path.size();

        if (trueWithProb(1.0 / numberOfDevices)) {
            FAILURE_GENERATOR->randomDroppedPackets.erase(pkt_id);
            FAILURE_GENERATOR->nr_dropped_packets++;
            numberOfRandomDroppedPacketsTest++;
            FAILURE_GENERATOR->_list_random_packet_drops.push_back(GLOBAL_TIME);
            std::cout << "Packet dropped at Random Packet drop" << std::endl;

            // Return if using a timeout, otherwise transform it in a trim and mark it as failed
            if (_use_timeouts) {
                pkt.free();
                return;
            } else {
                if (!pkt.header_only()) {
                    pkt.strip_payload();
                }
                pkt.is_failed = true;
            }
        }
    }
}

bool failuregenerator::simSwitchFailures(Packet &pkt, Switch *sw, Queue q) {
    if (GLOBAL_TIME > stop_failures_after) {
        return false;
    }
    return (switchFail(sw) || switchBER(pkt, sw, q) || switchDegradation(sw) || switchPeriodicDrop(sw));
}

bool failuregenerator::fail_new_switch(Switch *sw) {
    if (pause_fail_switch || GLOBAL_TIME > stop_failures_after) {
        return false;
    }

    uint32_t switch_id = sw->getUniqueID();
    std::string switch_name = sw->nodename();

    int numberOfFailingSwitches = failingSwitches.size();
    int numberOfAllSwitches = all_switches.size();
    float percent = (float)numberOfFailingSwitches / (float)numberOfAllSwitches;
    if (percent > switch_fail_max_percent) {
        std::cout << "Did not fail Switch, because of max-percent Switch-name: " << switch_name << std::endl;
        pause_fail_switch = true;
        return false;
    }

    if (neededSwitches.find(switch_id) != neededSwitches.end()) {
        // std::cout << "Did not fail critical Switch name: " << switch_name << std::endl;
        return false;
    }

    uint64_t failureTime = GLOBAL_TIME;
    uint64_t recoveryTime = GLOBAL_TIME + generateTimeSwitch();

    temp_failingSwitches = failingSwitches;
    temp_failingSwitches[switch_id] = std::make_pair(failureTime, recoveryTime);

    if (!check_connectivity()) {
        neededSwitches.insert(switch_id);
        std::cout << "Did not fail critical Switch name: " << switch_name << std::endl;
        return false;
    }
    failingSwitches[switch_id] = std::make_pair(failureTime, recoveryTime);

    switch_fail_next_fail = GLOBAL_TIME + switch_fail_period;

    _list_switch_failures.push_back(std::make_pair(failureTime, recoveryTime));
    std::cout << "Failed a new Switch name: " << switch_name << " at " << std::to_string(failureTime) << " for "
              << std::to_string((recoveryTime - failureTime) / 1e+12) << " seconds" << std::endl;

    return true;
}

bool failuregenerator::fail_new_periodic_switch(Switch *sw) {
    if (pause_switch_periodic_loss || GLOBAL_TIME > stop_failures_after) {
        return false;
    }

    uint32_t switch_id = sw->getUniqueID();
    std::string switch_name = sw->nodename();

    int numberOfFailingSwitches = periodicfailingSwitches.size();
    int numberOfAllSwitches = all_switches.size();
    float percent = (float)numberOfFailingSwitches / (float)numberOfAllSwitches;
    if (percent > switch_periodic_loss_max_percent) {
        std::cout << "Did not fail Switch, because of max-percent Switch-name: " << switch_name << std::endl;
        pause_switch_periodic_loss = true;
        return false;
    }

    if (neededSwitches.find(switch_id) != neededSwitches.end()) {
        return false;
    }

    uint64_t failureTime = GLOBAL_TIME;
    uint64_t recoveryTime = GLOBAL_TIME + generateTimeSwitch();

    temp_periodicfailingSwitches = periodicfailingSwitches;
    temp_periodicfailingSwitches[switch_id] = std::make_pair(failureTime, recoveryTime);

    if (!check_connectivity()) {
        neededSwitches.insert(switch_id);
        std::cout << "Did not fail critical Switch name: " << switch_name << std::endl;
        return false;
    }
    periodicfailingSwitches[switch_id] = std::make_pair(failureTime, recoveryTime);
    SwitchToNrDrops[switch_id] = 1;
    SwitchToNextDropTime[switch_id] = 0;

    switch_periodic_loss_next_fail = GLOBAL_TIME + switch_periodic_loss_period;

    std::cout << "Periodically failed a new Switch name: " << switch_name << " at " << std::to_string(failureTime)
              << " for " << std::to_string((recoveryTime - failureTime) / 1e+12) << " seconds" << std::endl;

    return true;
}

bool failuregenerator::switchFail(Switch *sw) {
    if (!switch_fail || GLOBAL_TIME > stop_failures_after) {
        return false;
    }
    uint32_t switch_id = sw->getUniqueID();
    std::string switch_name = sw->nodename();

    if (failingSwitches.find(switch_id) != failingSwitches.end()) {
        std::pair<uint64_t, uint64_t> curSwitch = failingSwitches[switch_id];
        uint64_t recoveryTime = curSwitch.second;

        if (GLOBAL_TIME > recoveryTime) {
            std::cout << "Recovered from Fail" << std::endl;
            neededSwitches.clear();
            pause_fail_switch = false;
            failingSwitches.erase(switch_id);
            return false;
        } else {
            std::cout << "Packet dropped at SwitchFail " << switch_name << std::endl;
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

    if (!switch_ber || GLOBAL_TIME > stop_failures_after) {
        return false;
    }

    FAILURE_GENERATOR->all_packets_switchBER++;

    uint32_t pkt_id = pkt.id();

    if (GLOBAL_TIME < switch_ber_next_fail) {
        return false;
    } else {
        float percent =
                (float)FAILURE_GENERATOR->num_corrupted_packets_switchBER / FAILURE_GENERATOR->all_packets_switchBER;

        if (percent > switch_ber_max_percent) {
            // std::cout << "Did not corrupt packet at SwitchBER, because of max-percent" << std::endl;
            return false;
        }
        if (corrupted_packets.find(pkt_id) == corrupted_packets.end()) {
            std::cout << "Added new corrupted packet at SwitchBER" << std::endl;
            FAILURE_GENERATOR->num_corrupted_packets_switchBER++;
            corrupted_packets.insert(pkt_id);
            switch_ber_next_fail = GLOBAL_TIME + switch_ber_period;
        }
        return false;
    }
}

bool failuregenerator::dropPacketsSwitchBER(Packet &pkt) {
    if (GLOBAL_TIME > stop_failures_after) {
        return false;
    }
    uint32_t pkt_id = pkt.id();
    if (corrupted_packets.find(pkt_id) != corrupted_packets.end()) {
        corrupted_packets.erase(pkt_id);
        std::cout << "Packet dropped at SwitchBER" << std::endl;
        FAILURE_GENERATOR->nr_dropped_packets++;
        FAILURE_GENERATOR->_list_switch_packet_drops.push_back(GLOBAL_TIME);
        return true;
    }
    return false;
}

bool failuregenerator::switchDegradation(Switch *sw) {

    if (!switch_degradation || GLOBAL_TIME > stop_failures_after) {
        return false;
    }
    uint32_t switch_id = sw->getUniqueID();

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
            int numberOfDegradedSwitches = degraded_switches.size();
            int numberOfAllSwitches = all_switches.size();
            float percent = (float)numberOfDegradedSwitches / numberOfAllSwitches;
            if (percent > switch_degradation_max_percent) {
                std::cout << "Did not degrade Switch, because of max-percent Switch-name: " << sw->nodename()
                          << std::endl;
                return false;
            }
            if (needed_degraded_switches.find(switch_id) != needed_degraded_switches.end()) {
                // std::cout << "Did not degrade critical Switch name: " << sw->nodename() << std::endl;
                return false;
            }

            temp_degraded_switches = degraded_switches;
            temp_degraded_switches.insert(switch_id);

            if (!check_connectivity()) {
                needed_degraded_switches.insert(switch_id);
                std::cout << "Did not degrade critical Switch name: " << sw->nodename() << std::endl;
                return false;
            }

            int port_nrs = sw->portCount();
            for (int i = 0; i < port_nrs; i++) {
                BaseQueue *q = sw->getPort(i);
                linkspeed_bps speed = q->_bitrate;
                q->update_bit_rate(speed / 10);
            }
            switch_degradation_next_fail = GLOBAL_TIME + switch_degradation_period;
            _list_switch_degradations.push_back(GLOBAL_TIME);
            std::cout << "New Switch degraded at:" << GLOBAL_TIME << "Switch_name: " << sw->nodename()
                      << " bitrate now 1000bps" << std::endl;
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

bool failuregenerator::switchPeriodicDrop(Switch *sw) {

    if (!switch_periodic_loss || GLOBAL_TIME > stop_failures_after) {
        return false;
    }

    uint32_t switch_id = sw->getUniqueID();
    std::string switch_name = sw->nodename();

    if (periodicfailingSwitches.find(switch_id) != periodicfailingSwitches.end()) {
        std::pair<uint64_t, uint64_t> curSwitch = periodicfailingSwitches[switch_id];
        uint64_t recoveryTime = curSwitch.second;

        if (GLOBAL_TIME > recoveryTime) {
            std::cout << "Recovered from Fail" << std::endl;
            neededSwitches.clear();
            pause_switch_periodic_loss = false;
            periodicfailingSwitches.erase(switch_id);
            return false;
        } else {
            uint32_t nextDropTime = SwitchToNextDropTime[switch_id];
            if (nextDropTime < switch_periodic_loss_drop_period) {
                SwitchToNextDropTime[switch_id] = nextDropTime + 1;
                return false;
            }
            uint32_t nrDrops = SwitchToNrDrops[switch_id];
            SwitchToNrDrops[switch_id] = nrDrops + 1;
            if (nrDrops + 1 >= switch_periodic_loss_pkt_amount) {
                SwitchToNrDrops[switch_id] = 0;
                SwitchToNextDropTime[switch_id] = 0;
                std::cout << "Packet dropped at switchPeriodicDrop " << switch_name << std::endl;
                return true;
            }

            std::cout << "Packet dropped at switchPeriodicDrop " << switch_name << std::endl;
            return true;
        }
    } else {
        if (GLOBAL_TIME < switch_periodic_loss_next_fail) {
            return false;
        }

        return fail_new_periodic_switch(sw);
    }
}

bool failuregenerator::simCableFailures(Pipe *p, Packet &pkt) {
    if (GLOBAL_TIME > stop_failures_after) {
        return false;
    }
    return (cableFail(p, pkt) || cableBER(pkt) || cableDegradation(p, pkt) || cablePeriodicDrop(p, pkt));
}

bool failuregenerator::fail_new_cable(Pipe *p) {
    if (pause_fail_cable || GLOBAL_TIME > stop_failures_after) {
        return false;
    }

    uint32_t cable_id = p->getID();
    std::string cable_name = p->nodename();

    if (cable_name.find("callbackpipe") != std::string::npos) {
        return false;
    }

    int numberOfFailingCables = failingCables.size();
    int numberOfAllCables = all_cables.size();
    std::set<uint32_t> failingCablesIDs;
    for (const auto &pair : failingCables) {
        failingCablesIDs.insert(pair.first);
    }

    if (cable_fail_per_switch) {
        for (const auto &pair : CablesPerSwitch) {
            const std::set<uint32_t> &cables = pair.second;
            if (cables.find(cable_id) == cables.end()) {
                continue;
            }
            std::set<uint32_t> intersection;
            std::set_intersection(cables.begin(), cables.end(), failingCablesIDs.begin(), failingCablesIDs.end(),
                                  std::inserter(intersection, intersection.begin()));
            int numberOfCablesSwitch = cables.size();
            int numberOfFailingCablesSwitch = intersection.size();
            float percent = (float)numberOfFailingCablesSwitch / numberOfCablesSwitch;
            if (percent > cable_fail_max_percent) {
                return false;
            }
        }

    } else {
        float percent = (float)numberOfFailingCables / numberOfAllCables;
        if (percent > cable_fail_max_percent) {
            std::cout << "Did not fail Cable, because of max-percent Cable-name: " << p->nodename() << std::endl;
            pause_fail_cable = true;
            return false;
        }
    }

    if (neededCables.find(cable_id) != neededCables.end()) {
        // std::cout << "Did not fail critical Cable name: " << cable_name << std::endl;
        return false;
    }

    uint64_t failureTime = GLOBAL_TIME;
    uint64_t recoveryTime = GLOBAL_TIME + generateTimeCable();

    temp_failingCables = failingCables;
    temp_failingCables[cable_id] = std::make_pair(failureTime, recoveryTime);

    if (!check_connectivity()) {
        neededCables.insert(cable_id);
        std::cout << "Did not fail critical Cable name: " << cable_name << std::endl;
        return false;
    }

    failingCables[cable_id] = std::make_pair(failureTime, recoveryTime);

    cable_fail_next_fail = GLOBAL_TIME + cable_fail_period;

    _list_cable_failures.push_back(std::make_pair(failureTime, recoveryTime));
    std::cout << "Failed a new Cable name: " << cable_name << " at " << std::to_string(failureTime) << " for "
              << std::to_string((recoveryTime - failureTime) / 1e+12) << " seconds" << std::endl;
    return true;
}

bool failuregenerator::fail_new_periodic_cable(Pipe *p) {
    if (pause_cable_periodic_loss || GLOBAL_TIME > stop_failures_after) {
        return false;
    }

    uint32_t cable_id = p->getID();
    std::string cable_name = p->nodename();

    if (cable_name.find("callbackpipe") != std::string::npos) {
        return false;
    }

    int numberOfFailingCables = periodicFailingCables.size();
    int numberOfAllCables = all_cables.size();

    float percent = (float)numberOfFailingCables / numberOfAllCables;
    if (percent > cable_periodic_loss_max_percent) {
        std::cout << "Did not fail Cable, because of max-percent Cable-name: " << p->nodename() << std::endl;
        pause_cable_periodic_loss = true;
        return false;
    }

    if (neededCables.find(cable_id) != neededCables.end()) {
        // std::cout << "Did not fail critical Cable name: " << cable_name << std::endl;
        return false;
    }

    uint64_t failureTime = GLOBAL_TIME;
    uint64_t recoveryTime = GLOBAL_TIME + generateTimeCable();

    temp_periodicFailingCables = periodicFailingCables;
    temp_periodicFailingCables[cable_id] = std::make_pair(failureTime, recoveryTime);

    if (!check_connectivity()) {
        neededCables.insert(cable_id);
        std::cout << "Did not fail critical Cable name: " << cable_name << std::endl;
        return false;
    }

    periodicFailingCables[cable_id] = std::make_pair(failureTime, recoveryTime);
    CableToNrDrops[cable_id] = 1;
    CableToNextDropTime[cable_id] = 0;

    cable_periodic_loss_next_fail = GLOBAL_TIME + cable_periodic_loss_period;

    std::cout << "Periodically failed a new Cable name: " << cable_name << " at " << std::to_string(failureTime)
              << " for " << std::to_string((recoveryTime - failureTime) / 1e+12) << " seconds" << std::endl;
    return true;
}

bool checkUStoCS(const std::string &str) {
    size_t foundUS = str.find("US");
    size_t foundCS = str.find("CS");
    return (foundUS != std::string::npos) && (foundCS != std::string::npos);
}

bool failuregenerator::cableFail(Pipe *p, Packet &pkt) {
    if (!cable_fail || GLOBAL_TIME > stop_failures_after) {
        return false;
    }

    if (only_us_cs) {
        const Route *route = pkt.get_route();
        PacketSink *sink = route->at(0);
        string name = sink->nodename();
        if (!checkUStoCS(name)) {
            // std::cout << "Did not fail Cable, because of only_us_cs: " << name << std::endl;
            return false;
        }
    }

    uint32_t cable_id = p->getID();
    if (failingCables.find(cable_id) != failingCables.end()) {
        std::pair<uint64_t, uint64_t> curCable = failingCables[cable_id];
        uint64_t recoveryTime = curCable.second;

        if (GLOBAL_TIME > recoveryTime) {
            std::cout << "Recovered from Fail" << std::endl;
            neededCables.clear();
            pause_fail_cable = false;
            failingCables.erase(cable_id);
            return false;
        } else {
            std::cout << "Packet dropped at CableFail" << std::endl;
            return true;
        }
    } else {

        if (GLOBAL_TIME < cable_fail_next_fail) {
            return false;
        }

        return fail_new_cable(p);
    }
}

bool failuregenerator::cableBER(Packet &pkt) {
    if (!cable_ber || GLOBAL_TIME > stop_failures_after) {
        return false;
    }

    FAILURE_GENERATOR->all_packets_cableBER++;

    uint32_t pkt_id = pkt.id();
    std::string from_queue = pkt.route()->at(0)->nodename();

    if (GLOBAL_TIME < cable_ber_next_fail) {
        return false;
    } else {
        float percent =
                (float)FAILURE_GENERATOR->num_corrupted_packets_cableBER / FAILURE_GENERATOR->all_packets_cableBER;

        if (percent > cable_ber_max_percent) {
            std::cout << "Did not corrupt packet at CableBER, because of max-percent" << std::endl;
            return false;
        }
        num_corrupted_packets_cableBER++;
        corrupted_packets.insert(pkt_id);
        cable_ber_next_fail = GLOBAL_TIME + cable_ber_period;
        return false;
    }

    if (from_queue.find("DST") != std::string::npos) {
        if (corrupted_packets.find(pkt_id) != corrupted_packets.end()) {
            FAILURE_GENERATOR->nr_dropped_packets++;
            FAILURE_GENERATOR->_list_cable_packet_drops.push_back(GLOBAL_TIME);
            corrupted_packets.erase(pkt_id);
            std::cout << "Packet dropped at cableBER" << std::endl;
            return true;
        }
    }
}

bool failuregenerator::cableDegradation(Pipe *p, Packet &pkt) {
    if (!cable_degradation || GLOBAL_TIME > stop_failures_after) {
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
        if (GLOBAL_TIME < cable_degradation_next_fail) {
            return false;
        }
        int numberOfDegradedCables = degraded_cables.size();
        int numberOfAllCables = all_cables.size();
        float percent = (float)numberOfDegradedCables / numberOfAllCables;
        if (percent > cable_degradation_max_percent) {
            std::cout << "Did not degrade Cable, because of max-percent Cable-name: " << p->nodename() << std::endl;
            return false;
        }

        if (neededCables.find(cable_id) != neededCables.end()) {
            // std::cout << "Did not degrade critical Cable name: " << p->nodename() << std::endl;
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

        temp_degraded_cables = degraded_cables;
        temp_degraded_cables[cable_id] = decided_type;

        if (!check_connectivity()) {
            needed_degraded_cables.insert(cable_id);
            std::cout << "Did not degrade critical Cable name: " << p->nodename() << std::endl;
            return false;
        }
        p->_delay = SINGLE_PKT_TRASMISSION_TIME_MODERN * 10;
        cable_degradation_next_fail = GLOBAL_TIME + cable_degradation_period;
        degraded_cables[cable_id] = decided_type;
        _list_cable_degradations.push_back(GLOBAL_TIME);
        std::cout << "Degraded a new Cable name: " << p->nodename() << std::endl;
        return true;
    }
}

bool failuregenerator::cablePeriodicDrop(Pipe *p, Packet &pkt) {

    if (!cable_periodic_loss || GLOBAL_TIME > stop_failures_after) {
        return false;
    }

    if (only_us_cs) {
        const Route *route = pkt.get_route();
        PacketSink *sink = route->at(0);
        string name = sink->nodename();
        if (!checkUStoCS(name)) {
            // std::cout << "Did not fail Cable, because of only_us_cs: " << name << std::endl;
            return false;
        }
    }

    uint32_t cable_id = p->getID();

    if (periodicFailingCables.find(cable_id) != periodicFailingCables.end()) {
        std::pair<uint64_t, uint64_t> curCable = periodicFailingCables[cable_id];
        uint64_t recoveryTime = curCable.second;

        if (GLOBAL_TIME > recoveryTime) {
            std::cout << "Recovered from Fail" << std::endl;
            neededCables.clear();
            pause_cable_periodic_loss = false;
            periodicFailingCables.erase(cable_id);
            return false;
        } else {
            uint32_t nextDropTime = CableToNextDropTime[cable_id];
            if (nextDropTime < cable_periodic_loss_drop_period) {
                CableToNextDropTime[cable_id] = nextDropTime + 1;
                return false;
            }
            uint32_t nrDrops = CableToNrDrops[cable_id];
            CableToNrDrops[cable_id] = nrDrops + 1;
            if (nrDrops + 1 == cable_periodic_loss_pkt_amount) {
                CableToNrDrops[cable_id] = 0;
                CableToNextDropTime[cable_id] = 0;
                std::cout << "Packet dropped at periodicCableFail " << p->nodename() << std::endl;
                return true;
            }

            std::cout << "Packet dropped at periodicCableFail " << p->nodename() << std::endl;
            return true;
        }
    } else {

        if (GLOBAL_TIME < cable_periodic_loss_next_fail) {
            return false;
        }

        return fail_new_periodic_cable(p);
    }
}

bool failuregenerator::simNICFailures(UecSrc *src, UecSink *sink, Packet &pkt) {
    if (GLOBAL_TIME > stop_failures_after) {
        return false;
    }
    return (nicFail(src, sink, pkt) || nicDegradation(src, sink, pkt));
}

bool failuregenerator::fail_new_nic(u_int32_t nic_id) {
    if (GLOBAL_TIME > stop_failures_after) {
        return false;
    }

    int numberOfFailingNICs = failingNICs.size();
    int numberOfAllNICs = all_srcs.size() + all_sinks.size();
    float percent = (float)numberOfFailingNICs / numberOfAllNICs;
    if (percent > nic_fail_max_percent) {
        std::cout << "Did not fail NIC, because of max-percent NIC-name: " << nic_id << std::endl;
        return false;
    }

    uint64_t failureTime = GLOBAL_TIME;
    uint64_t recoveryTime = GLOBAL_TIME + generateTimeNIC();
    nic_fail_next_fail = GLOBAL_TIME + nic_fail_period;
    failingNICs[nic_id] = std::make_pair(failureTime, recoveryTime);
    std::cout << "Failed a new NIC name: " << nic_id << " at " << std::to_string(failureTime) << " for "
              << std::to_string((recoveryTime - failureTime) / 1e+12) << " seconds" << std::endl;
    return true;
}

bool failuregenerator::nicFail(UecSrc *src, UecSink *sink, Packet &pkt) {

    if (!nic_fail || GLOBAL_TIME > stop_failures_after) {
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
    if (!nic_degradation || GLOBAL_TIME > stop_failures_after) {
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
            int numberOfDegradedNICs = degraded_NICs.size();
            int numberOfAllNICs = all_srcs.size() + all_sinks.size();
            float percent = (float)numberOfDegradedNICs / numberOfAllNICs;
            if (percent > nic_degradation_max_percent) {
                std::cout << "Did not degrade NIC, because of max-percent NIC-name: " << nic_id << std::endl;
                return false;
            }
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

void failuregenerator::addCablePerSwitch(uint32_t switch_id, uint32_t cable_id) {
    if (CablesPerSwitch.find(switch_id) == CablesPerSwitch.end()) {
        std::set<uint32_t> cables;
        cables.insert(cable_id);
        CablesPerSwitch[switch_id] = cables;
    } else {
        CablesPerSwitch[switch_id].insert(cable_id);
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

        if (next_queue->getSwitch() != nullptr) {
            addCablePerSwitch(next_queue->getSwitch()->getUniqueID(), next_pipe->getID());
        }

        if (next_switch == nullptr) {
            if (next_queue) {
                path += " -> " + next_queue->nodename();
            } else {
                std::cout << "Error: last next_queue is nullptr" << std::endl;
            }
            if (next_pipe) {
                path += " -> " + next_pipe->nodename();
                cables.insert(next_pipe->getID());
                FAILURE_GENERATOR->all_cables.insert(next_pipe->getID());
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
            FAILURE_GENERATOR->all_cables.insert(next_pipe->getID());
        } else {
            std::cout << "Error: next_pipe is nullptr" << std::endl;
        }
        if (next_switch) {
            path += " -> " + next_switch->nodename();
            switches.insert(next_switch->getUniqueID());
            FAILURE_GENERATOR->all_switches.insert(next_switch->getUniqueID());

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
                if (temp_failingSwitches.find(sw) != temp_failingSwitches.end() ||
                    temp_degraded_switches.find(sw) != temp_degraded_switches.end() ||
                    temp_periodicfailingSwitches.find(sw) != temp_periodicfailingSwitches.end()) {
                    all_switches_active = false;
                    break;
                }
            }

            for (uint32_t cable : cables_on_path) {

                if (temp_failingCables.find(cable) != temp_failingCables.end() ||
                    temp_degraded_cables.find(cable) != temp_degraded_cables.end() ||
                    temp_periodicFailingCables.find(cable) != temp_periodicFailingCables.end()) {
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
            } else if (key == "Switch-Fail-Max-Percent:") {
                switch_fail_max_percent = std::stof(value);
            } else if (key == "Switch-BER-Max-Percent:") {
                switch_ber_max_percent = std::stof(value);
            } else if (key == "Switch-Degradation-Max-Percent:") {
                switch_degradation_max_percent = std::stof(value);
            } else if (key == "Cable-Fail-Max-Percent:") {
                cable_fail_max_percent = std::stof(value);
            } else if (key == "Cable-BER-Max-Percent:") {
                cable_ber_max_percent = std::stof(value);
            } else if (key == "Cable-Degradation-Max-Percent:") {
                cable_degradation_max_percent = std::stof(value);
            } else if (key == "NIC-Fail-Max-Percent:") {
                nic_fail_max_percent = std::stof(value);
            } else if (key == "NIC-Degradation-Max-Percent:") {
                nic_degradation_max_percent = std::stof(value);
            } else if (key == "Random-Packet-Drop:") {
                randomPacketDrop = (value == "ON");
            } else if (key == "Random-Packet-Drop-Rate:") {
                dropRate = std::stof(value);
            } else if (key == "Only-US-CS-Cable:") {
                only_us_cs = (value == "ON");
            } else if (key == "Cable-Fail-Percent-Per-Switch:") {
                cable_fail_per_switch = (value == "ON");
            } else if (key == "Switch-Fail-Duration:") {
                switch_fail_duration = (value == "ON");
            } else if (key == "Switch-Fail-Duration-Time:") {
                switch_fail_duration_time = std::stof(value);
            } else if (key == "Cable-Fail-Duration:") {
                cable_fail_duration = (value == "ON");
            } else if (key == "Cable-Fail-Duration-Time:") {
                cable_fail_duration_time = std::stof(value);
            } else if (key == "Switch-Periodic-Packet-Loss:") {
                switch_periodic_loss = (value == "ON");
            } else if (key == "Switch-Periodic-Packet-Loss-Start-After:") {
                switch_periodic_loss_start = std::stof(value);
                switch_periodic_loss_next_fail = switch_periodic_loss_start;
            } else if (key == "Switch-Periodic-Packet-Loss-Period:") {
                switch_periodic_loss_period = std::stof(value);
            } else if (key == "Switch-Periodic-Packet-Loss-PktNr:") {
                switch_periodic_loss_pkt_amount = std::stof(value);
            } else if (key == "Switch-Periodic-Packet-Loss-Drop-Period:") {
                switch_periodic_loss_drop_period = std::stof(value);
            } else if (key == "Switch-Periodic-Packet-Loss-Max-Percent:") {
                switch_periodic_loss_max_percent = std::stof(value);
            } else if (key == "Cable-Periodic-Packet-Loss:") {
                cable_periodic_loss = (value == "ON");
            } else if (key == "Cable-Periodic-Packet-Loss-Start-After:") {
                cable_periodic_loss_start = std::stof(value);
                cable_periodic_loss_next_fail = cable_periodic_loss_start;
            } else if (key == "Cable-Periodic-Packet-Loss-Period:") {
                cable_periodic_loss_period = std::stof(value);
            } else if (key == "Cable-Periodic-Packet-Loss-PktNr:") {
                cable_periodic_loss_pkt_amount = std::stof(value);
            } else if (key == "Cable-Periodic-Packet-Loss-Drop-Period:") {
                cable_periodic_loss_drop_period = std::stof(value);
            } else if (key == "Cable-Periodic-Packet-Loss-Max-Percent:") {
                cable_periodic_loss_max_percent = std::stof(value);
            } else if (key == "Stop-Failures-After:") {
                stop_failures_after = std::stof(value);
            }

            else {
                std::cout << "Unknown key in failuregenerator input file: " << key << std::endl;
            }
        }
        inputFile.close();
    } else {
        std::cout << "Could not open failuregenerator_input file, all failures are turned off" << std::endl;
    }
}

// Generate logging data

void failuregenerator::createLoggingData() {

    std::cout << "Total number of dropped packets: " << nr_dropped_packets << std::endl;

    // Switch Packet drops
    string file_name = PROJECT_ROOT_PATH / ("sim/output/switch_drops/switch_drops.txt");
    std::ofstream MyFileSwitchDrops(file_name, std::ios_base::app);

    for (const auto p : _list_switch_packet_drops) {
        MyFileSwitchDrops << p << std::endl;
    }

    MyFileSwitchDrops.close();

    // Cable Packet drops
    file_name = PROJECT_ROOT_PATH / ("sim/output/cable_drops/cable_drops.txt");
    std::ofstream MyFileCableDrops(file_name, std::ios_base::app);

    for (const auto p : _list_cable_packet_drops) {
        MyFileCableDrops << p << std::endl;
    }
    MyFileCableDrops.close();

    // Switch Failures
    file_name = PROJECT_ROOT_PATH / ("sim/output/switch_failures/switch_failures.txt");
    std::ofstream MyFileSwitchFailures(file_name, std::ios_base::app);

    for (const auto &p : _list_switch_failures) {
        MyFileSwitchFailures << p.first << "," << p.second << std::endl;
    }
    MyFileSwitchFailures.close();

    // Cable Failures
    file_name = PROJECT_ROOT_PATH / ("sim/output/cable_failures/cable_failures.txt");
    std::ofstream MyFileCableFailures(file_name, std::ios_base::app);

    for (const auto &p : _list_cable_failures) {
        MyFileCableFailures << p.first << "," << p.second << std::endl;
    }
    MyFileCableFailures.close();

    // Routing Failed Switch
    file_name = PROJECT_ROOT_PATH / ("sim/output/routing_failed_switch/routing_failed_switch.txt");
    std::ofstream MyFileRoutingFailedSwitch(file_name, std::ios_base::app);
    for (const auto p : _list_routed_failing_switches) {
        MyFileRoutingFailedSwitch << p << std::endl;
    }
    MyFileRoutingFailedSwitch.close();

    // Routing Failed Cable
    file_name = PROJECT_ROOT_PATH / ("sim/output/routing_failed_cable/routing_failed_cable.txt");
    std::ofstream MyFileRoutingFailedCable(file_name, std::ios_base::app);
    for (const auto p : _list_routed_failing_cables) {
        MyFileRoutingFailedCable << p << std::endl;
    }
    MyFileRoutingFailedCable.close();

    // Switch Degradations
    file_name = PROJECT_ROOT_PATH / ("sim/output/switch_degradations/switch_degradations.txt");
    std::ofstream MyFileSwitchDegradations(file_name, std::ios_base::app);
    for (const auto p : _list_switch_degradations) {
        MyFileSwitchDegradations << p << std::endl;
    }
    MyFileSwitchDegradations.close();

    // Cable Degradations
    file_name = PROJECT_ROOT_PATH / ("sim/output/cable_degradations/cable_degradations.txt");
    std::ofstream MyFileCableDegradations(file_name, std::ios_base::app);
    for (const auto p : _list_cable_degradations) {
        MyFileCableDegradations << p << std::endl;
    }
    MyFileCableDegradations.close();

    // Random Packet Drops
    file_name = PROJECT_ROOT_PATH / ("sim/output/random_packet_drops/random_packet_drops.txt");
    std::ofstream MyFileRandomPacketDrops(file_name, std::ios_base::app);
    for (const auto p : _list_random_packet_drops) {
        MyFileRandomPacketDrops << p << std::endl;
    }

    for (const auto &pair : map_list_packet_seq) {
        UecSink *key = pair.first;
        const std::vector<uint64_t> &values = pair.second;

        file_name = PROJECT_ROOT_PATH /
                    ("sim/output/packet_seq/packet_seq" + key->_name + "_" + std::to_string(key->tag) + ".txt");
        std::ofstream MyFilePacketSeq(file_name, std::ios_base::app);

        for (const auto &p : values) {
            MyFilePacketSeq << p / PKT_SIZE_MODERN << std::endl;
        }
        MyFilePacketSeq.close();
    }
}
