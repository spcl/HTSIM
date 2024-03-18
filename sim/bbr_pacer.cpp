#include "bbr.h"

BBRPacer::BBRPacer(EventList &event_list, BBRSrc &flow)
        : EventSource(event_list, "generic_pacer"), flow(&flow),
          _interpacket_delay(0) {
    _last_send = eventlist().now();
}

void BBRPacer::schedule_send(simtime_picosec delay) {
    _interpacket_delay = delay;
    _next_send = _last_send + _interpacket_delay;
    /*printf("Scheduling Send Pacer - Time now %lu - Next Sent %lu\n",
           eventlist().now(), _next_send);*/
    if (_next_send <= eventlist().now()) {
        _next_send = eventlist().now();
        doNextEvent();
        return;
    }
    eventlist().sourceIsPending(*this, _next_send);
}

void BBRPacer::cancel() {
    _interpacket_delay = 0;
    _next_send = 0;
    eventlist().cancelPendingSource(*this);
}

void BBRPacer::just_sent() { _last_send = eventlist().now(); }

void BBRPacer::doNextEvent() {
    assert(eventlist().now() == _next_send);
    // printf("Pacer DoNextEvent\n");
    flow->pacedSend();

    _last_send = eventlist().now();

    if (_interpacket_delay > 0) {
        schedule_send(_interpacket_delay);
    } else {
        _interpacket_delay = 0;
        _next_send = 0;
    }
}