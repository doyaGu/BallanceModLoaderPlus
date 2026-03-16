/**
 * @file bml_engine_events.hpp
 * @brief C++ helpers for engine event payload validation
 *
 * Provides a one-line replacement for the 3-line null/struct_size/context
 * validation pattern repeated across modules.
 */

#ifndef BML_ENGINE_EVENTS_HPP
#define BML_ENGINE_EVENTS_HPP

#include "bml_engine_events.h"
#include "bml_imc_message.hpp"

namespace bml {

    /**
     * @brief Validate an engine event payload from an IMC message.
     *
     * Returns nullptr if msg.As<T>() fails, struct_size < sizeof(T),
     * or the context field is null.
     *
     * @tparam T Engine event payload type (must have struct_size and context fields)
     */
    template <typename T>
    const T *ValidateEnginePayload(const bml::imc::Message &msg) {
        auto *p = msg.As<T>();
        if (!p || p->struct_size < sizeof(T) || !p->context) {
            return nullptr;
        }
        return p;
    }

} // namespace bml

#endif /* BML_ENGINE_EVENTS_HPP */
