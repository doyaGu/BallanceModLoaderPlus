#ifndef BML_SCRIPTIMCSERVICE_H
#define BML_SCRIPTIMCSERVICE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "BML/Defines.h"
#include "BML/Imc.h"
#include "ScriptDiagnostic.h"

class ModContext;
class asIScriptFunction;

namespace BML {

class ScriptImcRecord;
class ScriptMod;
class ScriptImcServiceState;

class ScriptImcReply final {
public:
    explicit ScriptImcReply(bool expectsResponse = false);

    void Complete(int status);
    void Complete(int status, const ScriptImcRecord &record);
    int Commit(BML_ImcResponse *response,
               BML_ImcPayloadTypeId responsePayload) const;

private:
    bool m_ExpectsResponse = false;
    bool m_Completed = false;
    int m_Status = BML_ERROR_IMC_TARGET_EXECUTION_FAILED;
    std::vector<std::uint8_t> m_Bytes;
};

class ScriptImcRequestRef final {
public:
    struct Control;

    void AddRef();
    void Release();
    bool IsValid() const;
    bool IsComplete() const;
    int GetStatus() const;
    int Cancel();

private:
    friend class ScriptImcService;
    ScriptImcRequestRef(std::weak_ptr<ScriptImcServiceState> state,
                        std::shared_ptr<Control> control);

    std::atomic<int> m_RefCount{1};
    std::weak_ptr<ScriptImcServiceState> m_State;
    std::shared_ptr<Control> m_Control;
};

class ScriptImcSubscriptionRef final {
public:
    struct Control;

    void AddRef();
    void Release();
    bool IsValid() const;
    int GetStatus() const;
    int GetDroppedCount(std::uint64_t &count) const;
    int Cancel();

private:
    friend class ScriptImcService;
    ScriptImcSubscriptionRef(std::weak_ptr<ScriptImcServiceState> state,
                             std::shared_ptr<Control> control);

    std::atomic<int> m_RefCount{1};
    std::weak_ptr<ScriptImcServiceState> m_State;
    std::shared_ptr<Control> m_Control;
};

class ScriptImcProviderRef final {
public:
    struct Control;

    void AddRef();
    void Release();
    bool IsOpen() const;
    int GetStatus() const;
    int RegisterRpc(const std::string &route,
                    const std::string &requestPayload,
                    const std::string &responsePayload,
                    asIScriptFunction *handler);
    int UnregisterRpc(const std::string &route);
    int Publish(const std::string &topic,
                const std::string &payload,
                const ScriptImcRecord &message,
                std::uint64_t &delivered);
    int GetSubscriberCount(const std::string &topic,
                           std::uint64_t &count) const;
    int Close();

private:
    friend class ScriptImcService;
    ScriptImcProviderRef(std::weak_ptr<ScriptImcServiceState> state,
                         std::shared_ptr<Control> control);
    ~ScriptImcProviderRef();

    std::atomic<int> m_RefCount{1};
    std::weak_ptr<ScriptImcServiceState> m_State;
    std::shared_ptr<Control> m_Control;
};

class ScriptImcService final {
public:
    ScriptImcService();
    ~ScriptImcService();

    bool Bind(ModContext *context, ScriptMod *owner);
    int IsRpcAvailable(const std::string &route, bool &available);
    ScriptImcRequestRef *Call(const std::string &route,
                              const std::string &requestPayload,
                              const std::string &responsePayload,
                              const ScriptImcRecord &request,
                              asIScriptFunction *callback,
                              unsigned int timeoutMs);
    ScriptImcSubscriptionRef *Subscribe(const std::string &topic,
                                        const std::string &payload,
                                        asIScriptFunction *callback,
                                        unsigned int capacity);
    int Publish(const std::string &topic,
                const std::string &payload,
                const ScriptImcRecord &message,
                std::uint64_t &delivered);
    int GetSubscriberCount(const std::string &topic, std::uint64_t &count);
    ScriptImcProviderRef *OpenProvider();
    void ProcessQueuedCallbacks();
    void Release(ScriptDiagnostic *diagnostic = nullptr);
    std::size_t GetActiveCount() const;
    std::size_t GetQueuedCallbackCount() const;

private:
    std::shared_ptr<ScriptImcServiceState> m_State;
};

} // namespace BML

#endif
