// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef UEC_H
#define UEC_H

#include <memory>
#include <list>
#include <set>
#include <optional>

#include "uec_base.h"
#include "eventlist.h"
#include "trigger.h"
#include "uecpacket.h"
#include "circular_buffer.h"
#include "modular_vector.h"
#include "pciemodel.h"
#include "oversubscribed_cc.h"
#include "uec_mp.h"
#include "atlahs_event.h"
#include "atlahs_htsim_api.h"

#define timeInf 0
// min RTO bound in us
//  *** don't change this default - override it by calling UecSrc::setMinRTO()
#define DEFAULT_UEC_RTO_MIN 100

static const unsigned uecMaxInFlightPkts = 1 << 14;
class UecPullPacer;
class UecSink;
class UecSrc;
class UecLogger;


// UecNIC aggregates UecSrcs that are on the same NIC.  It round
// robins between active srcs when we're limited by the sending
// linkspeed due to outcast (or just at startup) - this avoids
// building an output queue like the old NDP simulator did, and so
// better models what happens in a h/w NIC.
class UecNIC : public EventSource, public NIC {
    struct PortData {
        simtime_picosec send_end_time;
        bool busy;
        mem_b last_pktsize;
    };
    struct CtrlPacket {
        UecBasePacket* pkt;
        UecSrc* src;
        UecSink* sink;
    };
public:
    UecNIC(id_t src_num, EventList& eventList, linkspeed_bps linkspeed, uint32_t ports);

    // handle traffic sources.
    const Route* requestSending(UecSrc& src);
    void startSending(UecSrc& src, mem_b pkt_size, const Route* rt);
    void cantSend(UecSrc& src);

    // handle control traffic from receivers.
    // only one of src or sink must be set
    void sendControlPacket(UecBasePacket* pkt, UecSrc* src, UecSink* sink);
    uint32_t findFreePort();
    void doNextEvent();

    linkspeed_bps linkspeed() const {return _linkspeed;}

    int activeSources() const { return _active_srcs.size(); }
    virtual const string& nodename() const {return _nodename;}
    list<UecSrc*> _active_srcs;

private:
    void sendControlPktNow();
    uint32_t sendOnFreePortNow(simtime_picosec endtime, const Route* rt);
    list<struct CtrlPacket> _control;
    mem_b _control_size;

    linkspeed_bps _linkspeed;

    // data related to the NIC ports
    vector<struct PortData> _ports;
    uint32_t _rr_port;  // round robin last port we sent on
    uint32_t _no_of_ports;
    uint32_t _busy_ports;

    int _ratio_data, _ratio_control, _crt;

    string _nodename;
};

// Packets are received on ports, but then passed to the Src for handling
class UecSrcPort : public PacketSink {
public:
    UecSrcPort(UecSrc& src, uint32_t portnum);
    void setRoute(const Route& route);
    inline const Route* route() const {return _route;}
    virtual void receivePacket(Packet& pkt);
    virtual const string& nodename();
private:
    UecSrc& _src;
    uint8_t _port_num;
    const Route* _route;  // we're only going to support ECMP_HOST for now.
};

class UecSrc : public EventSource, public TriggerTarget, public UecTransportConnection {
public:
    struct Stats {
        /* all must be non-negative, but we'll make them signed so we
           can do maths with them without concern about underflow */
        int32_t new_pkts_sent;
        int32_t rtx_pkts_sent;
        int32_t rts_pkts_sent;
        int32_t rto_events;
        int32_t acks_received;
        int32_t nacks_received;
        int32_t pulls_received;
        int32_t bounces_received;
        int32_t rts_nacks;
        int32_t _sleek_counter;
    };
    struct NsccStats {
        mem_b inc_fair_bytes;
        mem_b inc_prop_bytes;
        mem_b inc_fast_bytes;
        mem_b inc_eta_bytes;
        mem_b dec_multi_bytes;
        mem_b dec_quick_bytes;
        mem_b dec_nack_bytes;
    };
    UecSrc(TrafficLogger* trafficLogger, 
           EventList& eventList, 
           unique_ptr<UecMultipath> mp,
           UecNIC& nic, 
           uint32_t no_of_ports, 
           bool rts = false);
    void delFromSendTimes(simtime_picosec time, UecDataPacket::seq_t seq_no);
    /**
     * Initialize global NSCC parameters.
     */
    static void initNsccParams(simtime_picosec network_rtt, linkspeed_bps linkspeed, 
                               simtime_picosec target_Qdelay, int8_t qa_gate,
                               bool trimming_enabled);
    /**
     * Initialize per-connection NSCC parameters.
     */
    void initNscc(mem_b cwnd, simtime_picosec peer_rtt=UecSrc::_network_rtt);
    /**
     * Initialize per-connection RCCC parameters.
     */
    void initRccc(mem_b cwnd,simtime_picosec peer_rtt=UecSrc::_network_rtt);

    void logFlowEvents(FlowEventLogger& flow_logger) { _flow_logger = &flow_logger; }
    virtual void connectPort(uint32_t portnum, Route& routeout, Route& routeback, UecSink& sink, simtime_picosec start);
    const Route* getPortRoute(uint32_t port_num) const {return _ports[port_num]->route();}
    UecSrcPort* getPort(uint32_t port_num) {return _ports[port_num];}
    void timeToSend(const Route& route);
    void receivePacket(Packet& pkt, uint32_t portnum);
    void doNextEvent();
    uint32_t dst() { return _dstaddr; }
    void setDst(uint32_t dst) { _dstaddr = dst; }

    // Functions from UecTransportConnection
    virtual void continueConnection() override;
    virtual void startConnection() override;
    virtual bool hasStarted() override;
    virtual bool isActivelySending() override;
    virtual void makeReusable(UecMsgTracker* conn_reuse_tracker) override { _msg_tracker.emplace(conn_reuse_tracker); };
    virtual void addToBacklog(mem_b size) override;

    static void setMinRTO(uint32_t min_rto_in_us) {
        _min_rto = timeFromUs((uint32_t)min_rto_in_us);
    }
    void setCwnd(mem_b cwnd) {
        //_maxwnd = cwnd;
        _cwnd = cwnd;
    }
    void setMaxWnd(mem_b maxwnd) {
        //_maxwnd = cwnd;
        _maxwnd = maxwnd;
    }

    void setConfiguredMaxWnd(mem_b wnd){
        _configured_maxwnd = wnd;
    }

    void boundBaseRTT(simtime_picosec network_rtt){
        _base_rtt = network_rtt;
        _bdp = timeAsUs(_base_rtt) * _nic.linkspeed() / 8000000;
        _maxwnd =  1.5*_bdp;
        _configured_maxwnd = _maxwnd;

        if (!_shown){
            cout << "Bound base RTT: _bdp " << _bdp << " _maxwnd " << _maxwnd << " _base_rtt " << timeAsUs(_base_rtt) << endl;
            _shown = true;
        }
    }
    mem_b configuredMaxWnd() const { return _configured_maxwnd; }
    /*
     If a PDC is used, call the same function there. Checks if _all_ msgs are done,
     include the ones in the various backlog queues.
     Otherwise, it just returns the status of _done_sending.
    */
    bool isTotallyFinished();

    const Stats& stats() const { return _stats; }

    void setEndTrigger(Trigger& trigger);
    // called from a trigger to start the flow.
    virtual void activate();
    static int _global_node_count;
    static simtime_picosec _min_rto;
    static uint16_t _hdr_size;
    static uint16_t _mss;  // does not include header
    static uint16_t _mtu;  // does include header

    static bool _sender_based_cc;
    static bool _receiver_based_cc;

    enum Sender_CC { DCTCP, NSCC, CONSTANT};
    static Sender_CC _sender_cc_algo;

    static bool _disable_quick_adapt;
    static uint8_t _qa_gate;

    static bool update_base_rtt_on_nack;
    static bool _enable_sleek;

    virtual const string& nodename() { return _nodename; }
    virtual void setName(const string& name) override { _name=name; _mp->set_debug_tag(name); }
    inline void setFlowId(flowid_t flow_id) { _flow.set_flowid(flow_id); }
    void setFlowsize(uint64_t flow_size_in_bytes);
    mem_b flowsize() { return _flow_size; }
    inline PacketFlow* flow() { return &_flow; }
    optional<UecMsgTracker*> msg_tracker() { return _msg_tracker; };

    inline flowid_t flowId() const { return _flow.flow_id(); }

    static bool _debug;
    static bool _shown;
    bool _debug_src;
    bool debug() const { return _debug_src; }

   private:
    unique_ptr<UecMultipath> _mp;
    UecNIC& _nic;
    uint32_t _no_of_ports;
    vector <UecSrcPort*> _ports;
    struct sendRecord {
        // need a constructor to be able to put this in a map
        sendRecord(mem_b psize, simtime_picosec stime) : pkt_size(psize), send_time(stime){};
        mem_b pkt_size;
        simtime_picosec send_time;
    };
    UecLogger* _logger;
    TrafficLogger* _pktlogger;
    FlowEventLogger* _flow_logger;
    Trigger* _end_trigger;

    // TODO in-flight packet storage - acks and sacks clear it
    // list<UecDataPacket*> _activePackets;

    // we need to access the in_flight packet list quickly by sequence number, or by send time.
    map<UecDataPacket::seq_t, sendRecord> _tx_bitmap;
    multimap<simtime_picosec, UecDataPacket::seq_t> _send_times;
    map<UecDataPacket::seq_t, uint16_t> _rtx_times;

    map<UecDataPacket::seq_t, mem_b> _rtx_queue;
    bool isSendPermitted();
    void sendIfPermitted();
    mem_b sendPacket(const Route& route);
    mem_b sendNewPacket(const Route& route);
    mem_b sendRtxPacket(const Route& route);
    void sendRTS();
    void sendProbe();
    void createSendRecord(UecDataPacket::seq_t seqno, mem_b pkt_size);
    void queueForRtx(UecBasePacket::seq_t seqno, mem_b pkt_size);
    bool validateSendTs(UecBasePacket::seq_t acked_psn, bool rtx_echo);
    void recalculateRTO();
    void startRTO(simtime_picosec send_time);
    void clearRTO();   // timer just expired, clear the state
    void cancelRTO();  // cancel running timer and clear state

    // not used, except for debugging timer issues
    void checkRTO() {
        if (_rtx_timeout_pending)
            assert(_rto_timer_handle != eventlist().nullHandle());
        else
            assert(_rto_timer_handle == eventlist().nullHandle());
    }

    void rtxTimerExpired();
    UecBasePacket::pull_quanta computePullTarget();
    void handlePull(UecBasePacket::pull_quanta pullno);
    mem_b handleAckno(UecDataPacket::seq_t ackno);
    mem_b handleCumulativeAck(UecDataPacket::seq_t cum_ack);
    void processAck(const UecAckPacket& pkt);
    void processNack(const UecNackPacket& pkt);
    void processPull(const UecPullPacket& pkt);
    void runSleek(uint32_t ooo, UecBasePacket::seq_t cum_ack);

    //added for NSCC
    bool can_send_NSCC(mem_b pkt_size);
    bool can_send_RCCC();
    void set_cwnd_bounds();
    mem_b getNextPacketSize();
    void quick_adapt(bool trimmed);
    void updateCwndOnAck_NSCC(bool skip, simtime_picosec delay, mem_b newly_acked_bytes);
    void updateCwndOnNack_NSCC(bool skip, mem_b nacked_bytes, bool last_hop);

    void updateCwndOnAck_DCTCP(bool skip, simtime_picosec delay, mem_b newly_acked_bytes);
    void updateCwndOnNack_DCTCP(bool skip, mem_b nacked_bytes, bool last_hop);

    void dontUpdateCwndOnAck(bool skip, simtime_picosec delay, mem_b newly_acked_bytes);
    void dontUpdateCwndOnNack(bool skip, mem_b nacked_bytes, bool last_hop);

    void (UecSrc::*updateCwndOnAck)(bool skip, simtime_picosec delay, mem_b newly_acked_bytes);
    void (UecSrc::*updateCwndOnNack)(bool skip, mem_b nacked_bytes, bool last_hop);

    bool checkFinished(UecDataPacket::seq_t cum_ack);

    Stats _stats;
    // Stats over the whole connection lifetime
    NsccStats _nscc_overall_stats;
    // Stats per fulfill-adjustment period
    NsccStats _nscc_fulfill_stats;
    UecSink* _sink;

    // unlike in the NDP simulator, we maintain all the main quantities in bytes
    mem_b _flow_size;
    bool _done_sending;  // make sure we only trigger once
    optional<UecMsgTracker*> _msg_tracker;  
    mem_b _backlog;      // how much we need to send, not including retransmissions
    mem_b _rtx_backlog;
    mem_b _cwnd;
    mem_b _maxwnd;
    static mem_b _configured_maxwnd;
    UecBasePacket::pull_quanta _pull_target;
    UecBasePacket::pull_quanta _pull;
    mem_b _credit;  // receive request credit in pull_quanta, but consume it in bytes
    inline mem_b credit() const;
    void stopSpeculating();
    void spendCredit(mem_b pktsize);
    UecDataPacket::seq_t _highest_sent;
    UecDataPacket::seq_t _highest_rtx_sent;
    mem_b _in_flight;
    mem_b _bdp;
    bool _send_blocked_on_nic;
    bool _speculating;

    // Record last time this UecSrc was scheduled.
    optional<simtime_picosec> _last_event_time;
public:
    static linkspeed_bps _reference_network_linkspeed; 
    static simtime_picosec _reference_network_rtt; 
    static mem_b _reference_network_bdp; 
    static linkspeed_bps _network_linkspeed; 
    static simtime_picosec _network_rtt; 
    static mem_b _network_bdp; 
    static bool _network_trimming_enabled; 
    // Smarttrack parameters
    static mem_b _min_cwnd; 
    static uint32_t _qa_scaling; 
    static simtime_picosec _target_Qdelay;
    static double _gamma;
    static double _alpha;
    // static double _scaling_c;
    // static double _fd;
    static double _fi;
    static double _fi_scale;
    static double _scaling_factor_a;
    static double _scaling_factor_b;
    static double _eta;
    static double _qa_threshold; 
    static double _delay_alpha;
    // static double _ecn_thresh;
    static uint32_t _adjust_bytes_threshold;
    static simtime_picosec _adjust_period_threshold;
    //debug
    static flowid_t _debug_flowid;

    // ATLAHS
    AtlahsHtsimApi *_atlahs_api = nullptr;
    graph_node_properties *lgs_node; // used for logging purposes
    uint64_t send_size = 0;
    uint32_t from = -1;
    uint32_t to = -1;
    uint32_t tag;
    uint64_t lgs_time;
    uint64_t lgs_starttime;         // only used for MSGs to identify start times
    uint64_t lgs_syncstart;

    uint64_t lgs_ts; /* this is a timestamp that determines the (original) insertion order of 
                  elements in the queue, it is increased for every new element, not for 
                  re-insertions! Needed for correctness. */
    uint64_t lgs_size;						// number of bytes to send, recv, or time to spend in loclop
    uint32_t lgs_target;					// partner for send/recv
    uint32_t lgs_host;            // owning host 
    uint32_t lgs_offset;          // for Parser (to identify schedule element)
    uint32_t lgs_tag;							// tag for send/recv
    uint32_t lgs_handle;          // handle for network layer :-/
    uint8_t lgs_proc;							// processing element for this operation
    uint8_t lgs_nic;							// network interface for this operation
    char lgs_type;							  // see below
    void setFlowSize(uint64_t flow_size) { _flow_size = flow_size; }
    void set_dst(uint32_t dst) { _dstaddr = dst; }
    void setSrc(uint32_t src) { _srcaddr = src; }
    uint32_t             _srcaddr;
    simtime_picosec last_data_sent_time;
    simtime_picosec _flow_start_time;

private:
    bool quick_adapt(bool is_loss, bool skip, simtime_picosec delay);
    void fair_increase(uint32_t newly_acked_bytes);
    void proportional_increase(uint32_t newly_acked_bytes,simtime_picosec delay);
    void fast_increase(uint32_t newly_acked_bytes,simtime_picosec delay);
    // void fair_decrease(bool can_decrease, uint32_t newly_acked_bytes);
    void multiplicative_decrease();
    void fulfill_adjustment();
    void mark_packet_for_retransmission(UecBasePacket::seq_t psn, uint16_t pktsize);
    void update_delay(simtime_picosec delay, bool update_avg, bool skip);
    void update_base_rtt(simtime_picosec raw_rtt);
    simtime_picosec get_avg_delay();
    uint16_t get_avg_pktsize();

    // RTT estimate data for RTO and sender based CC.
    simtime_picosec _rtt, _mdev, _rto, _raw_rtt;
    bool _rtx_timeout_pending;       // is the RTO running?
    simtime_picosec _rto_send_time;  // when we sent the oldest packet that the RTO is waiting on.
    simtime_picosec _rtx_timeout;    // when the RTO is currently set to expire
    simtime_picosec _last_rts;       // time when we last sent an RTS (or zero if never sent)
    EventList::Handle _rto_timer_handle;


    //used to drive ACK clock
    uint64_t _recvd_bytes;

    // Smarttrack sender based CC variables.
    simtime_picosec _base_rtt;
    mem_b _base_bdp;
    mem_b _achieved_bytes = 0;
    //used to trigger SmartTrack fulfill
    mem_b _received_bytes = 0;
    uint32_t _fi_count = 0;
    bool _trigger_qa = false;
    simtime_picosec _qa_endtime = 0;
    uint32_t _bytes_to_ignore = 0;
    uint32_t _bytes_ignored = 0;
    uint32_t _inc_bytes = 0;
    simtime_picosec _avg_delay = 0;

    simtime_picosec _last_eta_time = 0;
    simtime_picosec _last_adjust_time = 0;
    bool _increase = false;
    simtime_picosec _last_dec_time = 0;
    uint32_t _highest_recv_seqno;

    /******** SLEEK parameters *********/

    static float loss_retx_factor;
    static int min_retx_config ;
    bool _loss_recovery_mode = false;
    uint32_t _recovery_seqno = 0;
    /******** END SLEEK parameters *********/

    /******** Probe parameters *********/    
    static int probe_first_trial_time;
    static int probe_retry_time;
    simtime_picosec _probe_timer_when = 0;
    simtime_picosec _probe_seqno = 0; 
    simtime_picosec _probe_send_time = 0; 
    EventList::Handle _probe_timer_handle; 
    /******** END Probe parameters *********/


    // Connectivity
    PacketFlow _flow;
    string _nodename;
    int _node_num;
    uint32_t _dstaddr;
};

// Packets are received on ports, but then passed to the Sink for handling
class UecSinkPort : public PacketSink {
public:
    UecSinkPort(UecSink& sink, uint32_t portnum);
    void setRoute(const Route& route);
    inline const Route* route() const {return _route;}
    virtual void receivePacket(Packet& pkt);
    virtual const string& nodename();
private:
    UecSink& _sink;
    uint8_t _port_num;
    const Route* _route;
};

class UecSink : public DataReceiver {
   public:
    struct Stats {
        uint64_t received;
        uint64_t bytes_received;
        uint64_t duplicates;
        uint64_t out_of_order;
        uint64_t trimmed;
        uint64_t pulls;
        uint64_t rts;
        uint64_t ecn_received;
        uint64_t ecn_bytes_received;
    };

    UecSink(TrafficLogger* trafficLogger, UecPullPacer* pullPacer, UecNIC& nic, uint32_t no_of_ports);
    UecSink(TrafficLogger* trafficLogger,
             linkspeed_bps linkSpeed,
             double rate_modifier,
             uint16_t mtu,
             EventList& eventList,
             UecNIC& nic, uint32_t no_of_ports);
    void receivePacket(Packet& pkt, uint32_t port_num);

    void processData(UecDataPacket& pkt);
    void processRts(const UecRtsPacket& pkt);
    void processTrimmed(const UecDataPacket& pkt);

    void handlePullTarget(UecBasePacket::seq_t pt);

    virtual const string& nodename() { return _nodename; }
    virtual uint64_t cumulative_ack() { return _expected_epsn; }
    virtual uint32_t drops() { return 0; }

    inline flowid_t flowId() const { return _flow.flow_id(); }

    UecPullPacket* pull(UecBasePacket::pull_quanta& extra_credit);

    bool shouldSack();
    uint16_t unackedPackets();
    void setEndTrigger(Trigger& trigger);

    UecBasePacket::seq_t sackBitmapBase(UecBasePacket::seq_t epsn);
    UecBasePacket::seq_t sackBitmapBaseIdeal();
    uint64_t buildSackBitmap(UecBasePacket::seq_t ref_epsn);
    UecAckPacket* sack(uint16_t path_id, UecBasePacket::seq_t seqno, UecBasePacket::seq_t acked_psn, bool ce, bool rtx_echo);

    UecNackPacket* nack(uint16_t path_id, UecBasePacket::seq_t seqno, bool last_hop, bool ecn_echo);

    UecBasePacket::pull_quanta backlog() {
        if (_highest_pull_target > _latest_pull)
            return _highest_pull_target - _latest_pull;
        else
            return 0;
    }
    UecBasePacket::pull_quanta slowCredit() {
        if (_highest_pull_target >= _latest_pull)
            return 0;
        else
            return _latest_pull - _highest_pull_target;
    }

    UecBasePacket::pull_quanta rtx_backlog() { return _retx_backlog; }
    const Stats& stats() const { return _stats; }
    void connectPort(uint32_t port_num, UecSrc& src, const Route& routeback);
    const Route* getPortRoute(uint32_t port_num) const {return _ports[port_num]->route();}
    UecSinkPort* getPort(uint32_t port_num) {return _ports[port_num];}
    void setSrc(uint32_t s) { _srcaddr = s; }
    inline void setFlowId(flowid_t flow_id) { _flow.set_flowid(flow_id); }

    inline bool inPullQueue() const { return _in_pull; }
    inline bool inSlowPullQueue() const { return _in_slow_pull; }

    inline void addToPullQueue() { _in_pull = true; }
    inline void removeFromPullQueue() { _in_pull = false; }
    inline void addToSlowPullQueue() {
        _in_pull = false;
        _in_slow_pull = true;
    }
    inline void removeFromSlowPullQueue() {
        _in_pull = false;
        _in_slow_pull = false;
    }
    inline UecNIC* getNIC() const { return &_nic; }

    inline void setPCIeModel(PCIeModel* c){assert(_model_pcie); _pcie = c;}
    inline void setOversubscribedCC(OversubscribedCC* c){_receiver_cc = c;}

    uint16_t nextEntropy();

    UecSrc* getSrc() { return _src; }
    uint32_t getConfiguredMaxWnd() { return _src->configuredMaxWnd(); };

    PCIeModel* pcieModel() const{ return _pcie;}

    static mem_b _bytes_unacked_threshold;
    static uint16_t _mtus_per_pull;
    static UecBasePacket::pull_quanta _credit_per_pull;
    static int TGT_EV_SIZE;

    static bool _receiver_oversubscribed_cc; 

    // for sink logger
    inline mem_b total_received() const { return _stats.bytes_received; }
    uint32_t reorder_buffer_size();  // count is in packets

    inline UecPullPacer* pullPacer() const {return _pullPacer;}

        // ATLAHS
    AtlahsHtsimApi *_atlahs_api = nullptr;
    graph_node_properties *lgs_node; // used for logging purposes
    uint64_t send_size = 0;
    uint32_t from_sink = -1;
    uint32_t to_sink = -1;
    uint32_t tag_sink;
    uint64_t lgs_time;
    uint64_t lgs_starttime;         // only used for MSGs to identify start times
    uint64_t lgs_syncstart;

    uint64_t lgs_ts; /* this is a timestamp that determines the (original) insertion order of 
                  elements in the queue, it is increased for every new element, not for 
                  re-insertions! Needed for correctness. */
    uint64_t lgs_size;						// number of bytes to send, recv, or time to spend in loclop
    uint32_t lgs_target;					// partner for send/recv
    uint32_t lgs_host;            // owning host 
    uint32_t lgs_offset;          // for Parser (to identify schedule element)
    uint32_t lgs_tag;							// tag for send/recv
    uint32_t lgs_handle;          // handle for network layer :-/
    uint8_t lgs_proc;							// processing element for this operation
    uint8_t lgs_nic;							// network interface for this operation
    char lgs_type;							  // see below
    //void setFlowSize(uint64_t flow_size) { _flow_size = flow_size; }
    //void set_dst(uint32_t dst) { _dstaddr = dst; }
    simtime_picosec last_data_sent_time;
    void set_src(uint32_t s) { _srcaddr = s; }

   private:
    uint32_t _no_of_ports;
    vector <UecSinkPort*> _ports;
    uint32_t _srcaddr;
    UecNIC& _nic;
    UecSrc* _src;
    PacketFlow _flow;
    UecPullPacer* _pullPacer;
    UecBasePacket::seq_t _expected_epsn;
    UecBasePacket::seq_t _high_epsn;
    UecBasePacket::seq_t
        _ref_epsn;  // used for SACK bitmap calculation in spec, unused here for NOW.
    UecBasePacket::pull_quanta _retx_backlog;
    UecBasePacket::pull_quanta _latest_pull;
    UecBasePacket::pull_quanta _highest_pull_target;

    bool _in_pull;       // this tunnel is in the pull queue.
    bool _in_slow_pull;  // this tunnel is in the slow pull queue.


    //received payload bytes, used to decide when flow has finished.
    mem_b _received_bytes;
    uint16_t _accepted_bytes;

    //used to help the sender slide his window.
    uint64_t _recvd_bytes;
    //used for flow control in sender CC mode. 
    //decides whether to reduce cwnd at sender; will change dynamically based on receiver resource availability. 
    uint8_t _rcv_cwnd_pen;

    Trigger* _end_trigger;
    ModularVector<uint8_t, uecMaxInFlightPkts>
        _epsn_rx_bitmap;  // list of packets above a hole, that we've received

    uint32_t _out_of_order_count;
    bool _ack_request;

    uint16_t _entropy;

    //variables for PCIe model
    PCIeModel* _pcie;
    OversubscribedCC* _receiver_cc;

    Stats _stats;
    string _nodename;

public:
    static bool _oversubscribed_cc;
    static bool _model_pcie;
};

class UecPullPacer : public EventSource {
   public:
    enum reason {PCIE = 0, OVERSUBSCRIBED_CC = 1};

    UecPullPacer(linkspeed_bps linkSpeed,
                  double pull_rate_modifier,
                  uint16_t bytes_credit_per_pull,
                  EventList& eventList,
                  uint32_t no_of_ports);
    void doNextEvent();
    void requestPull(UecSink* sink);

    bool isActive(UecSink* sink);
    bool isIdle(UecSink* sink);

    inline linkspeed_bps linkspeed() const {return _linkspeed;}

    void updatePullRate(reason r,double relative_rate);

   private:
    list<UecSink*> _active_senders;  // TODO priorities?
    list<UecSink*> _idle_senders;    // TODO priorities?

    const simtime_picosec _time_per_quanta;
    simtime_picosec _actual_time_per_quanta;

    bool _active;
    
    double _rates[2];

    linkspeed_bps _linkspeed;
    uint16_t _bytes_credit_per_pull;



};

#endif  // UEC_H
