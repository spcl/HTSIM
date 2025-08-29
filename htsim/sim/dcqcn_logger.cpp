// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#include <iostream>
#include <iomanip>
#include "dcqcn_logger.h"
#include "dcqcn.h"

#define ROCE_IS_ACK 1<<31
#define ROCE_IS_NACK 1<<30
#define ROCE_IS_HEADER 1<<28
#define ROCE_IS_LASTDATA 1<<27

void DCQCNTrafficLogger::logTraffic(Packet& pkt, Logged& location, TrafficEvent ev) {
    RocePacket& p = static_cast<RocePacket&>(pkt);
    uint32_t val3=0; // ugly hack to store data in a double

    if (p.type() == ROCEACK) {
        val3 |= ROCE_IS_ACK;
    } else if (p.type() == ROCENACK) {
        val3 |= ROCE_IS_NACK;
    } else if (p.type() == ROCE && p.last_packet()) {
        val3 |= ROCE_IS_LASTDATA;
    }

    if (p.header_only())
        val3 |= ROCE_IS_HEADER;
    
    _logfile->writeRecord(Logger::DCQCN_TRAFFIC,
                          location.get_id(),
                          ev,
                          p.flow().get_id(),
                          p.id(),
                          val3); 
}

string DCQCNTrafficLogger::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    assert(event._type == Logger::DCQCN_TRAFFIC);
    ss << " Type ROCETRAFFIC ID " << event._id;
    switch((TrafficLogger::TrafficEvent)event._ev) {
    case PKT_ARRIVE:
        ss << " Ev ARRIVE ";
        break;
    case PKT_DEPART:
        ss << " Ev DEPART ";
        break;
    case PKT_CREATESEND:
        ss << " Ev CREATESEND ";
        break;
    case PKT_CREATE:
        ss << " Ev CREATE ";
        break;
    case PKT_SEND:
        ss << " Ev SEND ";
        break;
    case PKT_DROP:
        ss << " Ev DROP ";
        break;
    case PKT_RCVDESTROY:
        ss << " Ev RCV ";
        break;
    case PKT_TRIM:
    case PKT_BOUNCE:
        abort(); // shouldn't happen with RoCE
    }
    ss << " FlowID " << (uint64_t)event._val1;
    uint32_t val3i = (uint32_t)event._val3;
    if (val3i & ROCE_IS_ACK) { 
        ss << " Ptype ACK"
           << " Ackno " << (uint64_t)event._val2;
    } else if (val3i & ROCE_IS_NACK) {
        ss << " Ptype NACK"
           << " Ackno " << (uint64_t)event._val2;
    } else {
        ss << " Ptype DATA"
           << " Seqno " << (uint64_t)event._val2;
        if (val3i & ROCE_IS_LASTDATA) {
            ss << " flag LASTDATA";
        }
    }
    if (val3i & ROCE_IS_HEADER) 
        ss << " Psize HEADER";
    else
        ss << " Psize FULL";
    return ss.str();
}

DCQCNSinkLoggerSampling::DCQCNSinkLoggerSampling(simtime_picosec period, 
                                               EventList& eventlist):
    SinkLoggerSampling(period, eventlist, Logger::DCQCN_SINK, HPCCLogger::RATE)
{
    cout << "DCQCNSinkLoggerSampling(p=" << timeAsSec(period) << " init \n";
}

void DCQCNSinkLoggerSampling::doNextEvent(){
    eventlist().sourceIsPendingRel(*this,_period);  
    simtime_picosec now = eventlist().now();
    simtime_picosec delta = now - _last_time;
    _last_time = now;
    TcpAck::seq_t  deltaB;
    uint32_t deltaSnd = 0;
    double rate;

    for (uint64_t i = 0; i<_sinks.size(); i++){
        DCQCNSink *sink = (DCQCNSink*)_sinks[i];
        if (_last_seq[i] <= sink->total_received()) {
            deltaB = sink->total_received() - _last_seq[i];
            if (delta > 0)
                rate = deltaB * 1000000000000.0 / delta;//Bps
            else 
                rate = 0;
            _logfile->writeRecord(_sink_type, sink->get_id(),
                                  _event_type, sink->cumulative_ack(), 
                                  deltaB>0?(deltaSnd * 100000 / deltaB):0, rate);

            _last_rate[i] = rate;
        }
        _last_seq[i] = sink->total_received();
    }
}

string DCQCNSinkLoggerSampling::event_to_str(RawLogEvent& event) {
    stringstream ss;
    ss << fixed << setprecision(9) << event._time;
    switch(event._type) {
    case Logger::HPCC_SINK:
        assert(event._ev == RoceLogger::RATE);
        ss << " Type DCQCN_SINK ID " << event._id << " Ev RATE"
           << " CAck " << (uint64_t)event._val1 << " Rate " << (uint64_t)event._val3;
        // val2 seems to always be zero - maybe a bug
        break;
    default:
        ss << "Unknown event " << event._type;
    }
    return ss.str();
}

