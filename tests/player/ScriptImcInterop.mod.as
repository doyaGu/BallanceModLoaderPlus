#include "test_scriptinterop_imc.as"

[bml.mod id="bml.script.imc.interop"
         name="BML Script IMC Interop"
         version="1.0.0"
         author="BML"
         description="Exercises generated IMC bindings from an ASMod"
         bml="0.3.13"]
class ScriptImcInterop {
    Test::Scriptinterop::Provider provider;
    BML::ImcSubscriptionRef@ nativeNotice;
    BML::ImcRequestRef@ initialCall;
    BML::ImcRequestRef@ noticeAck;
    bool providerReady = false;
    bool initialReply = false;
    bool nativeNoticeReceived = false;
    bool noticeAcknowledged = false;
    bool scriptNoticePublished = false;
    bool reported = false;

    void OnLoad(const BML::ModContext &in ctx) {
        Test::Scriptinterop::Handlers handlers;
        @handlers.ScriptEcho = Test::Scriptinterop::ScriptEchoHandler(this.OnScriptEcho);
        int providerStatus = provider.Start(ctx, handlers);
        providerReady = providerStatus == BML::ERROR_OK;

        Test::Scriptinterop::NativeNoticeCallback@ notice =
            Test::Scriptinterop::NativeNoticeCallback(this.OnNativeNotice);
        @nativeNotice = Test::Scriptinterop::SubscribeNativeNotice(ctx, notice, 8);

        int subscriptionStatus = nativeNotice is null
            ? BML::ERROR_INVALID_HANDLE : nativeNotice.Status;
        ctx.LogInfo("Script IMC setup: provider=" + providerStatus +
                    " subscription=" + subscriptionStatus);
    }

    int OnScriptEcho(const Test::Scriptinterop::Number &in request,
                     Test::Scriptinterop::Number &out response) {
        response.Value = request.Value + 1;
        return BML::ERROR_OK;
    }

    void OnNativeEcho(int status,
                      const Test::Scriptinterop::Number &in response) {
        if (status != BML::ERROR_OK)
            return;
        if (response.Value == 21)
            initialReply = true;
        else if (response.Value == 41)
            noticeAcknowledged = true;
    }

    void OnNativeNotice(int status,
                        const Test::Scriptinterop::Number &in message) {
        if (status != BML::ERROR_OK || message.Value != 40)
            return;
        nativeNoticeReceived = true;
        // The callback receives no ModContext, so the acknowledgement is
        // started from OnProcess after this flag is observed.
    }

    void OnProcess(const BML::ModContext &in ctx) {
        bool nativeEchoAvailable = false;
        if (initialCall is null &&
            Test::Scriptinterop::IsNativeEchoAvailable(
                ctx, nativeEchoAvailable) == BML::ERROR_OK &&
            nativeEchoAvailable) {
            Test::Scriptinterop::Number request;
            request.Value = 20;
            Test::Scriptinterop::NativeEchoCallback@ completion =
                Test::Scriptinterop::NativeEchoCallback(this.OnNativeEcho);
            @initialCall = Test::Scriptinterop::BeginCallNativeEcho(
                ctx, request, completion);
        }

        if (nativeNoticeReceived && noticeAck is null) {
            Test::Scriptinterop::Number request;
            request.Value = 40;
            Test::Scriptinterop::NativeEchoCallback@ completion =
                Test::Scriptinterop::NativeEchoCallback(this.OnNativeEcho);
            @noticeAck = Test::Scriptinterop::BeginCallNativeEcho(
                ctx, request, completion);
        }

        if (!scriptNoticePublished) {
            uint64 subscribers = 0;
            if (Test::Scriptinterop::GetScriptNoticeSubscriberCount(
                    ctx, subscribers) == BML::ERROR_OK && subscribers > 0) {
                Test::Scriptinterop::Number notice;
                notice.Value = 50;
                uint64 delivered = 0;
                scriptNoticePublished =
                    Test::Scriptinterop::PublishScriptNotice(
                        ctx, notice, delivered) == BML::ERROR_OK && delivered > 0;
            }
        }

        if (!reported && providerReady && nativeNotice !is null &&
            initialReply && nativeNoticeReceived && noticeAcknowledged &&
            scriptNoticePublished) {
            uint64 dropped = 0;
            bool handlesOk = initialCall !is null && initialCall.IsComplete &&
                initialCall.Status == BML::ERROR_OK && noticeAck !is null &&
                noticeAck.IsComplete && noticeAck.Status == BML::ERROR_OK &&
                nativeNotice.GetDroppedCount(dropped) == BML::ERROR_OK &&
                dropped == 0;
            if (!handlesOk)
                return;
            ctx.LogInfo("Script IMC interop: status=pass rpc_client=true " +
                        "rpc_provider=true topic_subscriber=true topic_publisher=true " +
                        "handles=true");
            reported = true;
        }
    }

    void OnUnload(const BML::ModContext &in ctx) {
        int subscriptionStatus = nativeNotice is null
            ? BML::ERROR_INVALID_HANDLE : nativeNotice.Cancel();
        @nativeNotice = null;
        int providerStatus = provider.Close();
        ctx.LogInfo("Script IMC interop unload: provider_closed=" +
                    (providerStatus == BML::ERROR_OK ? "true" : "false") +
                    " subscription_cancelled=" +
                    (subscriptionStatus == BML::ERROR_OK ? "true" : "false"));
    }
}
