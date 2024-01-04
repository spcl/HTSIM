#include "uec.h"

GenericPacer::GenericPacer(EventList &event_list, UecSrc &flow)
        : EventSource(event_list, "generic_pacer"), flow(&flow),
          _interpacket_delay(0) {
    _last_send = eventlist().now();
}

void GenericPacer::schedule_send(simtime_picosec delay) {
    _interpacket_delay = delay;
    _next_send = _last_send + _interpacket_delay;
    // printf("Scheduling Send Pacer - Time now %lu - Next Sent %lu\n",
    //        eventlist().now(), _next_send);
    if (_next_send <= eventlist().now()) {
        _next_send = eventlist().now();
        doNextEvent();
        return;
    }
    eventlist().sourceIsPending(*this, _next_send);
}

void GenericPacer::cancel() {
    _interpacket_delay = 0;
    _next_send = 0;
    eventlist().cancelPendingSource(*this);
}

void GenericPacer::just_sent() { _last_send = eventlist().now(); }

void GenericPacer::doNextEvent() {
    assert(eventlist().now() == _next_send);
    flow->send_paced();

    _last_send = eventlist().now();

    if (flow->pacing_delay_f() > 0) {
        schedule_send(flow->pacing_delay_f());
    } else {
        _interpacket_delay = 0;
        _next_send = 0;
    }
}