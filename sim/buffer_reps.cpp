#include "buffer_reps.h"

// Constructor
template <typename T> CircularBufferREPS<T>::CircularBufferREPS() : head(0), tail(0), count(0) {
    for (int i = 0; i < SIZE; ++i) {
        buffer[i] = Element();
    }
}

// Adds an element to the buffer
template <typename T> void CircularBufferREPS<T>::add(T element) {
    buffer[head].value = element;
    buffer[head].isValid = true;
    head = (head + 1) % SIZE;
    number_fresh_entropies++;

    if (count == SIZE) {
        tail = (tail + 1) % SIZE;
    } else {
        ++count;
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
    if (count == 0) {
        throw std::underflow_error("Buffer is empty");
    }

    int offset = 0;
    if (head - number_fresh_entropies < 0) {
        offset = head + SIZE - number_fresh_entropies;
    } else {
        offset = head - number_fresh_entropies;
    }
    T element = buffer[offset].value;
    buffer[tail].isValid = false;
    --count;
    return element;
}

// Removes an element from the buffer
template <typename T> T CircularBufferREPS<T>::remove_frozen() {
    if (count == 0) {
        throw std::underflow_error("Buffer is empty");
    }

    T element = buffer[head_forzen_mode].value;
    head_forzen_mode = (head_forzen_mode + 1) % size();
    buffer[tail].isValid = false;
    return element;
}

// Returns the number of elements in the buffer
template <typename T> int CircularBufferREPS<T>::size() const { return count; }

// Checks if the buffer is empty
template <typename T> bool CircularBufferREPS<T>::isEmpty() const { return count == 0; }

// Checks if the buffer is full
template <typename T> bool CircularBufferREPS<T>::isFull() const { return count == SIZE; }

// Prints the elements of the buffer
template <typename T> void CircularBufferREPS<T>::print() const {
    std::cout << "Buffer elements (value, isValid): ";
    for (int i = 0; i < SIZE; ++i) {
        std::cout << "(" << buffer[i].value << ", " << buffer[i].isValid << ") ";
    }
    std::cout << std::endl;
}

// Explicitly instantiate templates for common types (if needed)
template class CircularBufferREPS<int>;
// Add more template instantiations if you need other types
