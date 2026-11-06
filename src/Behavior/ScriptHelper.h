// Reading and rewriting the game's behaviour graphs from C++. Most of what Ballance does
// is a script rather than code, so a Mod that wants to change the game usually has to
// find a block inside a script, read what it was given, put a block of its own between
// two others, or take a link out of the way. The CK SDK can do all of that, and this is
// the short way to write it.
//
// A Mod gets the script to work on from ModContext::GetScriptByName or from
// IMod::OnLoadScript, which is the moment the loader offers a script before the game has
// run it and the only safe place to rewrite one. The names to look for are the ones
// Virtools Dev shows, and the GUIDs for CreateBB are in BML/Guids.
//
// Finding. FindBB walks the sub-blocks of a script and calls back for each one matching
// the filters given; a filter left at its default matches anything, and the counts of
// inputs, outputs, and parameters are there to tell apart the several blocks a script has
// under one name. Returning false from the callback stops the walk, and FindBB then
// returns false as well, which is how FindFirstBB stops at the first hit. With
// hierarchically set the walk goes into a block's own sub-graph before looking at the
// block itself, so the first hit can be one nested deep rather than the outermost, and
// FindFirstBB is not the topmost match. FindNextBB, FindPreviousBB, and FindEndOfChain
// follow the links out of a block instead, and the FindLink pair answers with the link.
//
// Writing. CreateBB adds a block of the given GUID to the script, with target set when
// the block takes one, and CreateLink joins two of them; the overloads differ only in
// whether a block plus a pin number or a CKBehaviorIO is passed. InsertBB is the one to
// reach for when hooking: it puts a block into an existing link, so what ran before still
// runs and the new block runs in the middle.
//
// Parameters. CreateLocalParameter and the CreateParam family make a value for a block to
// read; a block's own input parameter is usually a shortcut to somewhere else, so to
// change what a block reads, write to GetInputParameter(i)->GetDirectSource() for a
// parameter this code created, or GetRealSource() to follow the shortcuts to where the
// value really lives. SetParamValue and GetParamValue copy sizeof(T) bytes in and out
// unchecked, so the T has to be the type the parameter holds, an int for an int
// parameter, and a wrong one reads or writes the wrong number of bytes. Objects go in and
// out as a CK_ID, which is what SetParamObject and GetParamObject are for, and
// GetParamString hands back the parameter's own buffer, good until the parameter changes.
//
// Removing, which is not a removal. DeleteLink points the link at a hidden do-nothing
// block instead of destroying it, and DeleteBB deactivates the block and does that to
// every link touching it. Nothing leaves the script either way: the sub-block count is
// unchanged and FindBB still finds what was deleted. So a Mod that has to undo a hook
// puts the links back the way they were rather than expecting these to have removed
// anything, and DeleteLink also creates that hidden block once and keeps it for the rest
// of the process.
//
// All of it runs on the game thread and none of it is safe once the level holding the
// script is gone, since the blocks, links, and parameters are objects of that level.
#ifndef BML_PRIVATE_SCRIPTHELPER_H
#define BML_PRIVATE_SCRIPTHELPER_H

#include <functional>

#include "CKAll.h"

#include "BML/Defines.h"

namespace ScriptHelper {
bool FindBB(CKBehavior *script, std::function<bool(CKBehavior *)> callback,
                       const char *name = nullptr, bool hierarchically = false, int inputCnt = -1,
                       int outputCnt = -1, int inputParamCnt = -1, int outputParamCnt = -1);
CKBehavior *FindFirstBB(CKBehavior *script, const char *name = nullptr,
                                   bool hierarchically = false, int inputCnt = -1,
                                   int outputCnt = -1, int inputParamCnt = -1,
                                   int outputParamCnt = -1);

CKBehaviorLink *CreateLink(CKBehavior *script, CKBehavior *inBeh, CKBehavior *outBeh,
                                      int inPos = 0, int outPos = 0, int delay = 0);
CKBehaviorLink *CreateLink(CKBehavior *script, CKBehavior *inBeh, CKBehaviorIO *out,
                                      int inPos = 0, int delay = 0);
CKBehaviorLink *CreateLink(CKBehavior *script, CKBehaviorIO *in, CKBehavior *outBeh,
                                      int outPos = 0, int delay = 0);
CKBehaviorLink *CreateLink(CKBehavior *script, CKBehaviorIO *in, CKBehaviorIO *out,
                                      int delay = 0);

CKBehavior *CreateBB(CKBehavior *script, CKGUID guid, bool target = false);
void InsertBB(CKBehavior *script, CKBehaviorLink *link, CKBehavior *beh, int inPos = 0,
                         int outPos = 0);

CKParameterLocal *CreateLocalParameter(CKBehavior *script, const char *name,
                                                  CKGUID type);
CKParameterLocal *CreateParamObject(CKBehavior *script, const char *name, CKGUID guid,
                                               CKObject *value);
CKParameterLocal *CreateParamString(CKBehavior *script, const char *name,
                                               const char *value);
template <typename T>
CKParameterLocal *CreateParamValue(CKBehavior *script, const char *name, CKGUID guid, T value) {
    CKParameterLocal *param = CreateLocalParameter(script, name, guid);
    param->SetValue(&value, sizeof(T));
    return param;
}

void SetParamObject(CKParameter *param, CKObject *value);
void SetParamString(CKParameter *param, const char *value);
template <typename T> void SetParamValue(CKParameter *param, T value) {
    param->SetValue(&value, sizeof(T));
}

CKObject *GetParamObject(CKParameter *param);
const char *GetParamString(CKParameter *param);
template <typename T> T GetParamValue(CKParameter *param) {
    T res;
    param->GetValue(&res);
    return res;
}

CKBehaviorLink *FindNextLink(CKBehavior *script, CKBehavior *beh,
                                        const char *name = nullptr, int inPos = -1, int outPos = -1,
                                        int inputCnt = -1, int outputCnt = -1,
                                        int inputParamCnt = -1, int outputParamCnt = -1);
CKBehaviorLink *FindPreviousLink(CKBehavior *script, CKBehavior *beh,
                                            const char *name = nullptr, int inPos = -1,
                                            int outPos = -1, int inputCnt = -1, int outputCnt = -1,
                                            int inputParamCnt = -1, int outputParamCnt = -1);
CKBehavior *FindNextBB(CKBehavior *script, CKBehavior *beh, const char *name = nullptr,
                                  int inPos = -1, int outPos = -1, int inputCnt = -1,
                                  int outputCnt = -1, int inputParamCnt = -1,
                                  int outputParamCnt = -1);
CKBehavior *FindPreviousBB(CKBehavior *script, CKBehavior *beh,
                                      const char *name = nullptr, int inPos = -1, int outPos = -1,
                                      int inputCnt = -1, int outputCnt = -1, int inputParamCnt = -1,
                                      int outputParamCnt = -1);
CKBehaviorLink *FindNextLink(CKBehavior *script, CKBehaviorIO *io,
                                        const char *name = nullptr, int outPos = -1,
                                        int inputCnt = -1, int outputCnt = -1,
                                        int inputParamCnt = -1, int outputParamCnt = -1);
CKBehaviorLink *FindPreviousLink(CKBehavior *script, CKBehaviorIO *io,
                                            const char *name = nullptr, int inPos = -1,
                                            int inputCnt = -1, int outputCnt = -1,
                                            int inputParamCnt = -1, int outputParamCnt = -1);
CKBehavior *FindNextBB(CKBehavior *script, CKBehaviorIO *io, const char *name = nullptr,
                                  int outPos = -1, int inputCnt = -1, int outputCnt = -1,
                                  int inputParamCnt = -1, int outputParamCnt = -1);
CKBehavior *FindPreviousBB(CKBehavior *script, CKBehaviorIO *io,
                                      const char *name = nullptr, int inPos = -1, int inputCnt = -1,
                                      int outputCnt = -1, int inputParamCnt = -1,
                                      int outputParamCnt = -1);
CKBehavior *FindEndOfChain(CKBehavior *script, CKBehavior *beh);

void DeleteLink(CKBehavior *script, CKBehaviorLink *link);
void DeleteBB(CKBehavior *script, CKBehavior *beh);
}; // namespace ScriptHelper

#endif // BML_PRIVATE_SCRIPTHELPER_H