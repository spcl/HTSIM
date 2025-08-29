// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        

#include "eventlist.h"
#include "trigger.h"

simtime_picosec EventList::_endtime = 0;
simtime_picosec EventList::_lasteventtime = 0;
int EventList::_trafficeventcount = 0;
EventList::pendingsources_t EventList::_pendingsources;
vector <TriggerTarget*> EventList::_pending_triggers;
int EventList::_instanceCount = 0;
EventList* EventList::_theEventList = nullptr;

EventList::EventList()
{
    if (EventList::_instanceCount != 0) 
    {
        std::cerr << "There should be only one instance of EventList. Abort." << std::endl;
        abort();
    }

    EventList::_theEventList = this;
    EventList::_instanceCount += 1;
}

EventList& 
EventList::getTheEventList()
{
    if (EventList::_theEventList == nullptr) 
    {
        EventList::_theEventList = new EventList();
    }
    return *EventList::_theEventList;
}

void
EventList::setEndtime(simtime_picosec endtime)
{
    EventList::_endtime = endtime;
}

bool
EventList::doNextEvent() 
{
    // triggers happen immediately - no time passes; no guarantee that
    // they happen in any particular order (don't assume FIFO or LIFO).
    if (!_pending_triggers.empty()) {
        TriggerTarget *target = _pending_triggers.back();
        _pending_triggers.pop_back();
        target->activate();
        return true;
    }
    
    if (_pendingsources.empty())
        return false;
    
    pendingsources_t::iterator i = _pendingsources.begin();
    simtime_picosec nexteventtime = i->first;
    EventSource* nextsource = i->second;
    if (nextsource->isTraffic()) {
        _trafficeventcount--;
    } 
    _pendingsources.erase(i);
    assert(nexteventtime >= _lasteventtime);
    _lasteventtime = nexteventtime; // set this before calling doNextEvent, so that this::now() is accurate
    nextsource->doNextEvent();
    return true;
}


/* void 
EventList::sourceIsPending(EventSource &src, simtime_picosec when) 
{
    assert(when>=now());
    if ((_endtime==0 || when<_endtime) && (src.isTraffic() ||
        (_trafficeventcount > 0 || _lasteventtime == 0))) {
        _pendingsources.insert(make_pair(when,&src));
        if (src.isTraffic()) {
            _trafficeventcount++;
        }
    }
} */

void EventList::sourceIsPending(EventSource& src, simtime_picosec when) {
    /* printf("EventList::sourceIsPending: source %s at time %lu ps -- %lu %lu %lu\n",
               src.str().c_str(), static_cast<unsigned long>(when), _endtime, when, _endtime); */
    assert(when >= now());
    if (_endtime == 0 || when < _endtime) {
        _pendingsources.insert(make_pair(when, &src));
        /* printf("EventList1::sourceIsPending: source %s at time %lu ps\n",
               src.str().c_str(), static_cast<unsigned long>(when)); */
    }
}

EventList::Handle
EventList::sourceIsPendingGetHandle(EventSource &src, simtime_picosec when) 
{
    assert(when>=now());
    if ((_endtime==0 || when<_endtime) && (src.isTraffic() ||
        (_trafficeventcount > 0 || _lasteventtime == 0))) {
        EventList::Handle handle =_pendingsources.insert(make_pair(when,&src));
        if (src.isTraffic()) {
            _trafficeventcount++;
        }
        return handle;
    }
    return _pendingsources.end();
}

void
EventList::triggerIsPending(TriggerTarget &target) {
    _pending_triggers.push_back(&target);
}

void 
EventList::cancelPendingSource(EventSource &src) {
    pendingsources_t::iterator i = _pendingsources.begin();
    while (i != _pendingsources.end()) {
        if (i->second == &src) {
            if (src.isTraffic()) {
                _trafficeventcount--;
            }
            _pendingsources.erase(i);
            return;
        }
        i++;
    }
}

void 
EventList::cancelPendingSourceByTime(EventSource &src, simtime_picosec when) {
    // fast cancellation of a timer - the timer MUST exist
    // this should normally be fast, except if we have a lot of events with exactly the same time value

    auto range = _pendingsources.equal_range(when);

    for (auto i = range.first; i != range.second; ++i) {
        if (i->second == &src) {
            if (src.isTraffic()) {
                _trafficeventcount--;
            }
            _pendingsources.erase(i);
            return;
        }
    }
    abort();
}


void EventList::cancelPendingSourceByHandle(EventSource &src, EventList::Handle handle) {
    // If we're cancelling timers often, cancel them by handle.  But
    // be careful - cancelling a handle that has already been
    // cancelled or has already expired is undefined behaviour
    assert(handle->second == &src);
    assert(handle != _pendingsources.end());
    assert(handle->first >= now());
    
    if (src.isTraffic()) {
        _trafficeventcount--;
    }
    _pendingsources.erase(handle);
}

void 
EventList::reschedulePendingSource(EventSource &src, simtime_picosec when) {
    cancelPendingSource(src);
    sourceIsPending(src, when);
}

EventSource::EventSource(const string& name) : EventSource(EventList::getTheEventList(), name) 
{
}
