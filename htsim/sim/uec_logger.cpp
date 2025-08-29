// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include <iostream>
#include <iomanip>
#include <vector>
#include "uec_logger.h"

UecSinkLoggerSampling::UecSinkLoggerSampling(simtime_picosec period,
                                             EventList& eventlist):
    SinkLoggerSampling(period, eventlist, Logger::UEC_SINK, UecLogger::RATE)
{
    cout << "UecSinkLoggerSampling(p=" << timeAsSec(period) << " init \n";
}

void UecSinkLoggerSampling::doNextEvent(){
    eventlist().sourceIsPendingRel(*this,_period);
    simtime_picosec now = eventlist().now();
    simtime_picosec delta = now - _last_time;
    _last_time = now;
    TcpAck::seq_t  deltaB;
    //uint32_t deltaSnd = 0;                                                                                                                                                                                         
    double rate;
    //cout << "UecSinkLoggerSampling(p=" << timeAsSec(_period) << "), t=" << timeAsSec(now) << "\n";                                                                                                                
    for (uint64_t i = 0; i<_sinks.size(); i++){
        UecSink *sink = (UecSink*)_sinks[i];
        if ((mem_b)_last_seq[i] <= sink->total_received()) {
            deltaB = sink->total_received() - _last_seq[i];
            if (delta > 0)
                rate = deltaB * 1000000000000.0 / delta;//Bps                                                                                                                                                        
            else
                rate = 0;

            _logfile->writeRecord(_sink_type, sink->get_id(),
                                  _event_type, sink->cumulative_ack(),
                                  /*deltaB>0?(deltaSnd * 100000 / deltaB):0*/
                                  sink->reorder_buffer_size(), rate);

            _last_rate[i] = rate;
	}
        _last_seq[i] = sink->total_received();
    }
}

string UecSinkLoggerSampling::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    switch(event._type) {
    case Logger::UEC_SINK:
	assert(event._ev == UecLogger::RATE);
        ss << " Type UEC_SINK ID " << event._id << " Ev RATE"
           << " CAck " << (uint64_t)event._val1 << " ReorderBuffer " << (uint64_t)event._val2 << " Rate " << (uint64_t)event._val3;
        // val2 seems to always be zero - maybe a bug                                                                                                                                                                
        break;
    default:
        ss << "Unknown event " << event._type;
    }
    return ss.str();
}

/*
UecNicLoggerSampling::UecNicLoggerSampling(simtime_picosec period,
                                             EventList& eventlist):
    NicLoggerSampling(period, eventlist)
{
    cout << "UecNicLoggerSampling(p=" << timeAsSec(period) << " init \n";
}
*/





