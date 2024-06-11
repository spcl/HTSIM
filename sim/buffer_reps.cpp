#include "buffer_reps.h"

// Constructor
template <typename T>
CircularBufferREPS<T>::CircularBufferREPS(int bufferSize) : size(bufferSize), head(0), tail(0), count(0) {
    buffer = new Element[size];
}

// Destructor
template <typename T> CircularBufferREPS<T>::~CircularBufferREPS() { delete[] buffer; }

// Adds an element to the buffer
template <typename T> void CircularBufferREPS<T>::add(T element) {
    buffer[head].value = element;
    buffer[head].isValid = true;
    head = (head + 1) % size;
    number_fresh_entropies++;
    count++;
    if (count > size) {
        count = size;
    }
}

// Removes an element from the buffer
template <typename T> T CircularBufferREPS<T>::remove() {
    if (count == 0) {
        throw std::underflow_error("Buffer is empty");
    }

    if (!buffer[tail].isValid) {
        throw std::runtime_error("Element is not valid");
    }

    T element = buffer[tail].value;
    buffer[tail].isValid = false;
    --count;
    return element;
}

// Removes an element from the buffer
template <typename T> T CircularBufferREPS<T>::remove_earliest_fresh() {
    if (count == 0 || number_fresh_entropies == 0) {
        throw std::underflow_error("Buffer is empty or no fresh entropies");
        exit(EXIT_FAILURE);
    }

    int offset = 0;
    if (head - number_fresh_entropies < 0) {
        offset = head + size - number_fresh_entropies;
    } else {
        offset = head - number_fresh_entropies;
    }
    T element = buffer[offset].value;
    buffer[tail].isValid = false;
    number_fresh_entropies--;

    std::cout << "head: " << number_fresh_entropies << " number_fresh_entropies: " << tail << " offset: " << offset
              << " element: " << element << " offset: " << offset << std::endl;

    return element;
}

// Removes an element from the buffer
template <typename T> T CircularBufferREPS<T>::remove_frozen() {
    if (count == 0) {
        throw std::underflow_error("Buffer is empty");
    }
    if (!frozen_mode) {
        throw std::runtime_error("Using Remove Frozen without being in frozen mode");
    }

    T element = buffer[head_forzen_mode].value;
    buffer[head_forzen_mode].isValid = false;
    head_forzen_mode = (head_forzen_mode + 1) % getSize();
    return element;
}

// Returns the number of elements in the buffer
template <typename T> int CircularBufferREPS<T>::getSize() const { return count; }

// Returns the number of elements in the buffer
template <typename T> int CircularBufferREPS<T>::getNumberFreshEntropies() const { return number_fresh_entropies; }

// Checks if the buffer is empty
template <typename T> bool CircularBufferREPS<T>::isEmpty() const { return count == 0; }

// Checks if the buffer is full
template <typename T> bool CircularBufferREPS<T>::isFull() const { return count == size; }

// Prints the elements of the buffer
template <typename T> void CircularBufferREPS<T>::print() const {
    std::cout << "Buffer elements (value, isValid): ";
    for (int i = 0; i < size; ++i) {
        std::cout << "(" << buffer[i].value << ", " << buffer[i].isValid << ") ";
    }
    std::cout << std::endl;
}

// Explicitly instantiate templates for common types (if needed)
template class CircularBufferREPS<int>;
// Add more template instantiations if you need other types
