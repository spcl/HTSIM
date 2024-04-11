// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-

#include "failuregenerator.h"
#include "network.h"
#include <random>

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

bool failuregenerator::drop_pkt(Packet &pkt) {
    bool drop_decision = trueWithProb(0.05);
    if (drop_decision) {
        std::cout << "Packet dropped. Now dropped" << std::endl;
    }
    return drop_decision;
}
