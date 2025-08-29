// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-        
#ifndef UEC_LOGGER_H
#define UEC_LOGGER_H
#include "config.h"
#include "loggers.h"
#include "uec.h"

class UecSrc;

class UecLogger  : public Logger {
 public:
    enum UecEvent { UEC_RCV=0, UEC_RCV_FR_END=1, UEC_RCV_FR=2, UEC_RCV_DUP_FR=3,
                    UEC_RCV_DUP=4, UEC_RCV_3DUPNOFR=5,
                    UEC_RCV_DUP_FASTXMIT=6, UEC_TIMEOUT=7};
    enum UecState { UECSTATE_CNTRL=0, UECSTATE_SEQ=1 };
    enum UecRecord { AVE_CWND=0 };
    enum UecSinkRecord { RATE = 0 };
    enum UecMemoryRecord  {MEMORY = 0};

    virtual void logUec(UecSrc &src, UecEvent ev) =0;
    virtual ~UecLogger(){};
};

class UecSinkLoggerSampling : public SinkLoggerSampling {
    virtual void doNextEvent();
 public:
    UecSinkLoggerSampling(simtime_picosec period, EventList& eventlist);
    static string event_to_str(RawLogEvent& event);
};

/*
class UecNicLoggerSampling : public NicLoggerSampling {
    virtual void doNextEvent();
 public:
    UecNicLoggerSampling(simtime_picosec period, EventList& eventlist);
    static string event_to_str(RawLogEvent& event);
};
*/

#endif
