#ifndef BML_IVP_SET_H
#define BML_IVP_SET_H

#include "BML/IVP/Types.h"

#include <cstddef>
#include <cstdio>
#include <cstdint>

class IVP_Core;

struct IVP_VHash_Elem {
    std::uint32_t hash_index;
    const void *elem;
};

class IVP_VHash {
    friend struct BML_IvpVHashLayoutCheck;

private:
    static constexpr std::uint32_t TouchBit = 0x80000000u;

protected:
    explicit IVP_VHash(int initialSize) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::VHashConstruct, this, initialSize);
    }
    IVP_VHash(IVP_VHash_Elem *staticElements, int size)
        : size_mm(size - 1), nelems(0), dont_free(1), elems(staticElements) {}

    virtual IVP_BOOL compare(void *left, void *right) const = 0;

public:
    static int hash_index(const char *data, int size) {
        std::uint32_t index = 0xFFFFFFFFu;
        for (int byte = size - 1; byte >= 0; --byte) {
            index ^= static_cast<unsigned char>(*data++);
            for (int bit = 0; bit < 8; ++bit) {
                const std::uint32_t mask =
                    0u - static_cast<std::uint32_t>(index & 1u);
                index = (index >> 1u) ^ (0xEDB88320u & mask);
            }
        }
        return static_cast<int>(index | TouchBit);
    }

    static int fast_hash_index(int key) {
        const std::uint32_t keyBits = static_cast<std::uint32_t>(key);
        const std::uint32_t product = keyBits * 1001u;
        const std::uint32_t arithmeticHigh =
            (product >> 16u) |
            ((product & 0x80000000u) != 0u ? 0xFFFF0000u : 0u);
        return static_cast<int>(
            (arithmeticHigh + keyBits * 75u) | TouchBit);
    }

    void add_elem(const void *element, int hashIndex) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::VHashAdd, this, element, hashIndex);
    }

    void *remove_elem(const void *element, unsigned int hashIndex) {
        return BML::IVP::ABI::InvokeThis<void *>(
            BML::IVP::ABI::Address::VHashRemove,
            this, element, hashIndex);
    }

    void *find_elem(const void *element, unsigned int hashIndex) const {
        return BML::IVP::ABI::InvokeThis<void *>(
            BML::IVP::ABI::Address::VHashFind,
            const_cast<IVP_VHash *>(this), element, hashIndex);
    }

    void *touch_element(const void *element, unsigned int hashIndex) {
        int position = static_cast<int>(hashIndex) & size_mm;
        for (;; position = (position + 1) & size_mm) {
            IVP_VHash_Elem &entry = elems[position];
            if (!entry.elem)
                return nullptr;
            if ((entry.hash_index | TouchBit) != hashIndex ||
                compare(const_cast<void *>(entry.elem),
                        const_cast<void *>(element)) == IVP_FALSE) {
                continue;
            }
            entry.hash_index |= TouchBit;
            return const_cast<void *>(entry.elem);
        }
    }

    void garbage_collection(int preferredSize) {
        if (preferredSize < n_elems())
            return;
        int newSize = size_mm + 1;
        while (preferredSize * 2 + 1 < newSize)
            newSize /= 2;
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::VHashRehash, this, newSize);
    }

    void deactivate() {
        if (!dont_free && elems) {
            BML::IVP::ABI::Invoke<void>(
                BML::IVP::ABI::Address::Free, elems);
            elems = nullptr;
        }
        size_mm = -1;
    }

    void activate(int preferredSize) {
        size_mm = preferredSize - 1;
        nelems = 0;
        dont_free = 0;
        elems = BML::IVP::ABI::Invoke<IVP_VHash_Elem *>(
            BML::IVP::ABI::Address::AllocateZeroed,
            preferredSize, static_cast<int>(sizeof(IVP_VHash_Elem)));
    }

    int len() const { return size_mm + 1; }
    int n_elems() { return static_cast<int>(nelems); }
    int n_elems() const { return static_cast<int>(nelems); }
    void *element_at(int index) const {
        return const_cast<void *>(elems[index].elem);
    }
    IVP_BOOL is_element_touched(int index) const {
        return elems[index].hash_index >= TouchBit ? IVP_TRUE : IVP_FALSE;
    }
    void untouch_all() {
        for (int index = size_mm; index >= 0; --index)
            elems[index].hash_index &= TouchBit - 1u;
    }
    void print() const {
        std::printf("%i:", len());
        for (int index = 0; index <= size_mm; ++index) {
            std::printf(" %i:%p:%X  ",
                        static_cast<int>(elems[index].hash_index) & size_mm,
                        elems[index].elem, elems[index].hash_index);
        }
        std::printf("\n");
    }
    void check() {}

    int table_size() const { return len(); }
    const IVP_VHash_Elem *element_slot(int index) const {
        return index >= 0 && index <= size_mm ? &elems[index] : nullptr;
    }

    virtual ~IVP_VHash() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::VHashDestruct, this);
    }

protected:
    int size_mm;
    std::uint32_t nelems : 24;
    std::uint32_t dont_free : 8;
    IVP_VHash_Elem *elems;
};

struct IVP_VHash_Store_Elem {
    unsigned int hash_index;
    void *key_elem;
    void *elem;
};

class IVP_VHash_Store {
    friend struct BML_IvpVHashStoreLayoutCheck;

public:
    explicit IVP_VHash_Store(int initialSize) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::VHashStoreConstruct,
            this, initialSize);
    }
    IVP_VHash_Store(IVP_VHash_Store_Elem *staticElements, int initialSize)
        : size(initialSize), size_mm(initialSize - 1), nelems(0),
          elems_store(staticElements), dont_free(staticElements) {}
    ~IVP_VHash_Store() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::VHashStoreDestruct, this);
    }

    IVP_VHash_Store(const IVP_VHash_Store &) = delete;
    IVP_VHash_Store &operator=(const IVP_VHash_Store &) = delete;

    void add_elem(void *key, void *element) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::VHashStoreAdd,
            this, key, element);
    }
    void add_elem(void *key, void *element, int hashIndex) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::VHashStoreAddHashed,
            this, key, element, hashIndex);
    }
    void change_elem(void *key, void *element) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::VHashStoreChange,
            this, key, element);
    }
    void *remove_elem(void *key) {
        return BML::IVP::ABI::InvokeThis<void *>(
            BML::IVP::ABI::Address::VHashStoreRemove, this, key);
    }
    void *remove_elem(void *key, unsigned int hashIndex) {
        return BML::IVP::ABI::InvokeThis<void *>(
            BML::IVP::ABI::Address::VHashStoreRemoveHashed,
            this, key, hashIndex);
    }
    void *find_elem(void *key) {
        return BML::IVP::ABI::InvokeThis<void *>(
            BML::IVP::ABI::Address::VHashStoreFind, this, key);
    }
    void *find_elem(void *key, unsigned int hashIndex) {
        return BML::IVP::ABI::InvokeThis<void *>(
            BML::IVP::ABI::Address::VHashStoreFindHashed,
            this, key, hashIndex);
    }
    void *touch_element(void *key, unsigned int hashIndex) {
        int position = static_cast<int>(hashIndex) & size_mm;
        for (;; position = (position + 1) & size_mm) {
            IVP_VHash_Store_Elem &entry = elems_store[position];
            if (!entry.key_elem)
                return nullptr;
            if ((entry.hash_index | 0x80000000u) != hashIndex ||
                compare_store_hash(entry.key_elem, key) == IVP_FALSE) {
                continue;
            }
            entry.hash_index |= 0x80000000u;
            return entry.elem;
        }
    }

    int len() { return size; }
    int n_elems() { return nelems; }
    void *element_at(int index) { return elems_store[index].elem; }
    IVP_BOOL is_element_touched(int index) {
        return elems_store[index].hash_index >= 0x80000000u
            ? IVP_TRUE : IVP_FALSE;
    }
    void untouch_all() {
        for (int index = size - 1; index >= 0; --index)
            elems_store[index].hash_index &= 0x7FFFFFFFu;
    }
    void print() {
        std::printf("%i:", size);
        for (int index = 0; index < size; ++index) {
            std::printf(" %i:%p:%p:%X  ",
                        static_cast<int>(elems_store[index].hash_index) & size_mm,
                        elems_store[index].key_elem,
                        elems_store[index].elem,
                        elems_store[index].hash_index);
        }
        std::printf("\n");
    }
    void check() {}

protected:
    static IVP_BOOL compare_store_hash(void *left, void *right) {
        return left == right ? IVP_TRUE : IVP_FALSE;
    }
    static int hash_index_store(const char *data, int dataSize) {
        return IVP_VHash::hash_index(data, dataSize);
    }
    static int void_pointer_to_index(void *pointer) {
        void *value = pointer;
        return hash_index_store(
            reinterpret_cast<const char *>(&value), sizeof(value));
    }

private:
    int size;
    int size_mm;
    int nelems;
    IVP_VHash_Store_Elem *elems_store;
    void *dont_free;
};

template <class T>
class IVP_VHash_Enumerator {
public:
    explicit IVP_VHash_Enumerator(IVP_VHash *set)
        : index(set->len() - 1) {}

    T *get_next_element(IVP_VHash *set) {
        while (index >= 0) {
            T *result = static_cast<T *>(set->element_at(index--));
            if (result)
                return result;
        }
        return nullptr;
    }

private:
    int index;
};

template <class T>
class IVP_U_Set_Enumerator {
public:
    explicit IVP_U_Set_Enumerator(IVP_VHash *set)
        : index(set->len() - 1) {}

    T *get_next_element(IVP_VHash *set) {
        while (index >= 0) {
            T *result = static_cast<T *>(set->element_at(index--));
            if (result)
                return result;
        }
        return nullptr;
    }

private:
    int index;
};

template <class T>
class IVP_U_Set : public IVP_VHash {
public:
    explicit IVP_U_Set(int initialSize) : IVP_VHash(initialSize) {}
    ~IVP_U_Set() override = default;

    void add_element(T *element) {
        IVP_VHash::add_elem(element, elem_to_index(element));
    }
    void remove_element(T *element) {
        IVP_VHash::remove_elem(element, elem_to_index(element));
    }
    T *find_element(T *element) {
        return static_cast<T *>(
            IVP_VHash::find_elem(element, elem_to_index(element)));
    }
    void install_element(T *element) {
        const int index = elem_to_index(element);
        if (!IVP_VHash::find_elem(element, index))
            IVP_VHash::add_elem(element, index);
    }
    int n_elems() { return IVP_VHash::n_elems(); }

    T *element_at_slot(int index) const {
        const IVP_VHash_Elem *slot = element_slot(index);
        return slot ? static_cast<T *>(const_cast<void *>(slot->elem)) : nullptr;
    }

protected:
    IVP_BOOL compare(void *left, void *right) const override {
        return left == right ? IVP_TRUE : IVP_FALSE;
    }
    int elem_to_index(T *element) const {
        const auto bits = reinterpret_cast<std::uintptr_t>(element);
        return fast_hash_index(static_cast<std::int32_t>(bits));
    }
};

template <class T>
class IVP_U_Set_Active;

template <class T>
class IVP_Listener_Set_Active {
public:
    virtual void element_added(IVP_U_Set_Active<T> *, T *) = 0;
    virtual void element_removed(IVP_U_Set_Active<T> *, T *) = 0;
    virtual void pset_is_going_to_be_deleted(IVP_U_Set_Active<T> *) = 0;
};

template <class T>
class IVP_U_Set_Active : public IVP_U_Set<T> {
    template <class>
    friend struct BML_IvpActiveSetLayoutCheck;

public:
    explicit IVP_U_Set_Active(int initialSize)
        : IVP_U_Set<T>(initialSize) {}

    ~IVP_U_Set_Active() override {
        for (int index = listeners.len() - 1; index >= 0; --index)
            listeners.element_at(index)->pset_is_going_to_be_deleted(this);
    }

    void add_element(T *element) {
        IVP_U_Set<T>::add_element(element);
        notify_added(element);
    }
    void install_element(T *element) {
        if (IVP_U_Set<T>::find_element(element))
            return;
        IVP_U_Set<T>::add_element(element);
        notify_added(element);
    }
    void remove_element(T *element) {
        IVP_U_Set<T>::remove_element(element);
        for (int index = listeners.len() - 1; index >= 0; --index)
            listeners.element_at(index)->element_removed(this, element);
    }
    void add_listener_set_active(IVP_Listener_Set_Active<T> *listener) {
        listeners.add(listener);
    }
    void remove_listener_set_active(IVP_Listener_Set_Active<T> *listener) {
        listeners.remove(listener);
    }

    const IVP_U_Vector_Base &get_listeners() const { return listeners; }

protected:
    void notify_added(T *element) {
        for (int index = listeners.len() - 1; index >= 0; --index)
            listeners.element_at(index)->element_added(this, element);
    }

    IVP_U_Vector<IVP_Listener_Set_Active<T>> listeners;
};

template <class T>
struct BML_IvpActiveSetLayoutCheck {
    static constexpr std::size_t listeners =
        offsetof(IVP_U_Set_Active<T>, listeners);
};

struct BML_IvpVHashLayoutCheck {
    static constexpr std::size_t size_mask = offsetof(IVP_VHash, size_mm);
    static constexpr std::size_t elements = offsetof(IVP_VHash, elems);
};

struct BML_IvpVHashStoreLayoutCheck {
    static constexpr std::size_t size = offsetof(IVP_VHash_Store, size);
    static constexpr std::size_t size_mask =
        offsetof(IVP_VHash_Store, size_mm);
    static constexpr std::size_t count = offsetof(IVP_VHash_Store, nelems);
    static constexpr std::size_t elements =
        offsetof(IVP_VHash_Store, elems_store);
    static constexpr std::size_t static_storage =
        offsetof(IVP_VHash_Store, dont_free);
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_VHash_Elem) == 0x08);
static_assert(sizeof(IVP_VHash) == 0x10);
static_assert(BML_IvpVHashLayoutCheck::size_mask == 0x04);
static_assert(BML_IvpVHashLayoutCheck::elements == 0x0C);
static_assert(sizeof(IVP_VHash_Store_Elem) == 0x0C);
static_assert(sizeof(IVP_VHash_Store) == 0x14);
static_assert(BML_IvpVHashStoreLayoutCheck::size == 0x00);
static_assert(BML_IvpVHashStoreLayoutCheck::size_mask == 0x04);
static_assert(BML_IvpVHashStoreLayoutCheck::count == 0x08);
static_assert(BML_IvpVHashStoreLayoutCheck::elements == 0x0C);
static_assert(BML_IvpVHashStoreLayoutCheck::static_storage == 0x10);
static_assert(sizeof(IVP_U_Set<IVP_Core>) == 0x10);
static_assert(sizeof(IVP_Listener_Set_Active<IVP_Core>) == 0x04);
static_assert(sizeof(IVP_U_Set_Active<IVP_Core>) == 0x18);
static_assert(BML_IvpActiveSetLayoutCheck<IVP_Core>::listeners == 0x10);
static_assert(sizeof(IVP_VHash_Enumerator<IVP_Core>) == 0x04);
static_assert(sizeof(IVP_U_Set_Enumerator<IVP_Core>) == 0x04);
#endif

#endif // BML_IVP_SET_H
