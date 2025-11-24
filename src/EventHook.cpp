#include "EventHook.h"

#include "BMLMod.h"

#include "BML/ExecuteBB.h"
#include "BML/IBML.h"
#include "BML/IMessageReceiver.h"
#include "BML/ScriptHelper.h"

using namespace ScriptHelper;

namespace {
using Receiver = IMessageReceiver;

template<void (Receiver::*Method)()>
int Dispatch(const CKBehaviorContext *, void *arg) {
    auto *receiver = static_cast<Receiver *>(arg);
    (receiver->*Method)();
    return CKBR_OK;
}

template<void (Receiver::*Method)()>
CKBehavior *CreateEventHook(CKBehavior *script, Receiver &receiver) {
    return ExecuteBB::CreateHookBlock(script, &Dispatch<Method>, &receiver);
}
} // namespace

EventHookRegistrar::EventHookRegistrar(BMLMod &mod, IBML &bml)
    : m_Mod(mod), m_BML(bml), m_Receiver(bml) {}

void EventHookRegistrar::RegisterBaseEventHandler(CKBehavior *script) {
    CKBehavior *som = FindFirstBB(script, "Switch On Message", false, 2, 11, 11, 0);

    m_Mod.GetLogger()->Info("Insert message Start Menu Hook");
    InsertBB(script, FindNextLink(script, FindNextBB(script, som, nullptr, 0, 0)),
             CreateEventHook<&Receiver::OnPreStartMenu>(script, m_Receiver));
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 0, 0)),
               CreateEventHook<&Receiver::OnPostStartMenu>(script, m_Receiver));

    m_Mod.GetLogger()->Info("Insert message Exit Game Hook");
    InsertBB(script, FindNextLink(script, FindNextBB(script, som, nullptr, 1, 0)),
             CreateEventHook<&Receiver::OnExitGame>(script, m_Receiver));

    m_Mod.GetLogger()->Info("Insert message Load Level Hook");
    CKBehaviorLink *link = FindNextLink(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, som, nullptr, 2, 0))));
    InsertBB(script, link, CreateEventHook<&Receiver::OnPreLoadLevel>(script, m_Receiver));
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 2, 0)),
               CreateEventHook<&Receiver::OnPostLoadLevel>(script, m_Receiver));

    m_Mod.GetLogger()->Info("Insert message Start Level Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 3, 0)),
               CreateEventHook<&Receiver::OnStartLevel>(script, m_Receiver));

    m_Mod.GetLogger()->Info("Insert message Reset Level Hook");
    CKBehavior *rl = FindFirstBB(script, "reset Level");
    link = FindNextLink(rl, FindNextBB(rl, FindNextBB(rl, rl->GetInput(0))));
    InsertBB(script, link, CreateEventHook<&Receiver::OnPreResetLevel>(script, m_Receiver));
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 4, 0)),
               CreateEventHook<&Receiver::OnPostResetLevel>(script, m_Receiver));

    m_Mod.GetLogger()->Info("Insert message Pause Level Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 5, 0)),
               CreateEventHook<&Receiver::OnPauseLevel>(script, m_Receiver));

    m_Mod.GetLogger()->Info("Insert message Unpause Level Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 6, 0)),
               CreateEventHook<&Receiver::OnUnpauseLevel>(script, m_Receiver));

    CKBehavior *bs = FindNextBB(script, FindFirstBB(script, "DeleteCollisionSurfaces"));

    m_Mod.GetLogger()->Info("Insert message Exit Level Hook");
    link = FindNextLink(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, som, nullptr, 7, 0))))));
    InsertBB(script, link, CreateEventHook<&Receiver::OnPreExitLevel>(script, m_Receiver));
    InsertBB(script, FindNextLink(script, FindNextBB(script, bs, nullptr, 0, 0)),
             CreateEventHook<&Receiver::OnPostExitLevel>(script, m_Receiver));

    m_Mod.GetLogger()->Info("Insert message Next Level Hook");
    link = FindNextLink(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, FindNextBB(script, som, nullptr, 8, 0))))));
    InsertBB(script, link, CreateEventHook<&Receiver::OnPreNextLevel>(script, m_Receiver));
    InsertBB(script, FindNextLink(script, FindNextBB(script, bs, nullptr, 1, 0)),
             CreateEventHook<&Receiver::OnPostNextLevel>(script, m_Receiver));

    m_Mod.GetLogger()->Info("Insert message Dead Hook");
    CreateLink(script, FindEndOfChain(script, FindNextBB(script, som, nullptr, 9, 0)),
               CreateEventHook<&Receiver::OnDead>(script, m_Receiver));

    CKBehavior *hs = FindFirstBB(script, "Highscore");
    hs->AddOutput("Out");
    FindBB(hs, [hs](CKBehavior *beh) {
        CreateLink(hs, beh, hs->GetOutput(0));
        return true;
    }, "Activate Script");

    m_Mod.GetLogger()->Info("Insert message End Level Hook");
    InsertBB(script, FindNextLink(script, FindNextBB(script, som, nullptr, 10, 0)),
             CreateEventHook<&Receiver::OnPreEndLevel>(script, m_Receiver));
    CreateLink(script, hs, CreateEventHook<&Receiver::OnPostEndLevel>(script, m_Receiver));
}

void EventHookRegistrar::RegisterGameplayIngame(CKBehavior *script) {
    m_Mod.GetLogger()->Info("Insert Ball/Camera Active/Inactive Hook");
    CKBehavior *camonoff = FindFirstBB(script, "CamNav On/Off");
    CKBehavior *ballonoff = FindFirstBB(script, "BallNav On/Off");
    CKMessageManager *mm = m_BML.GetMessageManager();
    CKMessageType camon = mm->AddMessageType("CamNav activate");
    CKMessageType camoff = mm->AddMessageType("CamNav deactivate");
    CKMessageType ballon = mm->AddMessageType("BallNav activate");
    CKMessageType balloff = mm->AddMessageType("BallNav deactivate");
    CKBehavior *con, *coff, *bon, *boff;
    FindBB(camonoff, [camon, camoff, &con, &coff](CKBehavior *beh) {
        auto msg = GetParamValue<CKMessageType>(beh->GetInputParameter(0)->GetDirectSource());
        if (msg == camon) con = beh;
        if (msg == camoff) coff = beh;
        return true;
    }, "Wait Message");
    CreateLink(camonoff, con, CreateEventHook<&Receiver::OnCamNavActive>(camonoff, m_Receiver), 0, 0);
    CreateLink(camonoff, coff, CreateEventHook<&Receiver::OnCamNavInactive>(camonoff, m_Receiver), 0, 0);
    FindBB(ballonoff, [ballon, balloff, &bon, &boff](CKBehavior *beh) {
        auto msg = GetParamValue<CKMessageType>(beh->GetInputParameter(0)->GetDirectSource());
        if (msg == ballon) bon = beh;
        if (msg == balloff) boff = beh;
        return true;
    }, "Wait Message");
    CreateLink(ballonoff, bon, CreateEventHook<&Receiver::OnBallNavActive>(ballonoff, m_Receiver), 0, 0);
    CreateLink(ballonoff, boff, CreateEventHook<&Receiver::OnBallNavInactive>(ballonoff, m_Receiver), 0, 0);

    m_Mod.m_CurLevel = m_BML.GetArrayByName("CurrentLevel");

    CKBehavior *ballMgr = FindFirstBB(script, "BallManager");
    CKBehavior *deactBall = FindFirstBB(ballMgr, "Deactivate Ball");
    CKBehavior *pieces = FindFirstBB(deactBall, "reset Ballpieces");
    m_Mod.m_OverclockLinks[0] = FindNextLink(deactBall, pieces);
    CKBehavior *unphy = FindNextBB(deactBall, FindNextBB(deactBall, m_Mod.m_OverclockLinks[0]->GetOutBehaviorIO()->GetOwner()));
    m_Mod.m_OverclockLinkIO[0][1] = unphy->GetInput(1);

    CKBehavior *newBall = FindFirstBB(ballMgr, "New Ball");
    CKBehavior *physicsNewBall = FindFirstBB(newBall, "physicalize new Ball");
    m_Mod.m_OverclockLinks[1] = FindPreviousLink(newBall, FindPreviousBB(newBall, FindPreviousBB(newBall, FindPreviousBB(newBall, physicsNewBall))));
    m_Mod.m_OverclockLinkIO[1][1] = physicsNewBall->GetInput(0);
}

void EventHookRegistrar::RegisterGameplayEnergy(CKBehavior *script) {
    m_Mod.GetLogger()->Info("Insert Counter Active/Inactive Hook");
    CKBehavior *som = FindFirstBB(script, "Switch On Message");
    InsertBB(script, FindNextLink(script, som, nullptr, 3), CreateEventHook<&Receiver::OnCounterActive>(script, m_Receiver));
    InsertBB(script, FindNextLink(script, som, nullptr, 1), CreateEventHook<&Receiver::OnCounterInactive>(script, m_Receiver));

    m_Mod.GetLogger()->Info("Insert Life/Point Hooks");
    CKMessageManager *mm = m_BML.GetMessageManager();
    CKMessageType lifeup = mm->AddMessageType("Life_Up"), balloff = mm->AddMessageType("Ball Off"),
        sublife = mm->AddMessageType("Sub Life"), extrapoint = mm->AddMessageType("Extrapoint");
    CKBehavior *lu, *bo, *sl, *ep;
    FindBB(script, [lifeup, balloff, sublife, extrapoint, &lu, &bo, &sl, &ep](CKBehavior *beh) {
        auto msg = GetParamValue<CKMessageType>(beh->GetInputParameter(0)->GetDirectSource());
        if (msg == lifeup) lu = beh;
        if (msg == balloff) bo = beh;
        if (msg == sublife) sl = beh;
        if (msg == extrapoint) ep = beh;
        return true;
    }, "Wait Message");
    CKBehavior *luhook = CreateEventHook<&Receiver::OnPreLifeUp>(script, m_Receiver);
    InsertBB(script, FindNextLink(script, lu, "add Life"), luhook);
    CreateLink(script, FindEndOfChain(script, luhook), CreateEventHook<&Receiver::OnPostLifeUp>(script, m_Receiver));
    InsertBB(script, FindNextLink(script, bo, "Delayer"), CreateEventHook<&Receiver::OnBallOff>(script, m_Receiver));
    CKBehavior *slhook = CreateEventHook<&Receiver::OnPreSubLife>(script, m_Receiver);
    InsertBB(script, FindNextLink(script, sl, "sub Life"), slhook);
    CreateLink(script, FindEndOfChain(script, slhook), CreateEventHook<&Receiver::OnPostSubLife>(script, m_Receiver));
    InsertBB(script, FindNextLink(script, ep, "Show"), CreateEventHook<&Receiver::OnExtraPoint>(script, m_Receiver));

    CKBehavior *delay = FindFirstBB(script, "Delayer");
    m_Mod.m_OverclockLinks[2] = FindPreviousLink(script, delay);
    CKBehaviorLink *link = FindNextLink(script, delay);
    m_Mod.m_OverclockLinkIO[2][1] = link->GetOutBehaviorIO();

    for (int i = 0; i < 3; i++) {
        m_Mod.m_OverclockLinkIO[i][0] = m_Mod.m_OverclockLinks[i]->GetOutBehaviorIO();
        if (m_Mod.m_Overclock->GetBoolean())
            m_Mod.m_OverclockLinks[i]->SetOutBehaviorIO(m_Mod.m_OverclockLinkIO[i][1]);
    }
}

void EventHookRegistrar::RegisterGameplayEvents(CKBehavior *script) {
    m_Mod.GetLogger()->Info("Insert Checkpoint & GameOver Hooks");
    CKMessageManager *mm = m_BML.GetMessageManager();
    CKMessageType checkpoint = mm->AddMessageType("Checkpoint reached"),
        gameover = mm->AddMessageType("Game Over"),
        levelfinish = mm->AddMessageType("Level_Finish");
    CKBehavior *cp, *go, *lf;
    FindBB(script, [checkpoint, gameover, levelfinish, &cp, &go, &lf](CKBehavior *beh) {
        auto msg = GetParamValue<CKMessageType>(beh->GetInputParameter(0)->GetDirectSource());
        if (msg == checkpoint) cp = beh;
        if (msg == gameover) go = beh;
        if (msg == levelfinish) lf = beh;
        return true;
    }, "Wait Message");
    CKBehavior *hook = CreateEventHook<&Receiver::OnPreCheckpointReached>(script, m_Receiver);
    InsertBB(script, FindNextLink(script, cp, "set Resetpoint"), hook);
    CreateLink(script, FindEndOfChain(script, hook), CreateEventHook<&Receiver::OnPostCheckpointReached>(script, m_Receiver));
    InsertBB(script, FindNextLink(script, go, "Send Message"), CreateEventHook<&Receiver::OnGameOver>(script, m_Receiver));
    InsertBB(script, FindNextLink(script, lf, "Send Message"), CreateEventHook<&Receiver::OnLevelFinish>(script, m_Receiver));
}
