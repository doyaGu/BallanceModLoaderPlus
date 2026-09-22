#ifndef BML_IVP_MIN_LIST_H
#define BML_IVP_MIN_LIST_H

#include "BML/IVP/Types.h"

#include <cstdint>

using IVP_U_MINLIST_FIXED_POINT = IVP_FLOAT;
using IVP_U_MINLIST_INDEX = unsigned int;

inline constexpr IVP_U_MINLIST_FIXED_POINT IVP_U_MINLIST_MAXVALUE = 1.0e10f;
inline constexpr IVP_U_MINLIST_INDEX IVP_U_MINLIST_UNUSED = 0xFFFFu;
inline constexpr IVP_U_MINLIST_INDEX IVP_U_MINLIST_LONG_UNUSED = 0xFFFEu;
inline constexpr IVP_U_MINLIST_INDEX IVP_U_MINLIST_MAX_ALLOCATION = 0xFFFCu;

class IVP_U_Min_List_Element {
public:
    std::uint16_t long_next;
    std::uint16_t long_prev;
    std::uint16_t next;
    std::uint16_t prev;
    IVP_U_MINLIST_FIXED_POINT value;
    void *element;
};

class IVP_U_Min_List_Enumerator;

class IVP_U_Min_List {
    friend class IVP_U_Min_List_Enumerator;

private:
    std::uint16_t malloced_size;
    std::uint16_t free_list;
    IVP_U_Min_List_Element *elems;

public:
    IVP_U_MINLIST_FIXED_POINT min_value;
    std::uint16_t first_long;
    std::uint16_t first_element;
    std::uint16_t counter;

    explicit IVP_U_Min_List(int size = 4) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MinListConstruct, this, size);
    }
    ~IVP_U_Min_List() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MinListDestruct, this);
    }

    IVP_U_Min_List(const IVP_U_Min_List &) = delete;
    IVP_U_Min_List &operator=(const IVP_U_Min_List &) = delete;

    IVP_U_MINLIST_INDEX add(void *element,
                            IVP_U_MINLIST_FIXED_POINT value) {
        return BML::IVP::ABI::InvokeThis<IVP_U_MINLIST_INDEX>(
            BML::IVP::ABI::Address::MinListAdd, this, element, value);
    }
    void *find_min_elem() {
        return first_element == IVP_U_MINLIST_UNUSED
                   ? nullptr
                   : elems[first_element].element;
    }
    IVP_BOOL has_elements() {
        return counter != 0 ? IVP_TRUE : IVP_FALSE;
    }
    IVP_U_MINLIST_FIXED_POINT find_min_value() { return min_value; }

    // Retail was built with the optional IVP prefetch path disabled.
    void prefetch0_minlist() {}
    void prefetch1_minlist() {}

    void remove_minlist_elem(IVP_U_MINLIST_INDEX index) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MinListRemove, this, index);
    }

    // The retail release compiles this assertion-only verifier to no code.
    void check() {}
};

class IVP_U_Min_List_Enumerator {
private:
    IVP_U_Min_List *min_list;
    IVP_U_MINLIST_INDEX loop_elem;

public:
    explicit IVP_U_Min_List_Enumerator(IVP_U_Min_List *list)
        : min_list(list), loop_elem(list->first_element) {}

    void *get_next_element() {
        if (loop_elem == IVP_U_MINLIST_UNUSED)
            return nullptr;
        IVP_U_Min_List_Element &element = min_list->elems[loop_elem];
        loop_elem = element.next;
        return element.element;
    }
    IVP_U_Min_List_Element *get_next_element_header() {
        if (loop_elem == IVP_U_MINLIST_UNUSED)
            return nullptr;
        IVP_U_Min_List_Element *element = &min_list->elems[loop_elem];
        loop_elem = element->next;
        return element;
    }
    void *get_next_element_lt(IVP_FLOAT maxLimit) {
        if (loop_elem == IVP_U_MINLIST_UNUSED)
            return nullptr;
        IVP_U_Min_List_Element &element = min_list->elems[loop_elem];
        if (element.value >= maxLimit)
            return nullptr;
        loop_elem = element.next;
        return element.element;
    }
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_U_Min_List_Element) == 0x10);
static_assert(sizeof(IVP_U_Min_List) == 0x14);
static_assert(sizeof(IVP_U_Min_List_Enumerator) == 0x08);
#endif

#endif // BML_IVP_MIN_LIST_H
