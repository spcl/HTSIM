// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef FAILURE_GENERATOR_H
#define FAILURE_GENERATOR_H

#include "network.h"

class failuregenerator {

  public:
    bool drop_pkt(Packet &pkt);

  private:
};

#endif
