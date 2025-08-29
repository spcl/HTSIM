#include <math.h>
#include "oversubscribed_cc.h"
#include "uec.h"

simtime_picosec OversubscribedCC::_base_rtt = timeFromUs(12u);
double OversubscribedCC::_target_congestion = 0.3;
double OversubscribedCC::_Ai = .01;
double OversubscribedCC::_Md = 0.5;
double OversubscribedCC::_min_rate = 0.01;
double OversubscribedCC::_alpha = 0.5;

OversubscribedCC::OversubscribedCC(EventList& eventList,UecPullPacer* pacer)
    : EventSource(eventList, "OversubscribedCC"),
	_pullPacer(pacer){
	_rate = 1;
	_g = 0;
	_received_bytes = 0;
	_ecn_bytes = 0;
	_received = 0;
	_old_received = 0;
	_ecn = 0;
	_old_ecn = 0;
	_trimmed_last_hop = 0;
	_old_trimmed_last_hop = 0;
	_trimmed_other = 0;
	_old_trimmed_other = 0;
    _increase_count = 0;

	eventList.sourceIsPendingRel(*this,nextInterval());
}

void
OversubscribedCC::doNextEvent(){
    doCongestionControl();
    eventlist().sourceIsPendingRel(*this,nextInterval());
}

void 
OversubscribedCC::doCongestionControl(){
    int total_packets = _received - _old_received;
    int ecn = _ecn - _old_ecn;
    int trimmed_last_hop = _trimmed_last_hop - _old_trimmed_last_hop;
    int trimmed_other = _trimmed_other - _old_trimmed_other;
    _old_received = _received;
    _old_ecn = _ecn;
    _old_trimmed_last_hop = _trimmed_last_hop;
    _old_trimmed_other = _trimmed_other;

    assert (ecn+trimmed_last_hop+trimmed_other<=total_packets);
    double fraction = 0;
    bool decrease = false;

    //compute fraction of ECN marked packets out of the full data packets received.
    if (total_packets-trimmed_last_hop-trimmed_other!=0)
        fraction = (double)ecn / (total_packets -trimmed_last_hop-trimmed_other);

    //decrement based on non last hop trims.
    if (trimmed_other > 0) {
        _rate -= (double)trimmed_other / total_packets;
        decrease = true;
    }

    _g = _g * (1-_alpha) + _alpha * fraction; 

    if (_g>_target_congestion && _Md > 0){
        _rate = _rate * (1 - (_g-_target_congestion)* _Md);
        decrease = true;
    }
   
    if (!decrease){
        if (ecn==0)
            _rate += _Ai * pow(2,_increase_count);
        else
            _rate += _Ai;

        _increase_count++;
    }
    else {
        _increase_count = 0;
    }

    if (_rate < _min_rate)
        _rate = _min_rate;
    if (_rate > 1)
        _rate = 1;

    if (UecSrc::_debug)
        cout << "At "<< timeAsUs(eventlist().now()) << " oversubscribed cc " << _g << " rate " << _rate << " flow " << _pullPacer->get_id() << " total packets " << total_packets << " trimmed " << trimmed_other << " trimmed last hop " << trimmed_last_hop << " ecn " << ecn << endl;

    _pullPacer->updatePullRate(UecPullPacer::OVERSUBSCRIBED_CC, _rate);
}
