// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef DCQCN_LOGGER_H
#define DCQCN_LOGGER_H

#include "loggers.h"

class DCQCNSrc;

class DCQCNLogger  : public Logger {
 public:
    enum DCQCNEvent { DCQCN_RCV=0, DCQCN_TIMEOUT=1 };
    enum DCQCNState { DCQCNSTATE_ON=1, DCQCNSTATE_OFF=0 };
    enum DCQCNRecord { AVE_RATE=0 };
    enum DCQCNSinkRecord { RATE = 0 };
    enum DCQCNMemoryRecord  {MEMORY = 0};

    virtual void logDCQCN(DCQCNSrc &src, DCQCNEvent ev) =0;
    virtual ~DCQCNLogger(){};
};

class DCQCNTrafficLogger : public TrafficLogger {
 public:
    void logTraffic(Packet& pkt, Logged& location, TrafficEvent ev);
    static string event_to_str(RawLogEvent& event);
};

class DCQCNSinkLoggerSampling : public SinkLoggerSampling {
    virtual void doNextEvent();
 public:
    DCQCNSinkLoggerSampling(simtime_picosec period, EventList& eventlist);
    static string event_to_str(RawLogEvent& event);
};

#endif
