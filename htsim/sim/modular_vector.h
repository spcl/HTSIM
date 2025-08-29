#ifndef MODULAR_VECTOR_H
#define MODULAR_VECTOR_H

template <typename T, unsigned Size>
class ModularVector {
    // Size is a power of 2 (because of seqno wrap-around)
    // static_assert(!(Size & (Size - 1)));
    T buf[Size];

   public:
    ModularVector(T default_value) {
        for (uint i = 0; i < Size; i++) {
            buf[i] = default_value;
        }
    }
    T& operator[](unsigned idx) { return buf[idx & (Size - 1)]; }
};

#endif
