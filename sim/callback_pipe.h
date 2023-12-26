// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef CALLBACKPIPE_H
#define CALLBACKPIPE_H

/*
 * A pipe is a dumb device which simply delays all incoming packets
 */

#include "config.h"
#include "loggertypes.h"
#include "network.h"
#include "pipe.h"
#include <list>
#include <utility>

class CallbackPipe : public Pipe {
  public:
    CallbackPipe(simtime_picosec delay, EventList &eventlist, PacketSink *callback);
    virtual void doNextEvent(); // inherited from Pipe
  private:
    PacketSink *_callback;
};

#endif
