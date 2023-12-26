// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "compute_event.h"
#include "ecn.h"
#include "queue.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <math.h>
#include <regex>
#include <stdio.h>
#include <utility>

#define timeInf 0

ComputeEvent::ComputeEvent(UecLogger *logger, EventList &eventList)
        : EventSource(eventList, "compute_event") {} // Fix the name

void ComputeEvent::doNextEvent() {
    printf("ComputeEvent at %lu\n", GLOBAL_TIME);
    fflush(stdout);
    if (f_compute_over_hook) {
        f_compute_over_hook(1);
    }

    return;
}

void ComputeEvent::setCompute(simtime_picosec computation_time) {
    eventlist().sourceIsPendingRel(*this, computation_time * 1000); // ns to ps
    eventlist().doNextEvent();
}
