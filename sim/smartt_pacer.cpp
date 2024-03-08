#include "uec.h"

SmarttPacer::SmarttPacer(EventList &event_list, UecSrc &flow)
        : EventSource(event_list, "generic_pacer"), flow(&flow),
          _interpacket_delay(0) {
    _last_send = eventlist().now();
}

void SmarttPacer::schedule_send(simtime_picosec delay) {
    _interpacket_delay = delay;
    _next_send = _last_send + _interpacket_delay;
    if (_next_send <= eventlist().now()) {
        _next_send = eventlist().now();
        doNextEvent();
        return;
    }
    eventlist().sourceIsPending(*this, _next_send);
}

void SmarttPacer::cancel() {
    _interpacket_delay = 0;
    _next_send = 0;
    eventlist().cancelPendingSource(*this);
}

void SmarttPacer::just_sent() { _last_send = eventlist().now(); }

void SmarttPacer::doNextEvent() {
    assert(eventlist().now() == _next_send);
    flow->pacedSend();

    _last_send = eventlist().now();

    if (_interpacket_delay > 0) {
        schedule_send(_interpacket_delay);
    } else {
        _interpacket_delay = 0;
        _next_send = 0;
    }
}