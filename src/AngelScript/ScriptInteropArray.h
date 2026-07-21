#ifndef BML_SCRIPT_INTEROP_ARRAY_H
#define BML_SCRIPT_INTEROP_ARRAY_H

#include <limits>
#include <new>
#include <string>
#include <vector>

#include <angelscript.h>

#include "BML/Defines.h"

#include "CKAngelScriptAdapter.h"
#include "ScriptMod.h"
#include "ScriptStringInterop.h"

/* Shared marshalling for the advanced Interop script bridge.  It is private
 * host code: scripts see ordinary array<T> values, never CKAngelScript's
 * array ABI or these C++ helpers. */
namespace BML::ScriptInteropArray {

struct Access {
    const ::CKAngelScriptAdapter::Api *Api = nullptr;
    asIScriptEngine *Engine = nullptr;
    void *Array = nullptr;
};

inline void *GetGenericArrayArgument(asIScriptGeneric *gen, asUINT index) {
    if (!gen)
        return nullptr;
    void *array = gen->GetArgObject(index);
    return array ? array : gen->GetArgAddress(index);
}

inline int Open(ScriptMod *owner,
                asIScriptGeneric *gen,
                asUINT argumentIndex,
                const char *elementDeclaration,
                Access &out) {
    out = {};
    if (!owner || !gen || !elementDeclaration)
        return BML_ERROR_INVALID_PARAMETER;
    const ::CKAngelScriptAdapter::Api &api = owner->GetRuntimeForInterop().GetApi();
    asIScriptEngine *engine = gen->GetEngine();
    void *array = GetGenericArrayArgument(gen, argumentIndex);
    if (!engine || !array || !api.ArrayGetSize || !api.ArrayResize || !api.ArrayGetElementTypeId ||
        !api.ArrayGetElementAddress || !api.ArrayGetConstElementAddress) {
        return BML_ERROR_INTEROP_UNSUPPORTED;
    }
    const int expectedType = engine->GetTypeIdByDecl(elementDeclaration);
    int actualType = 0;
    if (expectedType < 0 || api.ArrayGetElementTypeId(array, &actualType) != CKAS_OK)
        return BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;
    if (actualType != expectedType)
        return BML_ERROR_INTEROP_VALUE_TYPE_MISMATCH;
    out.Api = &api;
    out.Engine = engine;
    out.Array = array;
    return BML_OK;
}

inline int Size(const Access &array, CKDWORD &outSize) {
    outSize = 0;
    return array.Api && array.Array && array.Api->ArrayGetSize &&
                   array.Api->ArrayGetSize(array.Array, &outSize) == CKAS_OK
               ? BML_OK
               : BML_ERROR_FAIL;
}

template <typename T>
int ReadFixed(ScriptMod *owner,
              asIScriptGeneric *gen,
              asUINT argumentIndex,
              const char *elementDeclaration,
              std::vector<T> &out) {
    Access array;
    int status = Open(owner, gen, argumentIndex, elementDeclaration, array);
    if (status != BML_OK)
        return status;
    CKDWORD size = 0;
    if ((status = Size(array, size)) != BML_OK)
        return status;
    try {
        std::vector<T> values;
        values.reserve(size);
        for (CKDWORD index = 0; index < size; ++index) {
            const void *element = nullptr;
            if (array.Api->ArrayGetConstElementAddress(array.Array, index, &element) != CKAS_OK || !element)
                return BML_ERROR_FAIL;
            values.push_back(*static_cast<const T *>(element));
        }
        out = std::move(values);
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
}

inline int ReadBool(ScriptMod *owner,
                    asIScriptGeneric *gen,
                    asUINT argumentIndex,
                    std::vector<int> &out) {
    Access array;
    int status = Open(owner, gen, argumentIndex, "bool", array);
    if (status != BML_OK)
        return status;
    CKDWORD size = 0;
    if ((status = Size(array, size)) != BML_OK)
        return status;
    try {
        std::vector<int> values;
        values.reserve(size);
        for (CKDWORD index = 0; index < size; ++index) {
            const void *element = nullptr;
            if (array.Api->ArrayGetConstElementAddress(array.Array, index, &element) != CKAS_OK || !element)
                return BML_ERROR_FAIL;
            values.push_back(*static_cast<const asBYTE *>(element) != 0 ? 1 : 0);
        }
        out = std::move(values);
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
}

inline int ReadString(ScriptMod *owner,
                      asIScriptGeneric *gen,
                      asUINT argumentIndex,
                      std::vector<std::string> &out) {
    Access array;
    int status = Open(owner, gen, argumentIndex, "string", array);
    if (status != BML_OK)
        return status;
    CKDWORD size = 0;
    if ((status = Size(array, size)) != BML_OK)
        return status;
    try {
        std::vector<std::string> values;
        values.reserve(size);
        for (CKDWORD index = 0; index < size; ++index) {
            const void *element = nullptr;
            std::string value;
            if (array.Api->ArrayGetConstElementAddress(array.Array, index, &element) != CKAS_OK || !element ||
                !ScriptStringInterop::ReadStringObject(array.Engine, element, value)) {
                return BML_ERROR_FAIL;
            }
            values.push_back(std::move(value));
        }
        out = std::move(values);
        return BML_OK;
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    }
}

template <typename T>
int WriteFixed(ScriptMod *owner,
               asIScriptGeneric *gen,
               asUINT argumentIndex,
               const char *elementDeclaration,
               const T *values,
               size_t count) {
    if (count > static_cast<size_t>(std::numeric_limits<CKDWORD>::max()))
        return BML_ERROR_OUT_OF_MEMORY;
    Access array;
    int status = Open(owner, gen, argumentIndex, elementDeclaration, array);
    if (status != BML_OK)
        return status;
    if (array.Api->ArrayResize(array.Array, static_cast<CKDWORD>(count)) != CKAS_OK)
        return BML_ERROR_OUT_OF_MEMORY;
    for (CKDWORD index = 0; index < static_cast<CKDWORD>(count); ++index) {
        void *element = nullptr;
        if (array.Api->ArrayGetElementAddress(array.Array, index, &element) != CKAS_OK || !element)
            return BML_ERROR_FAIL;
        *static_cast<T *>(element) = values[index];
    }
    return BML_OK;
}

inline int WriteBool(ScriptMod *owner,
                     asIScriptGeneric *gen,
                     asUINT argumentIndex,
                     const int *values,
                     size_t count) {
    if (count > static_cast<size_t>(std::numeric_limits<CKDWORD>::max()))
        return BML_ERROR_OUT_OF_MEMORY;
    Access array;
    int status = Open(owner, gen, argumentIndex, "bool", array);
    if (status != BML_OK)
        return status;
    if (array.Api->ArrayResize(array.Array, static_cast<CKDWORD>(count)) != CKAS_OK)
        return BML_ERROR_OUT_OF_MEMORY;
    for (CKDWORD index = 0; index < static_cast<CKDWORD>(count); ++index) {
        void *element = nullptr;
        if (array.Api->ArrayGetElementAddress(array.Array, index, &element) != CKAS_OK || !element)
            return BML_ERROR_FAIL;
        *static_cast<asBYTE *>(element) = values[index] != 0 ? 1 : 0;
    }
    return BML_OK;
}

inline int WriteString(ScriptMod *owner,
                       asIScriptGeneric *gen,
                       asUINT argumentIndex,
                       const std::vector<std::string> &values) {
    if (values.size() > static_cast<size_t>(std::numeric_limits<CKDWORD>::max()))
        return BML_ERROR_OUT_OF_MEMORY;
    Access array;
    int status = Open(owner, gen, argumentIndex, "string", array);
    if (status != BML_OK)
        return status;
    if (array.Api->ArrayResize(array.Array, static_cast<CKDWORD>(values.size())) != CKAS_OK)
        return BML_ERROR_OUT_OF_MEMORY;
    for (CKDWORD index = 0; index < static_cast<CKDWORD>(values.size()); ++index) {
        void *element = nullptr;
        if (array.Api->ArrayGetElementAddress(array.Array, index, &element) != CKAS_OK || !element ||
            !ScriptStringInterop::AssignStringObject(array.Engine, element, values[index])) {
            return BML_ERROR_FAIL;
        }
    }
    return BML_OK;
}

} // namespace BML::ScriptInteropArray

#endif // BML_SCRIPT_INTEROP_ARRAY_H
