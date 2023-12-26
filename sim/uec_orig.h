// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

#ifndef UEC_H
#define UEC_H

/*
 * A UEC source and sink
 */
#include "config.h"
#include "eventlist.h"
#include "fairpullqueue.h"
//#include "datacenter/logsim-interface.h"
#include "network.h"
#include "uecpacket.h"
#include <functional>
#include <list>
#include <map>

class UecSink;
// class LogSimInterface;

class SentPacket {
  public:
    SentPacket(simtime_picosec t, uint64_t s, bool a, bool n, bool to)
            : timer{t}, seqno{s}, acked{a}, nacked{n}, timedOut{to} {}
    SentPacket(const SentPacket &sp)
            : timer{sp.timer}, seqno{sp.seqno}, acked{sp.acked},
              nacked{sp.nacked}, timedOut{sp.timedOut} {}
    simtime_picosec timer;
    uint64_t seqno;
    bool acked;
    bool nacked;
    bool timedOut;
};

class UecSrc : public PacketSink, public EventSource {
    friend class UecSink;

  public:
    UecSrc(UecLogger *logger, TrafficLogger *pktLogger, EventList &eventList,
           uint64_t rtt, uint64_t bdp, uint64_t queueDrainTime, int hops);
    // UecSrc(UecLogger *logger, TrafficLogger* pktLogger, EventList& eventList,
    // uint64_t rtt=timeFromUs(5.25), uint64_t bdp=63000);
    ~UecSrc();

    virtual void doNextEvent() override;

    void receivePacket(Packet &pkt) override;
    const string &nodename() override;

    virtual void connect(const Route *routeout, const Route *routeback,
                         UecSink &sink, simtime_picosec startTime);
    void startflow();
    void set_paths(vector<const Route *> *rt);
    void map_entropies();

    void setCwnd(uint64_t cwnd) { _cwnd = cwnd; };
    void setReuse(bool reuse) { _use_good_entropies = reuse; };
    void setIgnoreEcnAck(bool ignore_ecn_ack) {
        _ignore_ecn_ack = ignore_ecn_ack;
    };
    void setIgnoreEcnData(bool ignore_ecn_data) {
        _ignore_ecn_data = ignore_ecn_data;
    };
    void setNumberEntropies(int num_entropies) {
        _num_entropies = num_entropies;
    };
    void setHopCount(int hops) {
        _hop_count = hops;
        printf("Hop Count is %d\n", hops);
    };
    void setFlowSize(uint64_t flow_size) { _flow_size = flow_size; }
    void setKeepAcksInTargetRtt(bool keep) { _target_based_received = keep; }
    // void setLgs(LogSimInterface *lgs) { _lgs = lgs; };
    // void setUsingLgs(bool val) { _using_lgs = val; };
    void enableTrimming(bool enable) { _trimming_enabled = enable; };

    uint64_t getCwnd() { return _cwnd; };
    uint64_t getHighestSent() { return _highest_sent; }
    uint32_t getUnacked() { return _unacked; }
    uint32_t getConsecutiveLowRtt() { return _consecutive_low_rtt; }
    uint64_t getLastAcked() { return _last_acked; }
    uint32_t getReceivedSize() { return _received_ecn.size(); }
    uint32_t getRto() { return _rto; }
    bool supportsTrimming() { return _trimming_enabled; }
    std::size_t getEcnInTargetRtt();

    void set_traffic_logger(TrafficLogger *pktlogger);
    static void set_queue_type(std::string value) { queue_type = value; }
    static void set_alogirthm(std::string value) { algorithm_type = value; }
    static void set_fast_drop(bool value) { use_fast_drop = value; }
    static void set_fast_drop_rtt(int value) { fast_drop_rtt = value; }

    static void set_do_jitter(bool value) { do_jitter = value; }
    static void set_do_exponential_gain(bool value) {
        do_exponential_gain = value;
    }
    static void set_use_fast_increase(bool value) { use_fast_increase = value; }
    static void set_use_super_fast_increase(bool value) {
        use_super_fast_increase = value;
    }
    static void set_gain_value_med_inc(double value) {
        exp_gain_value_med_inc = value;
    }
    static void set_jitter_value_med_inc(double value) {
        jitter_value_med_inc = value;
    }
    static void set_delay_gain_value_med_inc(double value) {
        delay_gain_value_med_inc = value;
    }
    static void set_target_rtt_percentage_over_base(int value) {
        target_rtt_percentage_over_base = value;
    }

    static void set_y_gain(double value) { y_gain = value; }
    static void set_x_gain(double value) { x_gain = value; }
    static void set_z_gain(double value) { z_gain = value; }
    static void set_w_gain(double value) { w_gain = value; }
    static void set_bonus_drop(double value) { bonus_drop = value; }
    static void set_buffer_drop(double value) { buffer_drop = value; }

    void set_flow_over_hook(std::function<void(const Packet &)> hook) {
        f_flow_over_hook = hook;
    }

    virtual void rtx_timer_hook(simtime_picosec now, simtime_picosec period);

    // should really be private, but loggers want to see:
    uint64_t _highest_sent; // seqno is in bytes
    bool need_fast_drop = false;
    uint64_t _packets_sent;
    uint64_t _new_packets_sent;
    uint64_t _rtx_packets_sent;
    uint64_t _acks_received;
    uint64_t _nacks_received;
    uint64_t _pulls_received;
    uint64_t _implicit_pulls;
    uint64_t _bounces_received;
    uint32_t _cwnd;
    uint32_t acked_bytes = 0;
    uint32_t good_bytes = 0;
    uint32_t saved_acked_bytes = 0;
    uint32_t saved_good_bytes = 0;
    uint32_t saved_trimmed_bytes = 0;
    uint32_t last_decrease = 0;
    uint32_t drop_amount = 0;
    uint32_t count_total_ecn = 0;
    uint32_t count_total_ack = 0;
    uint64_t _last_acked;
    uint32_t _flight_size;
    uint32_t _acked_packets;
    uint64_t _flow_start_time;
    uint64_t _next_check_window;
    uint64_t next_window_end = 0;
    bool update_next_window = true;
    bool _start_timer_window = true;
    bool stop_decrease = false;
    bool fast_drop = false;
    int ignore_for = 0;
    int count_received = 0;
    int count_ecn_in_rtt = 0;
    int count_trimmed_in_rtt = 0;
    int counter_consecutive_good_bytes;
    bool increasing = false;
    int total_routes;
    int routes_changed = 0;
    int exp_avg_bts = 0;
    int exp_avg_route = 0;
    double alpha_route = 0.0625;

    // Custom Parameters
    static std::string queue_type;
    static std::string algorithm_type;
    static bool use_fast_drop;
    static int fast_drop_rtt;
    bool was_zero_before = false;
    double ideal_x = 0;
    static bool do_jitter;
    static bool do_exponential_gain;
    static bool use_fast_increase;
    static bool use_super_fast_increase;
    static double exp_gain_value_med_inc;
    static double jitter_value_med_inc;
    static double delay_gain_value_med_inc;
    static int target_rtt_percentage_over_base;

    static double y_gain;
    static double x_gain;
    static double z_gain;
    static double w_gain;
    static double bonus_drop;
    static double buffer_drop;

  private:
    uint32_t _unacked;
    uint32_t _effcwnd;
    uint16_t _mss;
    uint64_t _flow_size;
    uint64_t _rtt;
    uint64_t _rto;
    uint64_t _rto_margin;
    uint64_t _rtx_timeout;
    uint64_t _maxcwnd;
    uint16_t _crt_path;
    uint64_t target_window;
    // LogSimInterface *_lgs;
    bool _flow_finished = false;

    bool _rtx_timeout_pending;
    bool _rtx_pending;

    // new CC variables
    uint64_t _target_rtt;
    uint64_t _base_rtt;
    uint64_t _bdp;
    uint32_t _consecutive_low_rtt;
    uint32_t _consecutive_no_ecn;
    uint64_t _ignore_ecn_until = 0;
    uint64_t previous_window_end = 0;
    bool _target_based_received;
    bool _using_lgs = false;
    int consecutive_nack = 0;
    bool ecn_last_rtt = false;
    int trimmed_last_rtt = 0;
    int consecutive_good_medium = 0;

    // SentPackets _sent_packets;
    uint64_t _highest_data_seq;
    uint64_t t_last_decrease = 0;
    uint64_t t_last_fairness = 0;
    uint64_t t_last_clear_byte = 0;
    bool can_decrease = false;
    bool can_clear_byte = false;
    bool can_fairness = false;
    uint64_t avg_rtt;
    int rx_count = 0;
    int achieved_bdp = 0;
    UecLogger *_logger;
    UecSink *_sink;

    const Route *_route;
    vector<const Route *> _paths;
    PacketFlow _flow;

    string _nodename;
    std::function<void(const Packet &p)> f_flow_over_hook;

    list<std::tuple<simtime_picosec, bool, uint64_t, uint64_t>>
            _received_ecn; // list of packets received
    vector<SentPacket> _sent_packets;
    unsigned _nack_rtx_pending;
    vector<tuple<simtime_picosec, uint64_t, uint64_t, uint64_t, uint64_t,
                 uint64_t>>
            _list_rtt;
    vector<pair<simtime_picosec, uint64_t>> _list_cwd;
    vector<pair<simtime_picosec, uint64_t>> _list_unacked;
    vector<pair<simtime_picosec, uint64_t>> _list_acked_bytes;
    vector<pair<simtime_picosec, uint64_t>> _list_ecn_rtt;
    vector<pair<simtime_picosec, uint64_t>> _list_trimmed_rtt;
    vector<pair<simtime_picosec, uint64_t>> _list_nack;
    vector<pair<simtime_picosec, uint64_t>> _list_bts;
    vector<pair<simtime_picosec, uint64_t>> _list_fast_increase_event;
    vector<pair<simtime_picosec, uint64_t>> _list_medium_increase_event;
    vector<pair<simtime_picosec, uint64_t>> _list_fast_decrease;
    vector<pair<simtime_picosec, int>> us_to_cs;
    vector<pair<simtime_picosec, int>> ls_to_us;

    vector<const Route *> _good_entropies;
    bool _use_good_entropies;
    bool _ignore_ecn_ack;
    bool _ignore_ecn_data;
    std::size_t _max_good_entropies;
    std::size_t _next_good_entropy;
    std::vector<int> _entropy_array;
    int _num_entropies = -1;
    int current_entropy = 0;
    bool _enableDistanceBasedRtx;
    bool _trimming_enabled;
    bool _bts_enabled = true;
    int _hop_count;

    vector<pair<simtime_picosec, int>> count_case_1;
    vector<pair<simtime_picosec, int>> count_case_2;
    vector<pair<simtime_picosec, int>> count_case_3;
    vector<pair<simtime_picosec, int>> count_case_4;

    void send_packets();
    void do_fast_drop(bool);
    uint64_t get_unacked();

    void adjust_window(simtime_picosec ts, bool ecn, simtime_picosec rtt);
    uint32_t medium_increase(simtime_picosec);
    void fast_increase();
    bool no_ecn_last_target_rtt();
    bool no_rtt_over_target_last_target_rtt();
    bool ecn_congestion();
    void drop_old_received();
    const Route *get_path();
    void mark_received(UecAck &pkt);
    void add_ack_path(const Route *rt);
    bool resend_packet(std::size_t i);
    void retransmit_packet();
    void processAck(UecAck &pkt, bool);
    std::size_t get_sent_packet_idx(uint32_t pkt_seqno);

    void update_rtx_time();
    void reduce_cwnd(uint64_t amount);
    void processNack(UecNack &nack);
    void processBts(UecPacket *nack);
    void reduce_unacked(uint64_t amount);
    void apply_timeout_penalty();
};

class UecSink : public PacketSink, public DataReceiver {
    friend class UecSrc;

  public:
    UecSink();

    void receivePacket(Packet &pkt) override;
    const string &nodename() override;

    uint64_t cumulative_ack() override;
    uint32_t drops() override;
    void connect(UecSrc &src, const Route &route);
    void set_paths(vector<const Route *> *rt);
    uint32_t from, to, tag;

  private:
    UecAck::seq_t _cumulative_ack;
    uint64_t _packets;
    uint32_t _drops;
    string _nodename;
    list<UecAck::seq_t> _received; // list of packets received OOO
    uint16_t _crt_path;
    const Route *_route;
    vector<const Route *> _paths;
    UecSrc *_src;

    void send_ack(simtime_picosec ts, bool marked, UecAck::seq_t seqno,
                  UecAck::seq_t ackno, const Route *rt, const Route *inRoute);
    void send_nack(simtime_picosec ts, bool marked, UecAck::seq_t seqno,
                   UecAck::seq_t ackno, const Route *rt);
    bool already_received(UecPacket &pkt);
};

class UecRtxTimerScanner : public EventSource {
  public:
    UecRtxTimerScanner(simtime_picosec scanPeriod, EventList &eventlist);
    void doNextEvent();
    void registerUec(UecSrc &uecsrc);

  private:
    simtime_picosec _scanPeriod;
    simtime_picosec _lastScan;
    typedef list<UecSrc *> uecs_t;
    uecs_t _uecs;
};

#endif
