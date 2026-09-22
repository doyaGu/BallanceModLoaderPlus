#ifndef BML_IVP_MEMORY_H
#define BML_IVP_MEMORY_H

#include "BML/IVP/Types.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// Header stored immediately before each arena block. This is the exact x86
// layout consumed by the retained Ballance expansion and reset bodies.
struct p_Memory_Elem {
    p_Memory_Elem *next;
    char data[4];
};

// Short-lived 32-byte-aligned arena used by simulation units, constraints and
// the friction solver. Retained operations always execute inside the retail
// DLL so their blocks stay on its MSVCRT heap; link-stripped source-inline
// operations are rebuilt only around those retained bodies.
class IVP_U_Memory {
public:
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    IVP_U_Memory() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::MemoryConstruct, this,
            [this] { initialize_fields(); });
    }

    ~IVP_U_Memory() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::MemoryDestruct, this,
            [this] { free_mem(); });
    }

    void init_mem() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::MemoryInit, this,
            [this] { initialize_fields(); });
    }

    void *get_mem(unsigned int size) {
        return BML::IVP::ABI::InvokeThisOr<void *>(
            BML::IVP::ABI::Address::MemoryGet, this,
            [this, size] {
                char *allocation = speicherbeginn;
                const std::uintptr_t next = align_address(
                    reinterpret_cast<std::uintptr_t>(allocation) + size);
                if (next >= reinterpret_cast<std::uintptr_t>(speicherende))
                    return static_cast<void *>(neuer_sp_block(size));
                speicherbeginn = reinterpret_cast<char *>(next);
                return static_cast<void *>(allocation);
            },
            size);
    }

    // This body was link-stripped from Ballance. The nearby implementation is
    // only get_mem followed by a byte-counted clear, so allocation itself still
    // crosses the DLL boundary through the exact retained body above.
    void *get_memc(unsigned int size) {
        void *memory = get_mem(size);
        if (memory)
            std::memset(memory, 0, size);
        return memory;
    }

    void free_mem() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::MemoryFree, this,
            [this] {
                for (p_Memory_Elem *element = last_elem; element;) {
                    p_Memory_Elem *next = element->next;
                    if (size_of_external_mem && element == first_elem)
                        break;
                    p_free(element);
                    element = next;
                }
                first_elem = nullptr;
                last_elem = nullptr;
                speicherbeginn = nullptr;
                speicherende = nullptr;
            });
    }

    char *neuer_sp_block(unsigned int size) {
        return BML::IVP::ABI::InvokeThisOr<char *>(
            BML::IVP::ABI::Address::MemoryNewBlock, this,
            [this, size] {
                constexpr std::uint32_t blockSize = 0x7FE0u;
                std::uint32_t usable =
                    blockSize - static_cast<std::uint32_t>(
                                    sizeof(p_Memory_Elem));
                const std::uint32_t alignedSize =
                    (static_cast<std::uint32_t>(size) + 0x1Fu) &
                    ~std::uint32_t{0x1Fu};
                if (alignedSize > usable)
                    usable = alignedSize;

                auto *element = static_cast<p_Memory_Elem *>(p_malloc(
                    static_cast<unsigned int>(
                        sizeof(p_Memory_Elem) + usable + 0x20u)));
                if (!element)
                    return static_cast<char *>(nullptr);
                element->next = last_elem;
                last_elem = element;
                char *begin = align_to_next_adress(element->data);
                speicherbeginn = begin + alignedSize;
                speicherende = begin + usable;
                return begin;
            },
            size);
    }

    void init_mem_transaction_usage(
        char *externalMemory = nullptr, int size = 0) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::MemoryInitTransaction, this,
            [this, externalMemory, size] {
                transaction_in_use = 0;
                if (externalMemory) {
                    size_of_external_mem = static_cast<std::uint16_t>(
                        size - 0x20);
                    auto *element =
                        reinterpret_cast<p_Memory_Elem *>(externalMemory);
                    element->next = last_elem;
                    last_elem = element;
                    char *begin = align_to_next_adress(element->data);
                    speicherbeginn = begin;
                    speicherende = begin + size_of_external_mem;
                } else {
                    size_of_external_mem = 0;
                    neuer_sp_block(0);
                }
                first_elem = last_elem;
            },
            externalMemory, size);
    }

    void start_memory_transaction() {
        ++transaction_in_use;
    }

    void end_memory_transaction() {
        --transaction_in_use;
        free_mem_transaction();
    }

    void *get_mem_transaction(unsigned int size) {
        return get_mem(size);
    }

private:
    static std::uintptr_t align_address(std::uintptr_t address) {
        return (address + 0x1Fu) & ~std::uintptr_t{0x1Fu};
    }

    static char *align_to_next_adress(void *memory) {
        return reinterpret_cast<char *>(
            align_address(reinterpret_cast<std::uintptr_t>(memory)));
    }

    void initialize_fields() {
        first_elem = nullptr;
        last_elem = nullptr;
        speicherbeginn = nullptr;
        speicherende = nullptr;
        transaction_in_use = 3;
        size_of_external_mem = 0;
    }

    void free_mem_transaction() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::MemoryFreeTransaction, this,
            [this] {
                for (p_Memory_Elem *element = last_elem; element;) {
                    p_Memory_Elem *next = element->next;
                    if (element == first_elem)
                        break;
                    p_free(element);
                    element = next;
                }
                char *begin = align_to_next_adress(first_elem->data);
                speicherbeginn = begin;
                speicherende = begin +
                    (size_of_external_mem
                         ? static_cast<std::size_t>(size_of_external_mem)
                         : std::size_t{0x7FE0u});
                last_elem = first_elem;
            });
    }

    p_Memory_Elem *first_elem;
    p_Memory_Elem *last_elem;
    char *speicherbeginn;
    char *speicherende;
    std::int16_t transaction_in_use;
    std::uint16_t size_of_external_mem;
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(p_Memory_Elem) == 0x08);
static_assert(sizeof(IVP_U_Memory) == 0x14);
#endif

#endif // BML_IVP_MEMORY_H
