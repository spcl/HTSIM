#include "buffer_reps.h"

// Static member initialization
template <typename T> bool CircularBufferREPS<T>::repsUseFreezing = false;
template <typename T> int CircularBufferREPS<T>::repsBufferSize = 10;

// Constructor
template <typename T>
CircularBufferREPS<T>::CircularBufferREPS(int bufferSize) : max_size(bufferSize), head(0), tail(0), count(0) {
    buffer = new Element[max_size];
    for (int i = 0; i < max_size; i++) {
        buffer[i].isValid = false;
        buffer[i].usable_lifetime = 0;
    }
}

// Destructor
template <typename T> CircularBufferREPS<T>::~CircularBufferREPS() { delete[] buffer; }

// Adds an element to the buffer
template <typename T> void CircularBufferREPS<T>::add(T element) {
    if (!buffer[head].isValid) {
        number_fresh_entropies++;
    }
    buffer[head].value = element;
    buffer[head].isValid = true;
    buffer[head].usable_lifetime = 4;
    head = (head + 1) % max_size;

    if (number_fresh_entropies > max_size) {
        number_fresh_entropies = max_size;
    }
    count++;
    if (count > max_size) {
        count = max_size;
    }
    printf("REPS_ADD: head: %d vs %d, tail: %d, count: %d, number_fresh_entropies: %d, entropy %d (%d), num_valid %d\n",
           head, head_forzen_mode, tail, count, number_fresh_entropies, element, buffer[head].usable_lifetime,
           numValid());
}

// Removes an element from the buffer
template <typename T> T CircularBufferREPS<T>::remove() {
    if (count == 0) {
        throw std::underflow_error("Buffer is empty");
    }

    if (!buffer[tail].isValid) {
        throw std::runtime_error("Element is not valid");
    }
    exit(EXIT_FAILURE);

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
        offset = head + max_size - number_fresh_entropies;
        if (offset < 0) {
            throw std::underflow_error("Offset can not be negative1");
            exit(EXIT_FAILURE);
        }
    } else {
        offset = head - number_fresh_entropies;
        if (offset < 0) {
            throw std::underflow_error("Offset can not be negative2");
            exit(EXIT_FAILURE);
        }
    }
    T element = buffer[offset].value;

    if (compressed_acks) {
        buffer[offset].usable_lifetime--;
        if (buffer[offset].usable_lifetime < 0) {
            buffer[offset].isValid = false;
            number_fresh_entropies--;
        }
    } else {
        buffer[offset].isValid = false;
        number_fresh_entropies--;
    }

    printf("REPS_REMOVE_FRESH: head: %d vs %d, tail: %d, count: %d, number_fresh_entropies: %d, entropy %d, num_valid "
           "%d\n",
           head, head_forzen_mode, tail, count, number_fresh_entropies, element, numValid());

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

    bool old_validity = buffer[head_forzen_mode].isValid;

    T element = buffer[head_forzen_mode].value;

    if (compressed_acks) {
        buffer[offset].usable_lifetime--;
        if (buffer[offset].usable_lifetime < 0) {
            buffer[offset].isValid = false;
            if (old_validity) {
                number_fresh_entropies--;
            }
        }
    } else {
        if (old_validity) {
            number_fresh_entropies--;
        }
        buffer[head_forzen_mode].isValid = false;
    }

    head_forzen_mode = (head_forzen_mode + 1) % getSize();

    printf("REPS_REMOVE_FROZEN: head: %d vs %d, tail: %d, count: %d, number_fresh_entropies: %d, entropy %d, valid %d, "
           "num_valid %d\n",
           head, head_forzen_mode, tail, count, number_fresh_entropies, element, old_validity, numValid());
    return element;
}

// Removes an element from the buffer
template <typename T> bool CircularBufferREPS<T>::is_valid_frozen() {
    if (count == 0) {
        throw std::underflow_error("Buffer is empty");
    }
    if (!frozen_mode) {
        throw std::runtime_error("Using Remove Frozen without being in frozen mode");
    }
    return buffer[head_forzen_mode].isValid;
}

// Returns the number of elements in the buffer
template <typename T> int CircularBufferREPS<T>::getSize() const { return count; }

// Returns the number of elements in the buffer
template <typename T> int CircularBufferREPS<T>::getNumberFreshEntropies() const { return number_fresh_entropies; }

// Checks if the buffer is empty
template <typename T> bool CircularBufferREPS<T>::isEmpty() const { return count == 0; }

// Checks if the buffer is full
template <typename T> bool CircularBufferREPS<T>::isFull() const { return count == max_size; }

// Count how many elements are valiud
template <typename T> int CircularBufferREPS<T>::numValid() const {
    int num_valid = 0;
    for (int i = 0; i < count; ++i) {
        if (buffer[i].isValid) {
            num_valid++;
        }
    }
    return num_valid;
}

// Prints the elements of the buffer
template <typename T> void CircularBufferREPS<T>::print() const {
    std::cout << "Buffer elements (value, isValid): ";
    for (int i = 0; i < max_size; ++i) {
        std::cout << "(" << buffer[i].value << ", " << buffer[i].isValid << ") ";
    }
    std::cout << std::endl;
}

// Explicitly instantiate templates for common types (if needed)
template class CircularBufferREPS<int>;
// Add more template instantiations if you need other types
