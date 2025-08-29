// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "pciemodel.h"
#include "uec.h"

mem_b PCIeModel::_max_pcie_backlog = 5000000;
mem_b PCIeModel::_min_threshold = 1000000;

static unsigned pktByteTimes(unsigned size) {
    // IPG (96 bit times) + preamble + SFD + ether header + FCS = 38B
    return max(size, 46u) + 38;
}

// pull rate modifier should generally be something like 0.99 so we pull at just less than line rate
PCIeModel::PCIeModel(linkspeed_bps linkSpeed,
                     uint16_t mtu,
                     EventList& eventList,
                     UecPullPacer* pacer)
    : EventSource(eventList, "PCIeModel"), _pktTime(8 * pktByteTimes(mtu) * 1e12 / (linkSpeed)) {
    _active = false;
    _actualPktTime = _pktTime;
    _mtu = mtu;

    _pcie_rate = 1;
    _pullPacer = pacer;

    cout << "PCIe serialization time: " << timeAsUs(_actualPktTime) << endl;
}

void PCIeModel::doNextEvent() {
    _backlog -= _mtu;

    if (_backlog < 0)
        _backlog = 0;

    adjustCreditRate();

    if (_backlog == 0) {
        _active = false;
    } else {
        _active = true;
        eventlist().sourceIsPendingRel(*this, _actualPktTime);
    }
}

void PCIeModel::setPCIeRate(double relative_rate) {
    _actualPktTime = _pktTime / relative_rate;
}

void PCIeModel::adjustCreditRate() {
    // modulate PCIe bandwidth based on backlog.
    if (pcieBacklog() < _min_threshold)
        _pcie_rate = 1;
    else if (pcieBacklog() > _max_pcie_backlog * 95 / 100)
        _pcie_rate = 0.001;
    else {
        /*
        //linear model below
        mem_b delta = _max_pcie_backlog - pcieBacklog();
        _pcie_rate = (double)delta / (_max_pcie_backlog - _src->configuredMaxWnd());
        */
        // quadratic model, should scale better.
        mem_b gap = _max_pcie_backlog - _min_threshold;
        double crt = pcieBacklog();

        _pcie_rate =
            (crt * crt - 2 * _max_pcie_backlog * crt + _max_pcie_backlog * _max_pcie_backlog) /
            (gap * gap);
    }

    if (UecSrc::_debug)
        cout << "At " << timeAsUs(eventlist().now()) << " PCIe backlog " << _backlog << " rate "
             << _pcie_rate << endl;

    _pullPacer->updatePullRate(UecPullPacer::PCIE, _pcie_rate);
}

bool PCIeModel::addBacklog(mem_b backlog) {
    if (_backlog + backlog > _max_pcie_backlog)
        return false;

    _backlog += backlog;

    adjustCreditRate();

    if (!_active) {
        eventlist().sourceIsPendingRel(*this, _actualPktTime);
        _active = true;
    }
    return true;
}
