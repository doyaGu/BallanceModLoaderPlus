#ifndef BML_ENGINE_HPP
#define BML_ENGINE_HPP
// Opt-in CK2/VxMath ABI. Link BML::Engine when including this header.
#include "BML/Engine.h"
#include "BML/Mod.hpp"
#include "CKAll.h"

namespace BML::Engine {
namespace Detail {
inline const BML_EngineInterface *Api() noexcept {
    return BML::Detail::Bind<BML_EngineInterface>(
        BML_ENGINE_INTERFACE_ID, 1, BML_ENGINE_INTERFACE_1_0_SIZE,
        +[](const BML_EngineInterface *table) {
            return BML_IFACE_HAS(table, BML_EngineInterface, BorrowContext) &&
                   BML_IFACE_HAS(table, BML_EngineInterface, ReferenceObject) &&
                   BML_IFACE_HAS(table, BML_EngineInterface, BorrowObject);
        });
}
template <class> struct Class;
#define BML_ENGINE_CLASS(Type, Id)                                                                 \
    template <> struct Class<Type> {                                                               \
        static constexpr uint32_t Value = Id;                                                      \
    }
BML_ENGINE_CLASS(CKObject, CKCID_OBJECT);
BML_ENGINE_CLASS(CKBeObject, CKCID_BEOBJECT);
BML_ENGINE_CLASS(CKBehavior, CKCID_BEHAVIOR);
BML_ENGINE_CLASS(CKDataArray, CKCID_DATAARRAY);
BML_ENGINE_CLASS(CKGroup, CKCID_GROUP);
BML_ENGINE_CLASS(CKMaterial, CKCID_MATERIAL);
BML_ENGINE_CLASS(CKMesh, CKCID_MESH);
BML_ENGINE_CLASS(CKTexture, CKCID_TEXTURE);
BML_ENGINE_CLASS(CK3dEntity, CKCID_3DENTITY);
BML_ENGINE_CLASS(CK3dObject, CKCID_3DOBJECT);
BML_ENGINE_CLASS(CK2dEntity, CKCID_2DENTITY);
BML_ENGINE_CLASS(CKCamera, CKCID_CAMERA);
BML_ENGINE_CLASS(CKTargetCamera, CKCID_TARGETCAMERA);
BML_ENGINE_CLASS(CKLight, CKCID_LIGHT);
BML_ENGINE_CLASS(CKTargetLight, CKCID_TARGETLIGHT);
BML_ENGINE_CLASS(CKSound, CKCID_SOUND);
BML_ENGINE_CLASS(CKSprite, CKCID_SPRITE);
BML_ENGINE_CLASS(CKScene, CKCID_SCENE);
BML_ENGINE_CLASS(CKLevel, CKCID_LEVEL);
#undef BML_ENGINE_CLASS
} // namespace Detail
[[nodiscard]] inline Result<CKContext *> BorrowContext(const Mod::Context &ctx) try {
    void *value = nullptr;
    const auto *api = Detail::Api();
    const int code = api ? api->BorrowContext(ctx.Handle(), &value) : BML_ERROR_UNAVAILABLE;
    return code == BML_OK ? Result<CKContext *>::Success(static_cast<CKContext *>(value))
                          : Result<CKContext *>::Failure(code);
} catch (const std::bad_alloc &) {
    return Result<CKContext *>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<CKContext *>::Failure(BML_ERROR_FAIL);
}
[[nodiscard]] inline Result<CKRenderContext *> BorrowRenderContext(const Mod::Context &ctx) try {
    auto context = BorrowContext(ctx);
    if (!context)
        return Result<CKRenderContext *>::Failure(context.Code());
    auto *value = context.Value()->GetPlayerRenderContext();
    return value ? Result<CKRenderContext *>::Success(value)
                 : Result<CKRenderContext *>::Failure(BML_ERROR_UNAVAILABLE);
} catch (const std::bad_alloc &) {
    return Result<CKRenderContext *>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<CKRenderContext *>::Failure(BML_ERROR_FAIL);
}
[[nodiscard]] inline Result<BML_ObjectRef> ReferenceObject(const Mod::Context &ctx,
                                                           const CKObject *object) try {
    BML_ObjectRef value{};
    const auto *api = Detail::Api();
    const int code =
        api ? api->ReferenceObject(ctx.Handle(), object, &value) : BML_ERROR_UNAVAILABLE;
    return code == BML_OK ? Result<BML_ObjectRef>::Success(value)
                          : Result<BML_ObjectRef>::Failure(code);
} catch (const std::bad_alloc &) {
    return Result<BML_ObjectRef>::Failure(BML_ERROR_OUT_OF_MEMORY);
} catch (...) {
    return Result<BML_ObjectRef>::Failure(BML_ERROR_FAIL);
}

template <class T>
[[nodiscard]] Result<T *> BorrowObject(const Mod::Context &ctx, BML_ObjectRef object) {
    static_assert(std::is_base_of_v<CKObject, T>);
    void *value = nullptr;
    const auto *api = Detail::Api();
    const int code = api ? api->BorrowObject(ctx.Handle(), object, Detail::Class<T>::Value, &value)
                         : BML_ERROR_UNAVAILABLE;
    return code == BML_OK ? Result<T *>::Success(static_cast<T *>(value))
                          : Result<T *>::Failure(code);
}
} // namespace BML::Engine
#endif
