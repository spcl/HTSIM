// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "fairpullqueue.h"
#include <cassert>

// Define once in exactly one TU
int _packets_per_burst = 1;

// =============================== BasePullQueue ===============================

template <class PullPkt>
BasePullQueue<PullPkt>::BasePullQueue() : _pull_count(0), _preferred_flow(-1) {}

// =============================== FifoPullQueue ===============================

template <class PullPkt>
FifoPullQueue<PullPkt>::FifoPullQueue() {}

template <class PullPkt>
void FifoPullQueue<PullPkt>::enqueue(PullPkt& pkt, int /*priority*/) {
    if (this->_preferred_flow >= 0 && pkt.flow_id() == (packetid_t)this->_preferred_flow) {
        typename std::list<PullPkt*>::iterator it = _pull_queue.begin();
        while (it != _pull_queue.end() &&
               ((PullPkt*)(*it))->flow_id() == (packetid_t)this->_preferred_flow)
            ++it;
        _pull_queue.insert(it, &pkt);
    } else {
        _pull_queue.push_front(&pkt);
    }
    this->_pull_count++;
    assert((size_t)this->_pull_count == _pull_queue.size());
}

template <class PullPkt>
PullPkt* FifoPullQueue<PullPkt>::dequeue() {
    if (this->_pull_count == 0) return 0;
    PullPkt* packet = _pull_queue.back();
    _pull_queue.pop_back();
    this->_pull_count--;
    assert(this->_pull_count >= 0 && (size_t)this->_pull_count == _pull_queue.size());
    return packet;
}

template <class PullPkt>
void FifoPullQueue<PullPkt>::flush_flow(flowid_t flow_id, int /*priority*/) {
    typename std::list<PullPkt*>::iterator it = _pull_queue.begin();
    while (it != _pull_queue.end()) {
        PullPkt* pull = *it;
        if (pull->flow_id() == flow_id && pull->type() == NDPPULL) {
            pull->free();
            it = _pull_queue.erase(it);
            this->_pull_count--;
        } else {
            ++it;
        }
    }
    assert((size_t)this->_pull_count == _pull_queue.size());
}

// =============================== FairPullQueue (Option A, header-unchanged) ==

template <class PullPkt>
FairPullQueue<PullPkt>::FairPullQueue() {
    _current_queue = _queue_map.end();
    _scheduled     = 0;
}

// Keep only active (non-empty) flows in _queue_map
template <class PullPkt>
void FairPullQueue<PullPkt>::enqueue(PullPkt& pkt, int /*priority*/) {
    CircularBuffer<PullPkt*>* q;
    auto it = _queue_map.find(pkt.flow_id());
    if (it == _queue_map.end()) {
        q = new CircularBuffer<PullPkt*>();
        it = _queue_map.insert(std::make_pair(pkt.flow_id(), q)).first;
        if (_queue_map.size() == 1) {
            _current_queue = _queue_map.begin();
            _scheduled     = 0;
        }
    } else {
        q = it->second;
    }

    PullPkt* p = &pkt;   // make an lvalue (pointer variable)
    q->push(p);          // push(T&)

    this->_pull_count++;
}

template <class PullPkt>
PullPkt* FairPullQueue<PullPkt>::dequeue() {
    if (this->_pull_count == 0 || _queue_map.empty()) return 0;

    if (_current_queue == _queue_map.end()) _current_queue = _queue_map.begin();

    CircularBuffer<PullPkt*>* q = _current_queue->second;

    // Pop one packet
    PullPkt* packet = q->pop();
    this->_pull_count--;
    _scheduled++;

    if (q->empty()) {
        // Erase drained flow so we never scan empties
        auto to_erase = _current_queue++;
        delete q;
        _queue_map.erase(to_erase);
        _scheduled = 0;
        if (_current_queue == _queue_map.end() && !_queue_map.empty())
            _current_queue = _queue_map.begin();
    } else if (_scheduled >= _packets_per_burst) {
        // Move to next active flow (round-robin)
        ++_current_queue;
        _scheduled = 0;
        if (_current_queue == _queue_map.end()) _current_queue = _queue_map.begin();
    }

    return packet;
}

template <class PullPkt>
void FairPullQueue<PullPkt>::flush_flow(flowid_t flow_id, int /*priority*/) {
    auto it = _queue_map.find(flow_id);
    if (it == _queue_map.end()) return;

    CircularBuffer<PullPkt*>* q = it->second;

    while (!q->empty()) {
        PullPkt* packet = q->pop();
        packet->free();
        this->_pull_count--;
    }

    // Advance RR pointer if we’re erasing the current one
    if (_current_queue == it) ++_current_queue;

    delete q;
    _queue_map.erase(it);

    if (_queue_map.empty()) {
        _current_queue = _queue_map.end();
        _scheduled     = 0;
    } else if (_current_queue == _queue_map.end()) {
        _current_queue = _queue_map.begin();
        _scheduled     = 0;
    }
}

// ---- helpers (unchanged signatures) ----------------------------------------

template <class PullPkt>
bool FairPullQueue<PullPkt>::queue_exists(const PullPkt& pkt) {
    return _queue_map.find(pkt.flow_id()) != _queue_map.end();
}

template <class PullPkt>
CircularBuffer<PullPkt*>* FairPullQueue<PullPkt>::find_queue(const PullPkt& pkt) {
    auto i = _queue_map.find(pkt.flow_id());
    if (i == _queue_map.end()) return 0;
    return i->second;
}

template <class PullPkt>
CircularBuffer<PullPkt*>* FairPullQueue<PullPkt>::create_queue(const PullPkt& pkt) {
    CircularBuffer<PullPkt*>* new_queue = new CircularBuffer<PullPkt*>;
    auto res = _queue_map.insert(std::make_pair(pkt.flow_id(), new_queue));
    if (_queue_map.size() == 1) {
        _current_queue = _queue_map.begin();
        _scheduled     = 0;
    }
    return res.first->second;
}

// =============================== Explicit instantiations =====================

template class BasePullQueue<Packet>;
template class FifoPullQueue<Packet>;
template class FairPullQueue<Packet>;
