#ifndef CIRCULARBUFFERREPS_H
#define CIRCULARBUFFERREPS_H

#include <iostream>
#include <stdexcept>

template <typename T> class CircularBufferREPS {
  private:
    struct Element {
        T value;
        bool isValid;

        Element() : value(T()), isValid(false) {}
    };

    static const int SIZE = 10; // Size of the circular buffer
    Element buffer[SIZE];       // Array to hold the buffer elements
    int head;                   // Points to the next element to be written
    int tail;                   // Points to the next element to be read
    int count;                  // Number of elements in the buffer
    int head_forzen_mode = 0;
    int number_fresh_entropies = 0;
    bool frozen_mode = false;

  public:
    CircularBufferREPS();
    void add(T element);
    T remove();
    T remove_earliest_fresh();
    T remove_frozen();
    int size() const;
    bool isEmpty() const;
    bool isFull() const;
    void print() const;
};

#endif // CIRCULARBUFFERREPS_H
