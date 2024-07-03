#ifndef CIRCULARBUFFERREPS_H
#define CIRCULARBUFFERREPS_H

#include <iostream>
#include <stdexcept>

template <typename T> class CircularBufferREPS {
  private:
    struct Element {
        T value;
        bool isValid;
        int usable_lifetime = 0;

        Element() : value(T()), isValid(false) {}
    };

    Element *buffer; // Pointer to dynamically allocated buffer array
    int max_size;    // Size of the circular buffer
    int head = 0;    // Points to the next element to be written
    int tail = 0;    // Points to the next element to be read
    int count = 0;   // Number of elements in the buffer
    int head_forzen_mode = 0;
    int head_round = 0;
    int number_fresh_entropies = 0;

    bool frozen_mode = false;
    bool circle_mode = true;

  public:
    CircularBufferREPS(int bufferSize = 10); // Default size is 10
    ~CircularBufferREPS();
    void add(T element);
    T remove();
    T remove_earliest_fresh();
    T remove_earliest_round();
    T remove_frozen();
    bool is_valid_frozen();
    int getSize() const;
    int getNumberFreshEntropies() const;
    bool containsEntropy(uint16_t ev);
    int mostValidIdx() const;
    bool isEmpty() const;
    bool isFull() const;
    void print();
    int numValid() const;
    void setFrozenMode(bool mode) {
        if (repsUseFreezing) {
            printf("Freezing mode on\n");
            frozen_mode = mode;
        }
    };
    bool isFrozenMode() { return frozen_mode; };
    static void setUseFreezing(bool enable_freezing_mode) { repsUseFreezing = enable_freezing_mode; };
    static void setBufferSize(int buff_size) { repsBufferSize = buff_size; };
    static void setUsableLifetime(int max_life) {
        repsMaxLifetimeEntropy = max_life;
        if (repsMaxLifetimeEntropy > 1) {
            compressed_acks = true;
        }
    };
    static bool repsUseFreezing;
    static int repsBufferSize;
    static int repsMaxLifetimeEntropy;
    static bool compressed_acks;
};

#endif // CIRCULARBUFFERREPS_H