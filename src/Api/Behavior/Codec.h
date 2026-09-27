#ifndef BML_API_BEHAVIOR_CODEC_H
#define BML_API_BEHAVIOR_CODEC_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Api/Behavior/Text.h"
#include "BML/Behavior.h"
#include "Behavior/FrameStore.h"
#include "Behavior/Install/Installations.h"
#include "Behavior/Script.h"
#include "Behavior/Sessions.h"
#include "Behavior/Watch.h"

class ModContext;

// Conversions between the bml.behavior wire records and the Behavior
// internals. Table entries check their pointers, make one Sessions call and
// hand the result to one of these readers or writers; nothing here takes the
// ModContext lease or checks the thread.
namespace BML::Api::Behavior {

using BML::Behavior::Internal::AdmissionState;
using BML::Behavior::Internal::BlockSpec;
using BML::Behavior::Internal::Error;
using BML::Behavior::Internal::ExecutionError;
using BML::Behavior::Internal::FrameRetention;
using BML::Behavior::Internal::FrameStore;
using BML::Behavior::Internal::GraphModel;
using BML::Behavior::Internal::GraphValue;
using BML::Behavior::Internal::GraphView;
using BML::Behavior::Internal::Layout;
using BML::Behavior::Internal::PatchId;
using BML::Behavior::Internal::PatchInfo;
using BML::Behavior::Internal::PlanId;
using BML::Behavior::Internal::PlanInfo;
using BML::Behavior::Internal::Program;
using BML::Behavior::Internal::PrototypeInfo;
using BML::Behavior::Internal::PrototypeQuery;
using BML::Behavior::Internal::PrototypeRef;
using BML::Behavior::Internal::ReadMode;
using BML::Behavior::Internal::RunInfo;
using BML::Behavior::Internal::RunResult;
using BML::Behavior::Internal::ScriptId;
using BML::Behavior::Internal::ScriptInfo;
using BML::Behavior::Internal::ScriptResult;
using BML::Behavior::Internal::SessionOwner;
using BML::Behavior::Internal::Sessions;
using BML::Behavior::Internal::Slot;
using BML::Behavior::Internal::SlotKind;
using BML::Behavior::Internal::Status;
using BML::Behavior::Internal::WatchSpec;
using Installations = BML::Behavior::Internal::Installations;
namespace Parameter = BML::Behavior::Internal::Parameter;

template <typename T>
bool HasStructSize(const T *value) noexcept {
    return value && value->StructSize >= sizeof(T);
}

bool FitsStrided(std::size_t count, std::size_t stride,
                 std::size_t recordSize) noexcept;
bool ReadString(BML_BehaviorString value, std::string &out,
                bool allowNul = false);
// Reads text that goes to Virtools, such as a name or a string value, in the
// engine's code page. BML identifiers are read with ReadString and stay UTF-8.
bool ReadNativeString(BML_BehaviorString value, std::string &out);
CKGUID Guid(BML_BehaviorGuid value) noexcept;
BML_BehaviorGuid Guid(CKGUID value) noexcept;

// Handles carry the internal ids unchanged.
inline std::uintptr_t SessionId(BML_BehaviorSession session) noexcept {
    return reinterpret_cast<std::uintptr_t>(session);
}

inline std::uintptr_t RunId(BML_BehaviorRun run) noexcept {
    return reinterpret_cast<std::uintptr_t>(run);
}

inline std::uintptr_t WatchId(BML_BehaviorWatch watch) noexcept {
    return reinterpret_cast<std::uintptr_t>(watch);
}

inline ScriptId ScriptIdOf(BML_BehaviorScript script) noexcept {
    return static_cast<ScriptId>(reinterpret_cast<std::uintptr_t>(script));
}

inline PatchId PatchIdOf(BML_BehaviorPatch patch) noexcept {
    return static_cast<PatchId>(reinterpret_cast<std::uintptr_t>(patch));
}

inline PlanId PlanIdOf(BML_BehaviorPlan plan) noexcept {
    return static_cast<PlanId>(reinterpret_cast<std::uintptr_t>(plan));
}

inline BML_BehaviorSession SessionHandle(std::uintptr_t id) noexcept {
    return reinterpret_cast<BML_BehaviorSession>(id);
}

inline BML_BehaviorRun RunHandle(std::uintptr_t id) noexcept {
    return reinterpret_cast<BML_BehaviorRun>(id);
}

inline BML_BehaviorWatch WatchHandle(std::uintptr_t id) noexcept {
    return reinterpret_cast<BML_BehaviorWatch>(id);
}

inline BML_BehaviorScript ScriptHandle(ScriptId id) noexcept {
    return reinterpret_cast<BML_BehaviorScript>(
        static_cast<std::uintptr_t>(id));
}

inline BML_BehaviorPatch PatchHandleOf(PatchId id) noexcept {
    return reinterpret_cast<BML_BehaviorPatch>(
        static_cast<std::uintptr_t>(id));
}

inline BML_BehaviorPlan PlanHandleOf(PlanId id) noexcept {
    return reinterpret_cast<BML_BehaviorPlan>(static_cast<std::uintptr_t>(id));
}

// Status. PrepareStatus resets a caller's record before any other check, and
// ResultCode is the one mapping from an internal Status to a C result.
std::uint32_t PublicError(Error error) noexcept;
std::uint32_t PublicError(ExecutionError error) noexcept;
void WriteStatus(BML_BehaviorStatus *out, const Status &status) noexcept;
bool PrepareStatus(BML_BehaviorStatus *out) noexcept;
bool ValidOutputs(BML_BehaviorRunInfo *info,
                  BML_BehaviorStatus *status) noexcept;
int ResultCode(const Status &status) noexcept;
Status InvalidValue(std::string message);

// Readers. Each one leaves the reason for a rejected record in status.
bool ReadSelector(const BML_BehaviorSelector &from, SlotKind slotKind,
                  CKGUID type, Slot &to, Status &status);
bool ReadSlotKind(std::uint32_t kind, SlotKind &out) noexcept;
// A Slot kind that carries a value: Target, Pin, Pout, Setting or Local.
bool ReadDataSlotKind(std::uint32_t kind, SlotKind &out) noexcept;
bool ReadLiveSlot(const BML_BehaviorSlotRef &from, Slot &slot,
                  Status &status);
bool ReadValue(const BML_BehaviorValue &from, ModContext &context,
               Parameter::Binding &to, Status &status);
bool ReadBindings(const BML_BehaviorBinding *bindings, std::uint32_t count,
                  SlotKind kind, ModContext &context, BlockSpec &block,
                  Status &status);
bool ReadBlock(const BML_BehaviorBlock &from, ModContext &context,
               BlockSpec &to, Status &status);
bool ReadFrames(const BML_BehaviorFramePolicy &from,
                FrameRetention &retention, Status &status);
bool ReadPrototypeQuery(const BML_BehaviorPrototypeQuery &from,
                        PrototypeQuery &to, Status &status);
// A stale reference, or one that names the wrong kind of object, fails with
// BML_ERROR_OBJECT_INVALID, as it does everywhere a call takes an ObjectRef.
CKBeObject *ReadOwner(BML_ObjectRef owner, ModContext &context,
                      Status &status);
CKBehavior *ReadBehavior(BML_ObjectRef reference, ModContext &context,
                         Status &status);
bool ReadPortQuery(const BML_BehaviorPortRef &from,
                   Installations::PortQuery &out, Status &status);
bool ReadNodeRef(const BML_BehaviorNodeRef &from,
                 Installations::SymbolRef &out, Status &status);
bool ReadPlanInstance(const BML_BehaviorPlanInstance &from,
                      Installations::PlanInstance &out, Status &status);
// Answers BML_OK or the result code of the first rejected field. A Graph
// Watch resolves root and every other kind resolves node.
int ReadWatchSpec(const BML_BehaviorWatchSpec &from, ModContext &context,
                  WatchSpec &spec, CKBehavior *&root, CKBehavior *&node,
                  Status &status);
// Takes the caller's reference through Retain. Edit programs and AttachHook
// share it.
Status ReadHook(const BML_BehaviorHookFunction *from,
                BML::Behavior::Internal::HookBlock::Hook &out);

// Info records.
void WriteRunInfo(BML_BehaviorRunInfo *out, const RunInfo &info) noexcept;
void WriteScriptInfo(BML_BehaviorScriptInfo *out,
                     const ScriptInfo &info) noexcept;
void WriteWatchInfo(BML_BehaviorWatchInfo *out,
                    const BML::Behavior::Internal::WatchInfo &info) noexcept;
void WritePlanInfo(BML_BehaviorPlanInfo *out, const PlanInfo &info) noexcept;
void WritePatchInfo(BML_BehaviorPatchInfo *out,
                    const PatchInfo &info) noexcept;
void WriteFailures(BML_BehaviorFailures *out, const Status &apply,
                   const Status &restore) noexcept;
BML_BehaviorPlanInstance WritePlanInstance(
    const Installations::PlanInstance &from) noexcept;

// Two-call buffer writers. Each reports the sizes it needs, answers
// BML_ERROR_BUFFER_TOO_SMALL when the caller's buffers are short and fills
// them only when everything fits.
int WritePrototypes(const std::vector<PrototypeInfo> &found,
                    BML_BehaviorPrototypeInfo *prototypes,
                    std::uint32_t prototypeCapacity,
                    std::uint32_t prototypeStride, void *payload,
                    std::uint32_t payloadCapacity,
                    std::uint32_t *outPrototypeCount,
                    std::uint32_t *outPayloadSize);
int WriteLayoutResult(const Layout &source, BML_BehaviorLayout *layout,
                      void *payload, std::uint32_t payloadCapacity,
                      std::uint32_t *outPayloadSize);
int WriteGraphResult(const GraphModel &source, BML_BehaviorGraph *graph,
                     void *payload, std::uint32_t payloadCapacity,
                     std::uint32_t *outPayloadSize);
int WriteGraphValueResult(const GraphValue &source,
                          BML_BehaviorGraphValue *value,
                          void *payload, std::uint32_t payloadCapacity,
                          std::uint32_t *outPayloadSize);
int WritePlanInstances(const std::vector<Installations::PlanInstance> &found,
                       BML_BehaviorPlanInstance *instances,
                       std::uint32_t instanceCapacity,
                       std::uint32_t instanceStride,
                       std::uint32_t *outInstanceCount) noexcept;
// Drains the retained frames of one Run into the caller's buffers.
int WriteFrames(FrameStore &store, BML_BehaviorRunFrame *headers,
                std::uint32_t headerCapacity, std::uint32_t headerStride,
                void *payload, std::uint32_t payloadCapacity,
                std::uint32_t *outHeaderCount,
                std::uint32_t *outPayloadSize);

// Delivers each Watch event to a foreign callback. Every way the callback
// can fail comes back as a CallbackFailed Status for the Watch to keep.
BML::Behavior::Internal::WatchBinding::Function WatchThunk(
    ModContext &context, const BML_BehaviorWatchFunction &function);

// The only decoder from a wire edit program to a Program. Symbols, when
// asked for, reports every Node and appended Port the author can address,
// under the caller's binding.
Status DecodeProgram(const BML_BehaviorEditProgram &program,
                     ModContext &context, Program &out,
                     Installations::SymbolMap *symbols = nullptr,
                     std::uint64_t binding = 0);
Status ReadScriptEdits(const BML_BehaviorScriptEdit *edits,
                       std::uint32_t count, ModContext &context,
                       std::vector<Installations::Rule> &out);
Status ReadGraphEdits(const BML_BehaviorGraphEdit *edits,
                      std::uint32_t count, ModContext &context,
                      std::vector<Installations::Target> &out);

} // namespace BML::Api::Behavior

#endif // BML_API_BEHAVIOR_CODEC_H
