#include "config.h"
#include "eventlist.h"

class BBRPacer : public EventSource {
  public:
    BBRPacer(EventList &eventlist, BBRSrc &flow);
    bool is_pending() const {
        // printf("Delay Send %lu \n", _interpacket_delay);
        return _interpacket_delay > 0;
    }
    void schedule_send(simtime_picosec delay);
    void just_sent();
    void doNextEvent();
    void cancel();
    simtime_picosec _interpacket_delay; // the interpacket delay, or zero if
                                        // we're not pacing

  private:
    BBRSrc *flow;
    simtime_picosec _last_send; // when the last packet was sent (always set,
                                // even when we're not pacing)
    simtime_picosec _next_send; // when the next scheduled packet should be sent
};