#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>

#include "PlayerProbe.h"
#include "test_scriptinterop_imc.hpp"

namespace {

namespace Interop = BML::Imc::Generated::Test::Scriptinterop;

class ScriptImcInteropTest final : public IMod {
public:
    explicit ScriptImcInteropTest(IBML *bml) : IMod(bml) {
        AddDependency("BML");
    }

    const char *GetID() override { return "ScriptImcInteropTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Script IMC Interop Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Exercises native and ASMod IMC in both directions";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        BML::PlayerTest::ProbeReport::Reset();

        Interop::Provider::Handlers handlers;
        handlers.Userdata = this;
        handlers.NativeEcho = &NativeEcho;
        const int providerStatus = m_Provider.Start(handlers);
        const int clientStatus = m_Client.Open();
        const int subscriptionStatus = m_Client.SubscribeScriptNotice(
            m_ScriptNotice, &OnScriptNotice, this, 8);
        m_Ready = providerStatus == BML_OK && clientStatus == BML_OK &&
                  subscriptionStatus == BML_OK;
        GetLogger()->Info("Script IMC native setup: provider=%d client=%d "
                          "subscription=%d",
                          providerStatus, clientStatus, subscriptionStatus);
        if (!m_Ready)
            Finish(false, "transport-open");
    }

    void OnProcess() override {
        if (BML::PlayerTest::ProbeReport::Reported() || !m_Ready)
            return;

        if (!m_ScriptRpcChecked) {
            bool available = false;
            if (m_Client.IsScriptEchoAvailable(available) == BML_OK && available) {
                Interop::NumberValue request;
                request.Value = 30;
                Interop::NumberValue response;
                m_ScriptRpcChecked = true;
                const int status = m_Client.CallScriptEcho(request, response);
                m_ScriptRpcPassed = status == BML_OK && response.Value == 31;
                GetLogger()->Info("Script IMC native RPC: status=%d value=%d",
                                  status, response.Value);
            }
        }

        if (!m_NativeNoticePublished) {
            std::size_t subscribers = 0;
            if (m_Client.GetNativeNoticeSubscriberCount(subscribers) == BML_OK &&
                subscribers > 0) {
                Interop::NumberValue notice;
                notice.Value = 40;
                std::size_t delivered = 0;
                m_NativeNoticePublished =
                    m_Client.PublishNativeNotice(notice, &delivered) == BML_OK &&
                    delivered > 0;
            }
        }

        const bool passed = m_ScriptRpcPassed && m_NativeNoticePublished &&
                            m_InitialScriptCall && m_NoticeAcknowledged &&
                            m_ScriptNoticeReceived;
        if (passed) {
            GetLogger()->Info(
                "Script IMC native interop: status=pass rpc_client=true "
                "rpc_provider=true topic_subscriber=true topic_publisher=true");
            Finish(true, "completed");
        } else if (++m_Frames > 600) {
            GetLogger()->Error(
                "Script IMC native timeout: rpc=%s published=%s "
                "initial=%s acknowledged=%s received=%s",
                m_ScriptRpcPassed ? "true" : "false",
                m_NativeNoticePublished ? "true" : "false",
                m_InitialScriptCall ? "true" : "false",
                m_NoticeAcknowledged ? "true" : "false",
                m_ScriptNoticeReceived ? "true" : "false");
            Finish(false, "interop-timeout");
        }
    }

    void OnUnload() override {
        (void)m_ScriptNotice.Close();
        (void)m_Client.Close();
        (void)m_Provider.Close();
    }

private:
    static int NativeEcho(const Interop::NumberValue &request,
                          Interop::NumberValue &response, void *userdata) {
        auto *self = static_cast<ScriptImcInteropTest *>(userdata);
        if (!self)
            return BML_ERROR_INVALID_PARAMETER;
        response.Value = request.Value + 1;
        if (request.Value == 20)
            self->m_InitialScriptCall = true;
        else if (request.Value == 40)
            self->m_NoticeAcknowledged = true;
        else
            return BML_ERROR_INVALID_PARAMETER;
        return BML_OK;
    }

    static void OnScriptNotice(int status, Interop::NumberValue *notice,
                               const BML_ImcMessage *, void *userdata) {
        auto *self = static_cast<ScriptImcInteropTest *>(userdata);
        if (self && status == BML_OK && notice && notice->Value == 50)
            self->m_ScriptNoticeReceived = true;
    }

    static void Finish(bool passed, const char *reason) {
        if (passed)
            BML::PlayerTest::ProbeReport::Pass(reason);
        else
            BML::PlayerTest::ProbeReport::Fail(reason);
    }

    Interop::Provider m_Provider;
    Interop::Client m_Client;
    Interop::ScriptNoticeSubscription m_ScriptNotice;
    int m_Frames = 0;
    bool m_Ready = false;
    bool m_ScriptRpcChecked = false;
    bool m_ScriptRpcPassed = false;
    bool m_NativeNoticePublished = false;
    bool m_InitialScriptCall = false;
    bool m_NoticeAcknowledged = false;
    bool m_ScriptNoticeReceived = false;
};

} // namespace

BML_PLAYER_PROBE_READ_EXPORT()

MOD_EXPORT IMod *BMLEntry(IBML *bml) { return new ScriptImcInteropTest(bml); }

MOD_EXPORT void BMLExit(IMod *mod) { delete mod; }
