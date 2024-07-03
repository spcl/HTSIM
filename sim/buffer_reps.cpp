#include "buffer_reps.h"

// Static member initialization
template <typename T> bool CircularBufferREPS<T>::repsUseFreezing = false;
template <typename T> int CircularBufferREPS<T>::repsBufferSize = 10;
template <typename T> int CircularBufferREPS<T>::repsMaxLifetimeEntropy = 1;
template <typename T> bool CircularBufferREPS<T>::compressed_acks = false;

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
    if (isFrozenMode()) {
        // If the element is contained in the buffer, return
        /* printf("elment is %d, max size is %d\n", element, max_size);
        fflush(stdout); */
        if (containsEntropy(element)) {
            // return;
        }
    }

    if (!buffer[head].isValid) {
        number_fresh_entropies++;
    }
    buffer[head].value = element;
    buffer[head].isValid = true;
    buffer[head].usable_lifetime = repsMaxLifetimeEntropy;
    head = (head + 1) % max_size;

    if (number_fresh_entropies > max_size) {
        number_fresh_entropies = max_size;
    }
    count++;
    if (count > max_size) {
        count = max_size;
    }
    /* printf(
        "REPS_ADD: head: %d vs %d, tail: %d, count: %d, number_fresh_entropies: %d, entropy %d "
        "(%d), num_valid %d\n",
        head, head_forzen_mode, tail, count, number_fresh_entropies, element,
        buffer[head].usable_lifetime, numValid()); */
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
        if (buffer[offset].usable_lifetime <= 0) {
            buffer[offset].isValid = false;
            number_fresh_entropies--;
        }
    } else {
        buffer[offset].isValid = false;
        number_fresh_entropies--;
    }

    /* printf(
        "REPS_REMOVE_FRESH: head: %d vs %d, tail: %d, count: %d, number_fresh_entropies: %d, "
        "entropy %d, num_valid "
        "%d\n",
        head, head_forzen_mode, tail, count, number_fresh_entropies, element, numValid()); */

    return element;
}

// Removes an element from the buffer
template <typename T> T CircularBufferREPS<T>::remove_earliest_round() {
    if (count == 0 || number_fresh_entropies == 0) {
        throw std::underflow_error("Buffer is empty or no fresh entropies");
        exit(EXIT_FAILURE);
    }

    int idx_to_use = mostValidIdx();
    buffer[idx_to_use].usable_lifetime--;
    if (buffer[idx_to_use].usable_lifetime <= 0) {
        buffer[idx_to_use].isValid = false;
        number_fresh_entropies--;
        buffer[idx_to_use].usable_lifetime = 0;
    }
    return buffer[idx_to_use].value;
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
        buffer[head_forzen_mode].usable_lifetime--;
        if (buffer[head_forzen_mode].usable_lifetime < 0) {
            buffer[head_forzen_mode].isValid = false;
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

    /* printf(
        "REPS_REMOVE_FROZEN: head: %d vs %d, tail: %d, count: %d, number_fresh_entropies: %d, "
        "entropy %d, valid %d, "
        "num_valid %d\n",
        head, head_forzen_mode, tail, count, number_fresh_entropies, element, old_validity,
        numValid()); */
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

// Checks if the buffer is empty
template <typename T> int CircularBufferREPS<T>::mostValidIdx() const {
    int max_lifetime = 0;
    int idx_max_lifetime = -1;
    for (int i = 0; i < max_size; i++) {
        if (buffer[i].isValid && buffer[i].usable_lifetime > max_lifetime) {
            max_lifetime = buffer[i].usable_lifetime;
            idx_max_lifetime = i;
        }
    }
    while (true) {
        int idx_to_try = rand() % max_size;
        if (buffer[idx_to_try].isValid) {
            return idx_to_try;
        }
    }
    // return idx_max_lifetime;
}

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
template <typename T> void CircularBufferREPS<T>::print() {
    std::cout << "Buffer elements forzen " << isFrozenMode() << " (value, isValid): ";
    for (int i = 0; i < max_size; i++) {
        std::cout << "(" << buffer[i].value << ", " << buffer[i].isValid << ") ";
    }
    std::cout << std::endl;
}

// Prints the elements of the buffer
template <typename T> bool CircularBufferREPS<T>::containsEntropy(uint16_t givenEntropy) {
    for (int i = 0; i < max_size; i++) {
        /* printf("Checking %d vs %d\n", buffer[i].value, givenEntropy);
        fflush(stdout); */
        if (buffer[i].value == givenEntropy) {
            return true;
        }
    }
    return false;
}

// Explicitly instantiate templates for common types (if needed)
template class CircularBufferREPS<int>;
// Add more template instantiations if you need other types