#ifndef BML_IVP_STRING_HASH_H
#define BML_IVP_STRING_HASH_H

#include "BML/IVP/Types.h"

#include <cstddef>
#include <cstring>

struct IVP_Hash_Elem {
    IVP_Hash_Elem *next;
    void *value;
    char key[1];
};

// Fixed-width binary-key hash used by the collision minimizer and compact
// builders. Ballance retains construction, destruction, lookup and insertion;
// only the source-inline CRC index and link-stripping remove body are rebuilt.
class IVP_Hash {
public:
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    IVP_Hash(
        int bucketCount, int keySize, void *notFoundValue = nullptr) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::HashConstruct, this,
            [this, bucketCount, keySize, notFoundValue] {
                key_size = keySize;
                size = bucketCount;
                not_found_value = notFoundValue;
                elems = reinterpret_cast<IVP_Hash_Elem **>(
                    p_calloc(static_cast<int>(sizeof(void *)), bucketCount));
            },
            bucketCount, keySize, notFoundValue);
    }

    ~IVP_Hash() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::HashDestruct, this,
            [this] {
                if (!elems)
                    return;
                for (int bucket = 0; bucket < size; ++bucket) {
                    IVP_Hash_Elem *element = elems[bucket];
                    while (element) {
                        IVP_Hash_Elem *next = element->next;
                        p_free(element);
                        element = next;
                    }
                }
                p_free(elems);
                elems = nullptr;
            });
    }

    void add(const char *key, void *value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::HashAdd, this, key, value);
    }

    void remove(const char *key) {
        const int bucket = hash_index(key);
        IVP_Hash_Elem *previous = nullptr;
        for (IVP_Hash_Elem *element = elems[bucket]; element;
             element = element->next) {
            if (std::memcmp(element->key, key,
                            static_cast<std::size_t>(key_size)) != 0) {
                previous = element;
                continue;
            }
            if (previous)
                previous->next = element->next;
            else
                elems[bucket] = element->next;
            element->next = nullptr;
            p_free(element);
            return;
        }
    }

    void *find(const char *key) const {
        return BML::IVP::ABI::InvokeThisOr<void *>(
            BML::IVP::ABI::Address::HashFind,
            const_cast<IVP_Hash *>(this),
            [this, key] {
                for (IVP_Hash_Elem *element = elems[hash_index(key)];
                     element; element = element->next) {
                    if (std::memcmp(
                            element->key, key,
                            static_cast<std::size_t>(key_size)) == 0)
                        return element->value;
                }
                return not_found_value;
            },
            key);
    }

    int hash_index(const char *key) const {
        std::uint32_t hash = 0xFFFFFFFFu;
        for (int index = 0; index < key_size; ++index) {
            hash ^= static_cast<unsigned char>(key[index]);
            for (int bit = 0; bit < 8; ++bit)
                hash = (hash >> 1u) ^
                       (0xEDB88320u & (0u - (hash & 1u)));
        }
        return static_cast<int>(
            hash % static_cast<std::uint32_t>(size));
    }

private:
    int key_size;
    int size;
    void *not_found_value;
    IVP_Hash_Elem **elems;
};

// This is the exact small hash used by physics_RT for its surface cache.
// Construction, lookup and insertion stay in the retail DLL. The destructor
// and remove method were inlined away there and are reconstructed from the
// matching allocation/list layout.
class IVP_U_String_Hash {
public:
    explicit IVP_U_String_Hash(int bucketCount,
                               void *notFoundValue = nullptr) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::StringHashConstruct,
            this,
            [this, bucketCount, notFoundValue] {
                // There is no compatible caller-CRT allocation fallback for
                // this retail-owned table. Keep an unresolved instance inert
                // while preserving the two scalar constructor arguments.
                size = bucketCount;
                not_found_value = notFoundValue;
                elems = nullptr;
            },
            bucketCount, notFoundValue);
    }

    ~IVP_U_String_Hash() {
        if (!elems)
            return;
        for (int bucket = 0; bucket < size; ++bucket) {
            IVP_Hash_Elem *element = elems[bucket];
            while (element) {
                IVP_Hash_Elem *next = element->next;
                BML::IVP::ABI::Invoke<void>(
                    BML::IVP::ABI::Address::Free, element);
                element = next;
            }
        }
        BML::IVP::ABI::Invoke<void>(BML::IVP::ABI::Address::Free, elems);
        elems = nullptr;
        size = 0;
    }

    void add(const char *key, void *value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::StringHashAdd, this, key, value);
    }

    void *find(const char *key) const {
        return BML::IVP::ABI::InvokeThisOr<void *>(
            BML::IVP::ABI::Address::StringHashFind,
            const_cast<IVP_U_String_Hash *>(this),
            [this] { return not_found_value; }, key);
    }

    void remove(const char *key) {
        if (!key || !elems || size <= 0)
            return;
        const int bucket = hash_index(key);
        IVP_Hash_Elem *previous = nullptr;
        for (IVP_Hash_Elem *element = elems[bucket]; element;
             element = element->next) {
            if (std::strcmp(element->key, key) != 0) {
                previous = element;
                continue;
            }
            if (previous)
                previous->next = element->next;
            else
                elems[bucket] = element->next;
            BML::IVP::ABI::Invoke<void>(
                BML::IVP::ABI::Address::Free, element);
            return;
        }
    }

    int hash_index(const char *key) const {
        // The retail routine uses CRC-32 with the reflected polynomial, an
        // initial value of 0xFFFFFFFF and no final xor. Recompute that value
        // directly because the table itself is private DLL data.
        std::uint32_t hash = 0xFFFFFFFFu;
        for (const unsigned char *cursor =
                 reinterpret_cast<const unsigned char *>(key);
             *cursor; ++cursor) {
            hash ^= *cursor;
            for (int bit = 0; bit < 8; ++bit)
                hash = (hash >> 1u) ^
                       (0xEDB88320u & (0u - (hash & 1u)));
        }
        return static_cast<int>(hash % static_cast<std::uint32_t>(size));
    }

    int size;
    void *not_found_value;
    IVP_Hash_Elem **elems;
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_U_String_Hash) == 0x0C);
static_assert(offsetof(IVP_U_String_Hash, elems) == 0x08);
static_assert(sizeof(IVP_Hash) == 0x10);
#endif

#endif // BML_IVP_STRING_HASH_H
