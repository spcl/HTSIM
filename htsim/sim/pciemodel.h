// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef PCIEMODEL_H
#define PCIEMODEL_H

#include <list>
#include <memory>
#include <tuple>

#include "circular_buffer.h"
#include "eventlist.h"
#include "modular_vector.h"
#include "trigger.h"
#include "uecpacket.h"

class UecPullPacer;

class PCIeModel : public EventSource {
public:
    PCIeModel(linkspeed_bps linkSpeed, uint16_t mtu, EventList& eventList, UecPullPacer* pacer);

    void doNextEvent();

    bool addBacklog(mem_b backlog);
    mem_b pcieBacklog() const { return _backlog; };

    void setPCIeRate(double relative_rate);

    simtime_picosec packettime() const { return _actualPktTime; }

    void adjustCreditRate();

    static mem_b _max_pcie_backlog;
    static mem_b _min_threshold;

private:
    const simtime_picosec _pktTime;
    simtime_picosec _actualPktTime;
    uint16_t _mtu;

    mem_b _backlog = 0;
    bool _active = false;

    UecPullPacer* _pullPacer = NULL;

    double _pcie_rate;  // credit rate as dictated by PCIe backlog, computed dynamically.
};

#endif
