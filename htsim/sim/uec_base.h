// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef UEC_BASE_H
#define UEC_BASE_H

#include "uecpacket.h"


class UecMsgTracker {
public:
    /*
    * Return next packet and keep track of its status internally.
    */
    virtual mem_b getNextPacket(UecDataPacket::seq_t seq_no) = 0;
    /*
    * Notify of seq no of dataless control packet
    */
    virtual void notifyCtrlSeqno(UecDataPacket::seq_t seq_no) = 0;
    /*
    * Track received packets. If this sequence number completed the
    * receiving of a message, its message id is returned.
    */
    virtual void addRecvd(UecDataPacket::seq_t seq_no) = 0;
    /*
    * Handle continuous acks
    */
    virtual void addCumAck(UecDataPacket::seq_t cum_ack) = 0;
    /*
    * Handle selective acks
    */
    virtual void addSAck(UecDataPacket::seq_t ackno) = 0;
    /*
    * Tells the connection that there is no additional data to send.
    * Returns true if all data is in flight (i.e. has been marked as
    * such through getNextPacket). 
    */
    virtual bool checkDoneSending() = 0;
    /*
    * Tells the connection that we are done for now.
    * Returns true if all messages have been confirmed by acks.
    */
    virtual bool checkFinished() = 0;
    /* All messages have finished, including the scheduled and triggered ones. 
    */
    virtual bool isTotallyFinished() = 0;
    /*
     Return the number of completed messages.
    */
    virtual uint32_t getMsgCompleted() = 0;

    virtual ~UecMsgTracker() = default;
};


class UecTransportConnection {
public:
    /*
     Make the connection reusable and set a connection tracker.
    */
    virtual void makeReusable(UecMsgTracker* conn_reuse_tracker) = 0;
    /*
    */
    virtual void addToBacklog(mem_b size) = 0;
    /*
     Has this connection been active before?
    */
    virtual bool hasStarted() = 0;
    /*
     This function is used by upper layers to determine if this connection/flow is currently
     being actively scheduled. This should be the case as long as there is more data to send.
     In that case the connection is either blocked by the NIC or blocked by CC; in either case
     there is nothing to do for the upper layer.
    */
    virtual bool isActivelySending() = 0;
    /*
     Start sending if it has not done so before.
    */
    virtual void startConnection() = 0;
    /* Continue flow after a short break. Is only used when conn_reuse is enabled. */
    virtual void continueConnection() = 0;

    virtual ~UecTransportConnection() = default;
};

#endif  // UEC_BASE_H
