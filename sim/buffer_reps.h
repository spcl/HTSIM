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

    Element *buffer; // Pointer to dynamically allocated buffer array
    int size;        // Size of the circular buffer
    int head = 0;    // Points to the next element to be written
    int tail = 0;    // Points to the next element to be read
    int count = 0;   // Number of elements in the buffer
    int head_forzen_mode = 0;
    int number_fresh_entropies = 0;
    bool frozen_mode = false;

  public:
    CircularBufferREPS(int bufferSize = 10); // Default size is 10
    ~CircularBufferREPS();
    void add(T element);
    T remove();
    T remove_earliest_fresh();
    T remove_frozen();
    int getSize() const;
    int getNumberFreshEntropies() const;
    bool isEmpty() const;
    bool isFull() const;
    void print() const;
    void setFrozenMode(bool mode) { frozen_mode = mode; };
    bool isFrozenMode() { return frozen_mode; };
};

#endif // CIRCULARBUFFERREPS_H
