// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef CNPPACKET_H
#define CNPPACKET_H

#include <list>
#include "network.h"

#define VALUE_NOT_SET -1
//#define PULL_MAXPATHS 256 // maximum path ID that can be pulled

class CNPPacket: public Packet {
public:
    typedef uint64_t seq_t;
    
    inline static CNPPacket* newpkt(PacketFlow &flow, const Route &route, 
                                  seq_t ackno, uint32_t destination = UINT32_MAX) {
        CNPPacket* p = _packetdb.allocPacket();
        p->set_route(flow,route,ACKSIZE,ackno);
        p->_type = CNP;
        p->_is_header = true;
        p->_ackno = ackno;
        p->_path_len = 0;
        p->_direction = NONE;
        p->set_dst(destination);
        return p;
    }

    void free() {_packetdb.freePacket(this);}
    inline seq_t ackno() const {return _ackno;}
    virtual PktPriority priority() const {return Packet::PRIO_HI;}
  
    virtual ~CNPPacket(){}
    const static int ACKSIZE=64; 
protected:
    seq_t _ackno;
    static PacketDB<CNPPacket> _packetdb;
};

#endif
