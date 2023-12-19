#pragma once

#include <BML/BMLAll.h>

#include <map>

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class MapScripts : public IMod {
public:
	explicit MapScripts(IBML* bml) : IMod(bml) {}

	const char *GetID() override { return "MapScripts"; }
	const char *GetVersion() override { return BML_VERSION; }
	const char *GetName() override { return "Map Scripts"; }
	const char *GetAuthor() override { return "Gamepiaynmo"; }
	const char *GetDescription() override { return "Activate callback scripts in maps."; }
	DECLARE_BML_VERSION;

	void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                      CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                      XObjectArray *objArray, CKObject *masterObj) override;

	void OnPostLoadLevel() override;
	void OnStartLevel() override;
	void OnPreResetLevel() override;
	void OnPostResetLevel() override;
	void OnPauseLevel() override;
	void OnUnpauseLevel() override;
	void OnPreExitLevel() override;
	void OnPreNextLevel() override;
	void OnDead() override;
	void OnPreEndLevel() override;
	void OnPostEndLevel() override;

	void OnCounterActive() override;
	void OnCounterInactive() override;
	void OnBallNavActive() override;
	void OnBallNavInactive() override;
	void OnCamNavActive() override;
	void OnCamNavInactive() override;
	void OnBallOff() override;
	void OnPreCheckpointReached() override;
	void OnPostCheckpointReached() override;
	void OnLevelFinish() override;
	void OnGameOver() override;
	void OnExtraPoint() override;
	void OnPreSubLife() override;
	void OnPostSubLife() override;
	void OnPreLifeUp() override;
	void OnPostLifeUp() override;

private:
	std::map<void*, CKBehavior*> m_MapScripts;
};