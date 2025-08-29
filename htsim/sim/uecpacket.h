// -*- c-basic-offset: 4; indent-tabs-mode: nil -*- 
#ifndef UECPACKET_H
#define UECPACKET_H

#include <cstdint>
#include <list>
#include <optional>
#include "network.h"
#include "ecn.h"

// All UEC packets are subclasses of Packet.
// They incorporate a packet database, to reuse packet objects that are no longer needed.
// Note: you never construct a new UEC packet directly; 
// rather you use the static method newpkt() which knows to reuse old packets from the database.

#define VALUE_NOT_SET -1

class UecBasePacket : public Packet {
public:
    enum PacketType {DATA_PULL = 0, DATA_SPEC = 1, DATA_RTX = 2, DATA_PROBE = 3};    
    typedef uint64_t seq_t;
    typedef uint64_t pull_quanta;  // actual pull fields are typically
                                   // uint16_t, but we'll use 64 bits
                                   // for fast simulation so we don't
                                   // need to cope with wrapping.
                                   // pull_quanta is in units of 512 bytes
  
    uint16_t _eqsrcid;  // source tunnel ID for the source.
    uint16_t _eqtgtid;  // destination tunnel ID. 
    const static int ACKSIZE=64;
    #define UEC_PULL_QUANTUM 256
    #define UEC_PULL_SHIFT 8
    static pull_quanta quantize_ceil(mem_b bytes);  // quantize and round up
    static pull_quanta quantize_floor(mem_b bytes); // quantize and round down
    static mem_b unquantize(pull_quanta credit_chunks);  // unquantize
    static mem_b get_ack_size() {return ACKSIZE;}
};

class UecDataPacket : public UecBasePacket {
    //using Packet::set_route;
public:
    //typedef enum {_500B,_1KB,_2KB,_4KB} packet_size;   // need to handle arbitrary packet sizes at end of messages

    inline static UecDataPacket* newpkt(PacketFlow &flow, const Route& route, 
                                         seq_t epsn, mem_b full_size, 
                                         PacketType pkttype, pull_quanta pull_target, 
                                         uint32_t destination = UINT32_MAX) {
        UecDataPacket* p = _packetdb.allocPacket();
        p->set_route(flow, route, full_size, epsn);  // also sets size and seqno
        p->_type = UECDATA;
        p->_is_header = false;
        p->_bounced = false;
        p->_epsn = epsn;
        p->_packet_type = pkttype;
        
        p->_eqsrcid = 0;
        p->_eqtgtid = 0;

        p->_pull_target = pull_target;
        p->_syn = false;
        p->_fin = false;
        
        p->_ar = false;
        p->set_dst(destination);

        p->_direction = NONE;
        p->_path_len = route.size();
        p->_trim_hop = {};
        p->_trim_direction = NONE;

        return p;
    }
  
    virtual inline void strip_payload(uint16_t trim_size = ACKSIZE) {
        Packet::strip_payload(trim_size); 
        _trim_hop = _nexthop;
        _trim_direction = _direction;
    };

    virtual inline void set_route(const Route &route) {
        if (_trim_hop.has_value())
            _trim_hop = *_trim_hop - route.size();

        Packet::set_route(route);
    }

    virtual inline void set_route(PacketFlow& flow, const Route &route, int pkt_size, packetid_t id){
        if (_trim_hop.has_value())
            _trim_hop = *_trim_hop - route.size();

        Packet::set_route(flow,route,pkt_size,id);
    };


    void free() {set_pathid(UINT32_MAX),  _packetdb.freePacket(this);}
    virtual ~UecDataPacket(){}

    inline seq_t epsn() const {return _epsn;}

    inline pull_quanta pull_target() const {return _pull_target;}
    inline bool retransmitted() const {return _packet_type == DATA_RTX;}
    inline void set_ar(bool ar){ _ar = ar;}

    inline PacketType type() const {return _packet_type;}

    inline bool ar() const {return _ar;}
    inline bool syn() const {return _syn;}
    inline bool fin() const {return _fin;}
    inline PacketType packet_type() const {return _packet_type;}

    inline int32_t trim_hop() const {return _trim_hop.value_or(INT32_MAX);}
    inline packet_direction trim_direction() const {return _trim_direction;}

    inline int32_t path_id() const {if (_pathid!=UINT32_MAX) return _pathid; else return _route->path_id();}

    virtual PktPriority priority() const {
        if (_is_header) {
            return Packet::PRIO_HI;
        } else {
            return _packet_type == DATA_SPEC ? PRIO_LO : PRIO_MID;
        }
    }

protected:
    seq_t _epsn;

    pull_quanta _pull_target;  // in a real implemention we'd handle wrapping, but here just never wrap
    bool _truncated; // we have more backlog than reflected in the pull_target

    bool _ar;
    bool _syn;
    bool _fin;

    PacketType _packet_type;

    //trim information, need to see if this stays here or goes to separate header.
    std::optional<int32_t> _trim_hop;
    packet_direction _trim_direction;
    static PacketDB<UecDataPacket> _packetdb;
};

class UecPullPacket : public UecBasePacket {
    using Packet::set_route;
public:
    inline static UecPullPacket* newpkt(PacketFlow& flow, const route_t* route, pull_quanta pullno, uint16_t ev,uint32_t destination = UINT32_MAX) {
        UecPullPacket* p = _packetdb.allocPacket();
        p->set_attrs(flow, ACKSIZE, 0);
        if (route) {
            // we may want to late-bind the route
            p->set_route();
        }
        assert(p->size()==ACKSIZE);

        p->_type = UECPULL;
        p->_is_header = true;
        p->_bounced = false;
        p->_pullno = pullno;
        p->_path_len = 0;
        p->set_dst(destination);
        p->_direction = NONE;

        p->_eqsrcid = 0;
        p->_eqtgtid = 0;
        p->_pathid = ev;
        //p->_rnr = rnr;
        p->_slow_pull = false;
        return p;
    }    

    void free() {set_pathid(UINT32_MAX), _packetdb.freePacket(this);}
    inline mem_b pullno() const {return _pullno;}
    inline bool is_rnr() const {return _rnr;}
    inline bool is_slow_pull() const {return _slow_pull;}
    inline void set_slow_pull(bool sp) {_slow_pull = sp;}

    virtual PktPriority priority() const {return Packet::PRIO_HI;}
  
    virtual ~UecPullPacket(){}

protected:
    pull_quanta _pullno;
    bool _slow_pull;

    bool _rnr;

    static PacketDB<UecPullPacket> _packetdb;
};

class UecAckPacket : public UecBasePacket {
    using Packet::set_route;
public:
    inline static UecAckPacket* newpkt(PacketFlow &flow, const Route *route, 
                                        seq_t cumulative_ack, seq_t ref_ack, seq_t acked_psn,/*pull_quanta pullno,*/
                                        uint16_t path_id, bool ecn_marked, uint64_t recv_bytes, uint8_t rcv_wnd_pen,
                                        uint32_t destination = UINT32_MAX) {
        UecAckPacket* p = _packetdb.allocPacket();
        p->set_attrs(flow, ACKSIZE, 0);
        if (route) {
            // we may want to late-bind the route
            p->set_route();
        }

        assert(p->size()==ACKSIZE);
        p->_type = UECACK;
        p->_is_header = true;
        p->_bounced = false;
        p->_ref_ack = ref_ack;
        p->_acked_psn = acked_psn;

        p->_cumulative_ack = cumulative_ack;
        //p->_pullno = pullno;
        p->_ev = path_id;
        p->set_pathid(path_id);
        p->_direction = NONE;
        p->_sack_bitmap = 0;
        p->_ecn_echo = ecn_marked;
        p->set_dst(destination);

        p->_recvd_bytes = recv_bytes;
        p->_rcv_cwnd_pen = rcv_wnd_pen;
        return p;
    }
  
    void free() {set_pathid(UINT32_MAX), _packetdb.freePacket(this);}
    inline seq_t ref_ack() const {return _ref_ack;}
    inline seq_t acked_psn() const {return _acked_psn;}
    inline seq_t cumulative_ack() const {return _cumulative_ack;}
    inline simtime_picosec residency_time() const {return _residency_time;}
    inline uint64_t recvd_bytes() const {return _recvd_bytes;}
    inline uint8_t rcv_wnd_pen() const {return _rcv_cwnd_pen;}
    inline void set_ooo(uint32_t out_of_order_count) { _out_of_order_count = out_of_order_count;}
    inline uint32_t ooo() const {return _out_of_order_count; }
    inline void set_is_rts(bool is_rts) { _is_rts = is_rts;}
    inline bool is_rts() const {return _is_rts; }

    inline void set_bitmap(uint64_t bitmap){_sack_bitmap = bitmap;};
    /* inline pull_quanta pullno() const {return _pullno;}*/
    uint16_t  ev() const {return _ev;}
    inline bool ecn_echo() const {return _ecn_echo;}
    uint64_t bitmap() const {return _sack_bitmap;}
    virtual PktPriority priority() const {return Packet::PRIO_HI;}
    
    void set_probe_ack(bool probe_ack){ _is_probe_ack = probe_ack; }
    inline bool is_probe_ack() const {return _is_probe_ack;}
    inline void set_rtx_echo(bool rtx_bit){_rtx_echo = rtx_bit;};
    inline bool rtx_echo() const {return _rtx_echo;}

    virtual ~UecAckPacket(){}

protected:
    seq_t _ref_ack;  // corresponds to the base of the bitmap
    seq_t _acked_psn; //the PSN of the packet that triggered this ACK.
    seq_t _cumulative_ack;  // highest in-order packet received.
    //pull_quanta _pullno; we don't need this field

    //SACK bitmap here 
    uint64_t _sack_bitmap;
    uint16_t _ev; //path id for the packet that triggered the SACK 

    uint64_t _recvd_bytes;
    uint8_t _rcv_cwnd_pen;

    bool _rnr;
    bool _ecn_echo;
    bool _rtx_echo;
    bool _is_rts = false;
    simtime_picosec _residency_time;
    uint32_t _out_of_order_count;
    bool _is_probe_ack;

    static PacketDB<UecAckPacket> _packetdb;
};

class UecNackPacket : public UecBasePacket {
    using Packet::set_route;
public:
    inline static UecNackPacket* newpkt(PacketFlow &flow, const Route *route, 
                                         seq_t ref_epsn, /*pull_quanta pullno, */
                                         uint16_t path_id,uint64_t recv_bytes, uint64_t tbytes,
                                         uint32_t destination = UINT32_MAX) {
        UecNackPacket* p = _packetdb.allocPacket();
        p->set_attrs(flow, ACKSIZE, ref_epsn);
        if (route) {
            // we may want to late-bind the route
            p->set_route();
        }

        assert(p->size()==ACKSIZE);
        p->_type = UECNACK;
        p->_is_header = true;
        p->_bounced = false;
        p->_ref_epsn = ref_epsn;
        //p->_pullno = pullno;
        p->_ev = path_id; // used to indicate which path the data packet was trimmed on
        p->set_pathid(path_id);
        p->_ecn_echo = false;
        p->_rnr = false;

        p->_direction = NONE;
        p->_path_len = 0;
        p->set_dst(destination);

        p->_recvd_bytes = recv_bytes;
        p->_target_bytes = tbytes;
        p->_last_hop = false;

        return p;
    }
  
    void free() {set_pathid(UINT32_MAX), _packetdb.freePacket(this);}
    inline seq_t ref_ack() const {return _ref_epsn;}
    //inline pull_quanta pullno() const {return _pullno;}
    uint16_t ev() const {return _ev;}
    inline void set_ecn_echo(bool ecn_echo) {_ecn_echo = ecn_echo;}
    inline bool ecn_echo() const {return _ecn_echo;}
    inline uint64_t recvd_bytes() const {return _recvd_bytes;}
    inline uint64_t target_bytes() const {return _target_bytes;}

    inline void set_last_hop(bool lh){ _last_hop = lh;}
    inline bool last_hop() const { return _last_hop;}
    virtual PktPriority priority() const {return Packet::PRIO_HI;}
  
    virtual ~UecNackPacket(){}

protected:
    seq_t _ref_epsn;
    //pull_quanta _pullno;
    uint16_t _ev;
    uint64_t _recvd_bytes;
    uint64_t _target_bytes;
    bool _last_hop;

    bool _rnr;
    bool _ecn_echo;
    static PacketDB<UecNackPacket> _packetdb;
};

class UecRtsPacket : public UecDataPacket {
    using Packet::set_route;
public:    
    inline static UecRtsPacket* newpkt(PacketFlow& flow, const Route* route, seq_t epsn, pull_quanta pull_target, uint32_t destination = UINT32_MAX) {
        UecRtsPacket* p = _packetdb.allocPacket();
        //p->set_route(flow,route,ACKSIZE,0);
        p->set_attrs(flow, ACKSIZE, 0);
        if (route) {
            // we may want to late-bind the route
            p->set_route();
        }
        p->_type = UECRTS;
        p->_is_header = true;
        p->_bounced = false;
        p->_pull_target = pull_target;
        p->_epsn = epsn;
        p->_direction = NONE;    

        p->_ar = true; //always request ack.
        p->set_dst(destination);
        return p;
    }
    
    void free() {set_pathid(UINT32_MAX), _packetdb.freePacket(this);}
    
    inline bool ar() const {return _ar;}

    virtual PktPriority priority() const {return Packet::PRIO_HI;}
    
    virtual ~UecRtsPacket(){}

protected:
    static PacketDB<UecRtsPacket> _packetdb;
};

#endif
