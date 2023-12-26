// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef MTCP_SUB_CTRL_H
#define MTCP_SUB_CTRL_H

#include "config.h"
#include "eventlist.h"
#include "mtcp.h"
#include "network.h"
#include "tcp.h"
#include <list>
#include <math.h>

class MultipathTcpSubCtrl : public MultipathTcpSrc {
  public:
    MultipathTcpSubCtrl(char cc_type, EventList &ev, MultipathTcpLogger *logger, double epsilon = 0.1);

    // run as normal. Every timestep check to see if a subflow needs adding,
    // deleting, etc.
    void doNextEvent();
};

#endif
