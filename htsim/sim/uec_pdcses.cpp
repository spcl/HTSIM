// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "uec_pdcses.h"
#include <optional>
#include <cstdint>
#include <algorithm>

bool UecPdcSes::_debug = false;
bool UecMsg::_output_completion_time = false;

UecMsg::UecMsg(UecPdcSes& pdc, msgid_t msg_id, mem_b size, bool debug): 
        _debug(debug),
        _pdc(pdc),
        _msg_id(msg_id),
        _state(MsgStatus::Init),
        _total_bytes(size),
        _sent_bytes(0),
        _recvd_bytes(0),
        _acked_bytes(0),
        _stats(),
        _sent_pkt_notrecvd(),
        _sent_pkt_notacked(),
        _pkt_size(),
        _triggers()
        {};

UecMsg::~UecMsg() {
    // TODO: Check who is keeping track of triggers
}

pair<mem_b, bool> UecMsg::getNextSegment(UecDataPacket::seq_t seq_no, mem_b mss) {
    if (_debug) {
        cout << "getNextSegment " << _pdc._debug_tag
            << " msgid " << _msg_id 
            << " seq_no " << seq_no
            << " mss " << mss
            << " _total_bytes " << _total_bytes
            << " _sent_bytes " << _sent_bytes
            << " _remaining_bytes " << getRemainingBytes()
            << endl;
    }
    assert(mss > 0);
    bool is_last_seg = false;

    mem_b segment_size = min(getRemainingBytes(), mss);

    _pkt_size.insert({seq_no, segment_size});
    _sent_bytes += segment_size;
    _sent_pkt_notrecvd.insert(seq_no);

    if (!_first_seq.has_value()) {
        assert(_sent_bytes>0);
        assert(_state==MsgStatus::Init);
        _state=MsgStatus::SentFirst;
        _first_seq.emplace(seq_no);

        _stats.start_time=EventList::getTheEventList().now();
    }

    if (getRemainingBytes() == 0) {
        assert(_sent_bytes==_total_bytes);
        assert(_state==MsgStatus::SentFirst);

        set_status(MsgStatus::SentLast);
        _last_seq.emplace(seq_no);
        is_last_seg = true;
    }

    return { segment_size, is_last_seg };
}

mem_b UecMsg::addRecvd(UecDataPacket::seq_t seq_no) {
    mem_b new_bytes = 0;
    uint32_t seq_cnt = _sent_pkt_notrecvd.count(seq_no);

    if (seq_cnt == 0) {
        // Duplicate pkt
        cout << "UecMsg::addRecvd " 
                << " " << _pdc._debug_tag
                << " msg id " << _msg_id
                << " seq no " << seq_no
                << " duplicate packet received"
                << endl;
        return 0;
    } else if (seq_cnt > 1) {
        // Multiple entries with the same seq no
        cout << "UecMsg::addRecvd " 
                << " " << _pdc._debug_tag
                << " msg id " << _msg_id
                << " ERROR "
                << "found " << _sent_pkt_notrecvd.count(seq_no)
                << " entries for seq no " << seq_no
                << endl;
        abort();
    }

    // else: seq_cnt == 1
    if (_sent_pkt_notrecvd.erase(seq_no) == 1) {
        _sent_pkt_notacked.insert(seq_no);
        new_bytes = _pkt_size.at(seq_no);
        _recvd_bytes += new_bytes;
    }

    if (_debug) {
        cout << "UecMsg::addRecvd " 
            << " " << _pdc._debug_tag
            << " msg id " << _msg_id
            << " seq no " << seq_no
            << " packet size " << _pkt_size.at(seq_no)
            << " recvd bytes " << _recvd_bytes
            << endl;
    }

    if (new_bytes > 0 and _recvd_bytes == _total_bytes) {
        assert(_state==MsgStatus::SentLast);
        assert(_sent_pkt_notrecvd.empty());

        set_status(MsgStatus::RecvdLast);
    }

    return new_bytes;
}

mem_b UecMsg::addAck(UecDataPacket::seq_t ackno) {
    mem_b cur_acked_bytes = 0;

    if (_sent_pkt_notacked.erase(ackno) == 1) {
        cur_acked_bytes += _pkt_size.at(ackno);
        _acked_bytes += cur_acked_bytes;
    }

    if (_debug) {
        cout << timeAsUs(EventList::getTheEventList().now()) 
             << " UecMsg::addAck " 
             << " " << _pdc._debug_tag
             << " msgid " << _msg_id
             << " ackno " << ackno
             << " packet size " << _pkt_size.at(ackno)
             << " acked bytes " << _acked_bytes
             << " cur acked bytes " << cur_acked_bytes
             << endl;
    }

    if (cur_acked_bytes > 0
        and _acked_bytes == _total_bytes) {
        assert(_state==MsgStatus::RecvdLast);
        assert(getRemainingBytes() == 0);
        assert(_sent_pkt_notacked.empty());

        set_status(MsgStatus::Finished);

        _stats.end_time=EventList::getTheEventList().now();

        if (_output_completion_time) {
            cout << timeAsUs(EventList::getTheEventList().now()) 
                << " " << _pdc._debug_tag
                << " msgid " << _msg_id
                << " finished"
                << " duration " << timeAsUs(_stats.end_time-_stats.start_time)
                << endl;
        }
    }

    return cur_acked_bytes;
}

optional<pair<UecDataPacket::seq_t,UecDataPacket::seq_t>> UecMsg::getSeq() {
    if (!_first_seq.has_value() || !_last_seq.has_value()) {
        return {};
    }

    return {{_first_seq.value(), _last_seq.value()}};
}

void UecMsg::set_status(MsgStatus new_status) {
    assert(_state!=new_status);
    _state = new_status;

    bool trigger = false;
    bool callback = false;
    if (_triggers[_state].has_value()) {
        _triggers[_state].value()->activate();
        trigger = true;
    }
    if (_callbacks[_state].has_value()) {
        _callbacks[_state].value()->msg_status_changed(_pdc, _msg_id, _state);

        callback = true;
    }

    if (_debug) {
        cout << timeAsUs(EventList::getTheEventList().now()) 
            << " UecMsg::set_status " << _pdc._debug_tag << " msg id " << _msg_id
            << " msg_status " << new_status
            << " trigger " << trigger
            << " callback " << callback
            << endl;
    }
}

bool UecMsg::status(MsgStatus status) {
    return _state >= status;
}

bool UecMsg::checkFinished() {
    return status(MsgStatus::Finished);
}

void UecMsg::activate() {
    mem_b new_bytes = _pdc.makeMsgEligible(this);

    _pdc.schedule_connection(new_bytes);
}

UecPdcSes::UecPdcSes(UecTransportConnection* connection,
                     EventList& eventlist,
                     mem_b mss, 
                     mem_b hdr_size,
                     string debug_tag): 
                     EventSource(eventlist, "uecPdcSes"),
                     _connection(connection),
                     _mss(mss),
                     _hdr_size(hdr_size),
                     _connection_name(),
                     _next_msg_id(1),
                     _total_pkt_bytes(0), 
                     _scheduled_pkt_bytes(0), 
                     _triggered_pkt_bytes(0), 
                     _eligible_pkt_bytes(0), 
                     _sent_pkt_bytes(0), 
                     _recvd_pkt_bytes(0), 
                     _acked_pkt_bytes(0), 
                     _debug_tag(debug_tag),
                     _max_contiguous_ack(),
                     _min_seq_no(), 
                     _max_seq_no(), 
                     _msgs_queue_scheduled(),
                     _msgs_queue_triggered(),
                     _msgs_queue_eligible(),
                     _msgs_in_flight(),
                     _msgs_complete(),
                     _ctrl_seq(),
                     _seq_to_msg(), 
                     _msgs()
                     {
    connection->makeReusable(this);
};


UecPdcSes::~UecPdcSes() {
    for (auto [_, msg]: _msgs) {
        delete msg;
    }
}

UecMsg::msgid_t UecPdcSes::get_next_msg_id()
{
    // I know about i ++, but I think this makes the intent clearer.
    auto next_msg_id = _next_msg_id;
    _next_msg_id += 1;
    return next_msg_id;
}

UecMsg* UecPdcSes::enque(mem_b size, 
                      optional<simtime_picosec> scheduled_time, 
                      bool schedule_event) {
    UecMsg::msgid_t msg_id = get_next_msg_id();
    assert(!schedule_event or scheduled_time.has_value());
    UecMsg* msg = new UecMsg(*this, msg_id, size, _debug);

    if (scheduled_time.has_value()) {
        simtime_picosec schedule_ts = 0;
        if (scheduled_time.value() == 0) {
            schedule_ts = eventlist().now();
        } else if (scheduled_time.value() < eventlist().now()) {
            cout << "ERROR: Message due time is in the past! "
                 << _debug_tag
                 << " msgid " << msg_id
                 << " now " << eventlist().now()
                 << " scheduled_time " << scheduled_time.value()
                 << endl;
            abort();
        } else {
            schedule_ts = scheduled_time.value();
        }
        _msgs_queue_scheduled.insert({schedule_ts, msg});
        _scheduled_pkt_bytes += calc_packeted_size(msg->size());

        if (schedule_event) {
            if (_events_scheduled.find(schedule_ts) == _events_scheduled.end()) {
                eventlist().sourceIsPending(*this, schedule_ts);
                _events_scheduled.insert(schedule_ts);
            }
        }
    } else {
        _msgs_queue_triggered.insert(msg);
        _triggered_pkt_bytes += calc_packeted_size(size);
    }
    _total_pkt_bytes += calc_packeted_size(size);

    _msgs.insert({msg->msg_id(), msg});
    return msg;
}

void UecPdcSes::doNextEvent() {
    simtime_picosec now = eventlist().now();

    mem_b new_bytes_packetized = updateScheduledMsgs(now);

    if (_debug) {
        cout << timeAsUs(now) 
             << " UecPdc::doNextEvent" 
             << " " << _debug_tag
             << " new_bytes_packetized " << new_bytes_packetized
             << endl;
    }

    // Make sure we had actually something scheduled.
    assert(new_bytes_packetized > 0);

    auto evt = _events_scheduled.find(now);
    assert(evt != _events_scheduled.end());
    _events_scheduled.erase(evt);

    schedule_connection(new_bytes_packetized);
}

mem_b UecPdcSes::getNextPacket(UecDataPacket::seq_t seq_no) {
    assert(_seq_to_msg.count(seq_no)==0);
    assert(!_max_seq_no.has_value() || seq_no > _max_seq_no);
    mem_b pkt_size = 0;

    if (!_cur_msg.has_value()) {
        if (_msgs_queue_eligible.size() > 0) {
            _cur_msg.emplace(_msgs_queue_eligible.front());
            _msgs_in_flight.insert(_cur_msg.value());
            _msgs_queue_eligible.pop_front();
        } else {
            return 0;
        }
    }

    auto [segment_size, last_segment] = _cur_msg.value()->getNextSegment(seq_no, _mss);
    assert(segment_size > 0);
    pkt_size = segment_size + _hdr_size;
    _sent_pkt_bytes += pkt_size;

    _seq_to_msg.insert({seq_no, _cur_msg.value()});

    if (_debug) {
        cout << timeAsUs(eventlist().now()) 
             << " UecPdc::getNextSegment" 
             << " " << _debug_tag
             << " seq_no " << seq_no 
             << " msg id " << _cur_msg.value()->msg_id()
             << " _msgs_queue_eligible " << _msgs_queue_eligible.size()
             << " _msgs_in_flight " << _msgs_in_flight.size()
             << endl;
    }

    if (last_segment) {
        _cur_msg.reset();
    }

    if (!_min_seq_no.emplace()) {
        _min_seq_no.emplace(seq_no);
    }
    _max_seq_no = seq_no;

    return pkt_size;
}

void UecPdcSes::notifyCtrlSeqno(UecDataPacket::seq_t seq_no) {
    _ctrl_seq.insert(seq_no);
    _max_seq_no = seq_no;
    if (_debug) {
        cout << timeAsUs(eventlist().now()) 
             << " UecPdc::notifyCtrlSeqno" 
             << " " << _debug_tag
             << " seq_no " << seq_no 
             << endl;
    }
}


void UecPdcSes::addCumAck(UecDataPacket::seq_t cum_ack) {
    /*
    It is not our job to figure out if an ack is a duplicate
    or valid. We just do completion tracking.

    The cumack is the next expected seq_no, hence the cum_ack
    seq number itself does not exist yet.
    */
    mem_b cur_acked_msg_bytes = 0;
    mem_b cur_acked_pkt_bytes = 0;
    mem_b sum_acked_msg_bytes = 0;
    mem_b sum_acked_pkt_bytes = 0;
    map<UecDataPacket::seq_t, UecMsg*>::iterator seq_it;

    for (seq_it=_seq_to_msg.begin(); seq_it!=_seq_to_msg.end();) {

        if (seq_it->first >= cum_ack) {
            break;
        }

        // Seq numbers of control packets
        if (_ctrl_seq.find(seq_it->first) != _ctrl_seq.end()) {
            ++seq_it;
            continue;
        }

        cur_acked_msg_bytes = seq_it->second->addAck(seq_it->first);
        if (cur_acked_msg_bytes > 0) {
            cur_acked_pkt_bytes = cur_acked_msg_bytes + _hdr_size;
        } else {
            cur_acked_pkt_bytes = cur_acked_msg_bytes;
        }
        sum_acked_msg_bytes += cur_acked_msg_bytes;
        sum_acked_pkt_bytes += cur_acked_pkt_bytes;

        if (cur_acked_pkt_bytes > 0) {
            if (seq_it->second->checkFinished()) {
                _msgs_in_flight.erase(seq_it->second);
                _msgs_complete.push_back(seq_it->second);
            }
            seq_it = _seq_to_msg.erase(seq_it);
        } else {
            ++seq_it;
        }
    }

    _acked_pkt_bytes += sum_acked_pkt_bytes;

    _max_contiguous_ack.emplace(cum_ack);
    if (_seq_to_msg.empty()) {
        _min_seq_no.reset();
        _max_seq_no.reset();
    } else {
        _min_seq_no.emplace(_seq_to_msg.begin()->first);
    }

    if (_debug) {
        cout << timeAsUs(eventlist().now()) 
            << " UecPdc::addCumAck"
            << " " << _debug_tag
            << " last_ack " << _max_contiguous_ack.value_or(-1)
            << " cum ack " << cum_ack
            << " acked msg bytes " << sum_acked_msg_bytes
            << " acked pkt bytes " << sum_acked_pkt_bytes
            << endl;
    }
}

void UecPdcSes::addSAck(UecDataPacket::seq_t ackno) {
    /*
    Don't try to figure out if the ack is a duplicate
    or valid. We just do completion tracking.
    */

    assert(ackno <= _max_seq_no);

    // Seq numbers of control packets
    if (_ctrl_seq.find(ackno) != _ctrl_seq.end()) {
        return;
    }

    mem_b acked_msg_bytes = 0;
    mem_b acked_pkt_bytes = 0;

    if (_seq_to_msg.count(ackno) == 0) {
        return;
    }

    UecMsg* cur_msg = _seq_to_msg.at(ackno);
    _seq_to_msg.erase(_seq_to_msg.find(ackno));

    acked_msg_bytes = cur_msg->addAck(ackno);
    if (acked_msg_bytes > 0) {
        acked_pkt_bytes += acked_msg_bytes + _hdr_size;
    }
    _acked_pkt_bytes += acked_pkt_bytes;

    if (cur_msg->checkFinished()) {
        _msgs_in_flight.erase(cur_msg);
        _msgs_complete.push_back(cur_msg);
    }

    if (_seq_to_msg.empty()) {
        _min_seq_no.reset();
        _max_seq_no.reset();
    } else if (ackno == _min_seq_no) {
        _min_seq_no.emplace(_seq_to_msg.begin()->first);
    } else if (ackno == _max_seq_no) {
        _min_seq_no.emplace(std::prev(_seq_to_msg.end())->first);
    }

    if (_debug) {
        cout << timeAsUs(eventlist().now())
            << " UecPdc::addSAck"
            << " " << _debug_tag
            << " msgid " << cur_msg->msg_id()
            << " ackno " << ackno
            << " acked msg bytes " << acked_msg_bytes
            << " acked pkt bytes " << acked_pkt_bytes
            << endl;
    }
}

void UecPdcSes::addRecvd(UecDataPacket::seq_t seq_no) {
    // We don't have to figure out if a duplicate or old
    // seq_no is a problem, just ignore and let uec.cpp
    // handle the rest.
    mem_b recvd_pkt_bytes = 0;
    if (_seq_to_msg.find(seq_no) != _seq_to_msg.end()) {
        UecMsg* msg = _seq_to_msg.at(seq_no);

        recvd_pkt_bytes = msg->addRecvd(seq_no);
        if (recvd_pkt_bytes > 0) {
            recvd_pkt_bytes += _hdr_size;
        }

        _recvd_pkt_bytes += recvd_pkt_bytes;
    }
    if (_debug) {
        cout << timeAsUs(eventlist().now())
            << " UecPdc::addRecvd "
            << " " << _debug_tag
            << " seq_no " << seq_no
            << " newly recvd pkt bytes " << recvd_pkt_bytes
            << " recvd pkt bytes " << _recvd_pkt_bytes
            << endl;
    }
}

optional<pair<UecDataPacket::seq_t,UecDataPacket::seq_t>> 
UecPdcSes::getMsgSeq(UecMsg::msgid_t msg_id) {
    return _msgs.at(msg_id)->getSeq();
}

bool UecPdcSes::checkDoneSending() {
    bool cur_done_sending; 
    if (_cur_msg.has_value()) {
        cur_done_sending = _cur_msg.value()->status(UecMsg::MsgStatus::SentLast)
            and _msgs_queue_eligible.size() == 0; 
    } else {
        cur_done_sending = true;
    }

    bool retval = _sent_pkt_bytes == _eligible_pkt_bytes;
    if (retval){
        assert(_msgs_queue_eligible.empty() && cur_done_sending);
    }

    if (_debug) {
        cout << timeAsUs(eventlist().now())
            << " checkDoneSending " << _connection_name
            << " " << _debug_tag
            << " _cur_msg " << cur_done_sending
            << " _msgs_queue_eligible " << _msgs_queue_eligible.size()
            << " total bytes " << _eligible_pkt_bytes
            << " sent bytes " << _sent_pkt_bytes 
            << " recv bytes " << _recvd_pkt_bytes 
            << " acked bytes " << _acked_pkt_bytes 
            << " retval " << (_msgs_queue_eligible.empty() && cur_done_sending)
            << " _msgs_queue_scheduled " << _msgs_queue_scheduled.size()
            << " bytes scheduled " << _scheduled_pkt_bytes
            << " _msgs_queue_triggered " << _msgs_queue_triggered.size()
            << " bytes triggered " << _triggered_pkt_bytes
            << endl;
    }
    return retval;
}

bool UecPdcSes::checkFinished() {
    bool retval = _acked_pkt_bytes == _eligible_pkt_bytes;
    assert(retval == (_msgs_in_flight.empty() and _msgs_queue_eligible.empty()));

    if (retval) {
        if (_cur_msg.has_value()) {
            assert(_cur_msg.value()->checkFinished());
            if (!_max_contiguous_ack.has_value() || _max_contiguous_ack.value() < _cur_msg.value()->getLastSeqNo()) {
                _max_contiguous_ack = _cur_msg.value()->getLastSeqNo();
            }
        } 

        // seq_to_msg might still contain control packets
        // ignore for now.
        // assert(_seq_to_msg.empty() && checkDoneSending());
        assert(checkDoneSending());
        assert(_acked_pkt_bytes == _sent_pkt_bytes);
        assert(_acked_pkt_bytes == _recvd_pkt_bytes);
        assert(_msgs.size() == _msgs_complete.size());
        assert(_msgs_in_flight.empty() and _msgs_queue_eligible.empty());
    }

    if (_debug) {
        cout << timeAsUs(eventlist().now())
            << " checkFinished " << _connection_name 
            << " " << _debug_tag
            << " #(_seq_to_msg) " << _seq_to_msg.size()
            << " _msgs_queue_eligible " << _msgs_queue_eligible.size()
            << " _eligible_bytes " << _eligible_pkt_bytes
            << " _sent_bytes " << _sent_pkt_bytes
            << " _recvd_bytes " << _recvd_pkt_bytes
            << " _acked_bytes " << _acked_pkt_bytes
            << " _msgs_queue_scheduled " << _msgs_queue_scheduled.size()
            << " bytes scheduled " << _scheduled_pkt_bytes
            << " _msgs_queue_triggered " << _msgs_queue_triggered.size()
            << " bytes triggered " << _triggered_pkt_bytes
            << " retval " << retval
            << endl;
    }

    return retval;
}

bool UecPdcSes::isTotallyFinished() {
    return checkFinished() && _total_pkt_bytes == _acked_pkt_bytes;
}

uint32_t UecPdcSes::getMsgCompleted() {
    return _msgs_complete.size();
}

mem_b UecPdcSes::eligiblePktSize() {
    return _eligible_pkt_bytes;
}

UecMsg* UecPdcSes::getMsg(UecMsg::msgid_t msg_id) {
    return _msgs.at(msg_id);
}

UecMsg::msgid_t UecPdcSes::getMsgId(UecDataPacket::seq_t seq_no) {
    return _seq_to_msg.at(seq_no)->msg_id();
}

mem_b UecPdcSes::updateScheduledMsgs(simtime_picosec now) {
    bool scheduled_msg_found = false;
    mem_b new_bytes_pkt = 0;
    mem_b total_new_bytes_pkt = 0;

    auto msg_it = _msgs_queue_scheduled.begin(); 
    if (_debug) {
        cout << timeAsUs(eventlist().now())
             << " UecPdc::updateScheduleMsgs"
             << " " << _debug_tag;
    }

    while (msg_it != _msgs_queue_scheduled.end()) {
        if (msg_it->first <= now) {
            // We should get a call for every scheduled time.
            // If now, figure out what went wrong.
            assert(msg_it->first==now);
            assert(msg_it->second != nullptr);

            if (_debug) cout << " msg " << msg_it->second->msg_id();

            _msgs_queue_eligible.push_back(msg_it->second);

            new_bytes_pkt = calc_packeted_size(msg_it->second->size());
            total_new_bytes_pkt += new_bytes_pkt;
            _eligible_pkt_bytes += new_bytes_pkt;
            _scheduled_pkt_bytes -= new_bytes_pkt;
            msg_it = _msgs_queue_scheduled.erase(msg_it);

            scheduled_msg_found = true;
        } else {
            break;
        }
    }
    if (_debug) cout << endl;

    assert(scheduled_msg_found);

    return total_new_bytes_pkt;
}

mem_b UecPdcSes::makeMsgEligible(UecMsg* msg) {
    assert(_msgs_queue_triggered.count(msg) > 0);
    assert(find(_msgs_queue_eligible.begin(), 
                _msgs_queue_eligible.end(),
                msg) == _msgs_queue_eligible.end());
    
    _msgs_queue_eligible.push_back(msg);
    _msgs_queue_triggered.erase(msg);

    mem_b new_bytes_pkt = calc_packeted_size(msg->size());
    _eligible_pkt_bytes += new_bytes_pkt;
    _triggered_pkt_bytes -= new_bytes_pkt;

    if (_debug) {
        cout << timeAsUs(eventlist().now()) 
            << " UecPdc::makeMsgEligible"
            << " " << _debug_tag
            << " msg " << msg->msg_id()
            << endl;
    }

    return new_bytes_pkt;
}

void UecPdcSes::schedule_connection(mem_b new_bytes) {
    if (_debug) {
        cout << timeAsUs(eventlist().now())
            << " UecPdc::schedule_connection"
            << " " << _debug_tag
            << " new bytes " << new_bytes;
    }

    if (!_connection->hasStarted()) {
        // This must be done after the checks
        _connection->addToBacklog(new_bytes);
        if (_debug) cout << " startFlow()" << endl;
        _connection->startConnection();
    } else if (!_connection->isActivelySending()) {
        // This must be done after the checks
        _connection->addToBacklog(new_bytes);
        if (_debug) cout << " continueFlow()" << endl;
        _connection->continueConnection();
    } else {
        _connection->addToBacklog(new_bytes);
        if (_debug) cout << " NOOP" << endl;
    }
}