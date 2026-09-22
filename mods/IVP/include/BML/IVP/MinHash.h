#ifndef BML_IVP_MIN_HASH_H
#define BML_IVP_MIN_HASH_H

#include "BML/IVP/Types.h"

#include <cstddef>
#include <cstdint>

class IVP_U_Min_Hash_Elem {
public:
    IVP_U_Min_Hash_Elem *next;
    IVP_DOUBLE value;
    // Ballance was built with SORT_MINDIST_ELEMENTS enabled.
    int cmp_index;
    void *elem;
};

class IVP_U_Min_Hash_Enumerator;

class IVP_U_Min_Hash {
    friend class IVP_U_Min_Hash_Enumerator;

public:
    explicit IVP_U_Min_Hash(int initialSize = 256) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MinHashConstruct,
            this, initialSize);
    }
    ~IVP_U_Min_Hash() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MinHashDestruct, this);
    }

    IVP_U_Min_Hash(const IVP_U_Min_Hash &) = delete;
    IVP_U_Min_Hash &operator=(const IVP_U_Min_Hash &) = delete;

    void add(void *element, IVP_DOUBLE value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MinHashAdd,
            this, element, value);
    }
    void change_value(void *element, IVP_DOUBLE newValue) {
        // The nearby implementation is exactly remove followed by add; both
        // allocator-owning operations remain in the retail module.
        remove(element);
        add(element, newValue);
    }
    void *find_min_elem() {
        return stadel[1] ? stadel[1]->elem : nullptr;
    }
    IVP_DOUBLE find_min_value() { return stadel[1]->value; }
    int is_elem(void *element) const {
        for (IVP_U_Min_Hash_Elem *candidate = elems[hash_index(element)];
             candidate; candidate = candidate->next) {
            if (candidate->elem == element)
                return 1;
        }
        return 0;
    }
    void remove(void *element) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MinHashRemove, this, element);
    }
    void remove_min() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MinHashRemoveMinimum, this);
    }

private:
    int hash_index(const void *element) const {
        const std::uint32_t key = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(element));
        const std::uint32_t mixed =
            ((key * 101u) >> 8u) + key * 1001u;
        return static_cast<int>(mixed & (size - 1u));
    }

    unsigned int size;
    IVP_U_Min_Hash_Elem **stadel;
    IVP_U_Min_Hash_Elem **min_per_array_pos;
    IVP_U_Min_Hash_Elem **elems;

public:
    int counter;
};

class IVP_U_Min_Hash_Enumerator {
public:
    explicit IVP_U_Min_Hash_Enumerator(IVP_U_Min_Hash *hash)
        : min_hash(hash), loop_elem(nullptr), loop_index(-1) {}

    void *get_next_element() {
        if (loop_elem)
            loop_elem = loop_elem->next;
        while (!loop_elem) {
            ++loop_index;
            if (loop_index >= static_cast<int>(min_hash->size))
                return nullptr;
            loop_elem = min_hash->elems[loop_index];
        }
        return loop_elem->elem;
    }

    void *get_next_element_lt(IVP_DOUBLE maximum) {
        while (true) {
            void *element = get_next_element();
            if (!element)
                return nullptr;
            if (loop_elem->value < maximum)
                return element;
        }
    }

private:
    IVP_U_Min_Hash *min_hash;
    IVP_U_Min_Hash_Elem *loop_elem;
    int loop_index;
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_U_Min_Hash_Elem) == 0x18);
static_assert(offsetof(IVP_U_Min_Hash_Elem, value) == 0x08);
static_assert(offsetof(IVP_U_Min_Hash_Elem, cmp_index) == 0x10);
static_assert(sizeof(IVP_U_Min_Hash) == 0x14);
static_assert(offsetof(IVP_U_Min_Hash, counter) == 0x10);
static_assert(sizeof(IVP_U_Min_Hash_Enumerator) == 0x0C);
#endif

#endif // BML_IVP_MIN_HASH_H
