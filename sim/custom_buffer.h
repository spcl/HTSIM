// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef CUST_BUF_H
#define CUST_BUF_H

/*
 * A resizable circular buffer intended to replace a List as a queue
 * structure where we don't care about reclaiming space afterwards
 */

#include <cassert>
#include <vector>

using std::size_t;

template <typename T> class CustomBuffer {
  public:
    CustomBuffer() {
        _count = 0;
        _next_push = 0;
        _next_pop = 0;
        _size = 8; // initial size; we'll resize if needed
        _queue.resize(_size);
    }
    CustomBuffer(int starting_size) {
        _count = 0;
        _next_push = 0;
        _next_pop = 0;
        _size = starting_size; // initial size; we'll resize if needed
        _queue.resize(_size);
    }

    void push(T &item) { _queue.push_back(item); }

    T &pop() {
        // validate();
        assert(_count > 0);
        int old_index = _next_pop;
        _next_pop = (_next_pop + 1) % _size;
        _count--;
        // validate();
        return _queue[old_index];
    }

    T &peek() { return _queue[_next_pop]; }

    T &pop_front() {
        // validate();
        assert(_count > 0);
        int old_index = (_next_push + _size - 1) % _size;
        _next_push = old_index;
        _count--;
        // validate();
        return _queue[old_index];
    }

    T &back() { return _queue.back(); }

    bool empty() { return _queue.empty(); }
    int size() { return _queue.size(); }
    std::vector<T> _queue;
    int _next_push, _next_pop, _count, _size, max_size;
};

#endif
