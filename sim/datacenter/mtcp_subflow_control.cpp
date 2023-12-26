#include "mtcp_subflow_control.h"
#include <iostream>
#include <math.h>
////////////////////////////////////////////////////////////////
//  Multipath TCP Subflow Control
////////////////////////////////////////////////////////////////

#define ABS(x) ((x) >= 0 ? (x) : -(x))

MultipathTcpSubCtrl::MultipathTcpSubCtrl(char cc_type, EventList &ev, MultipathTcpLogger *logger, double epsilon)
        : MultipathTcpSrc(cc_type, ev, logger, epsilon) {}

// every delta ms do something nice!
void MultipathTcpSubCtrl::doNextEvent() {}
