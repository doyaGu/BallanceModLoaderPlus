#include "Overclock.h"

using namespace ScriptHelper;

IMod *BMLEntry(IBML *bml) {
    return new Overclock(bml);
}

void BMLExit(IMod *mod) {
    delete mod;
}

void Overclock::OnLoad() {
    GetConfig()->SetCategoryComment("Misc", "Miscellaneous");

    m_Overclock = GetConfig()->GetProperty("Misc", "Overclock");
    m_Overclock->SetComment("Remove delay of spawn / respawn");
    m_Overclock->SetDefaultBoolean(false);
}

void Overclock::OnModifyConfig(const char *category, const char *key, IProperty *prop) {
    if (prop == m_Overclock) {
        for (int i = 0; i < 3; i++) {
            m_OverclockLinks[i]->SetOutBehaviorIO(m_OverclockLinkIO[i][m_Overclock->GetBoolean()]);
        }
    }
}

void Overclock::OnLoadScript(const char *filename, CKBehavior *script) {
    if (!strcmp(script->GetName(), "Gameplay_Ingame"))
        OnEditScript_Gameplay_Ingame(script);

    if (!strcmp(script->GetName(), "Gameplay_Energy"))
        OnEditScript_Gameplay_Energy(script);
}

void Overclock::OnEditScript_Gameplay_Ingame(CKBehavior *script) {
    CKBehavior *ballMgr = FindFirstBB(script, "BallManager");
    CKBehavior *deactBall = FindFirstBB(ballMgr, "Deactivate Ball");
    CKBehavior *pieces = FindFirstBB(deactBall, "reset Ballpieces");
    m_OverclockLinks[0] = FindNextLink(deactBall, pieces);
    CKBehavior *unphy = FindNextBB(deactBall, FindNextBB(deactBall, m_OverclockLinks[0]->GetOutBehaviorIO()->GetOwner()));
    m_OverclockLinkIO[0][1] = unphy->GetInput(1);

    CKBehavior *newBall = FindFirstBB(ballMgr, "New Ball");
    CKBehavior *physicsNewBall = FindFirstBB(newBall, "physicalize new Ball");
    m_OverclockLinks[1] = FindPreviousLink(newBall, FindPreviousBB(newBall, FindPreviousBB(newBall, FindPreviousBB(newBall, physicsNewBall))));
    m_OverclockLinkIO[1][1] = physicsNewBall->GetInput(0);
}

void Overclock::OnEditScript_Gameplay_Energy(CKBehavior *script) {
    CKBehavior *delay = FindFirstBB(script, "Delayer");
    m_OverclockLinks[2] = FindPreviousLink(script, delay);
    CKBehaviorLink *link = FindNextLink(script, delay);
    m_OverclockLinkIO[2][1] = link->GetOutBehaviorIO();

    for (int i = 0; i < 3; i++) {
        m_OverclockLinkIO[i][0] = m_OverclockLinks[i]->GetOutBehaviorIO();
        if (m_Overclock->GetBoolean())
            m_OverclockLinks[i]->SetOutBehaviorIO(m_OverclockLinkIO[i][1]);
    }
}
