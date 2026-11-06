// Reading and rewriting the game's behaviour graphs from C++. Most of what Ballance does
// is a script rather than code, so a Mod that wants to change the game usually has to
// find a block inside a script, read what it was given, put a block of its own between
// two others, or take a link out of the way. The CK SDK can do all of that, and this is
// the short way to write it.
//
// A Mod gets the script to work on from IBML::GetScriptByName or from
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
#ifndef BML_SCRIPTHELPER_H
#define BML_SCRIPTHELPER_H

#include <functional>

#include "BML/Engine.hpp"

#include "BML/Defines.h"

namespace ScriptHelper {
    inline bool FindBB(CKBehavior *script, std::function<bool(CKBehavior *)> callback, const char *name = nullptr,
                           bool hierarchically = false,
                           int inputCnt = -1, int outputCnt = -1, int inputParamCnt = -1, int outputParamCnt = -1);
    inline CKBehavior *FindFirstBB(CKBehavior *script, const char *name = nullptr, bool hierarchically = false,
                                       int inputCnt = -1, int outputCnt = -1, int inputParamCnt = -1,
                                       int outputParamCnt = -1);

    inline CKBehaviorLink *CreateLink(CKBehavior *script, CKBehavior *inBeh, CKBehavior *outBeh,
                                          int inPos = 0, int outPos = 0, int delay = 0);
    inline CKBehaviorLink *CreateLink(CKBehavior *script, CKBehavior *inBeh, CKBehaviorIO *out,
                                          int inPos = 0, int delay = 0);
    inline CKBehaviorLink *CreateLink(CKBehavior *script, CKBehaviorIO *in, CKBehavior *outBeh,
                                          int outPos = 0, int delay = 0);
    inline CKBehaviorLink *CreateLink(CKBehavior *script, CKBehaviorIO *in, CKBehaviorIO *out, int delay = 0);

    inline CKBehavior *CreateBB(CKBehavior *script, CKGUID guid, bool target = false);
    inline void InsertBB(CKBehavior *script, CKBehaviorLink *link, CKBehavior *beh, int inPos = 0, int outPos = 0);

    inline CKParameterLocal *CreateLocalParameter(CKBehavior *script, const char *name, CKGUID type);
    inline CKParameterLocal *CreateParamObject(CKBehavior *script, const char *name, CKGUID guid, CKObject *value);
    inline CKParameterLocal *CreateParamString(CKBehavior *script, const char *name, const char *value);
    template<typename T>
    CKParameterLocal *CreateParamValue(CKBehavior *script, const char *name, CKGUID guid, T value) {
        CKParameterLocal *param = CreateLocalParameter(script, name, guid);
        param->SetValue(&value, sizeof(T));
        return param;
    }

    inline void SetParamObject(CKParameter *param, CKObject *value);
    inline void SetParamString(CKParameter *param, const char *value);
    template<typename T>
    void SetParamValue(CKParameter *param, T value) {
        param->SetValue(&value, sizeof(T));
    }

    inline CKObject *GetParamObject(CKParameter *param);
    inline const char *GetParamString(CKParameter *param);
    template<typename T>
    T GetParamValue(CKParameter *param) {
        T res;
        param->GetValue(&res);
        return res;
    }

    inline CKBehaviorLink *FindNextLink(CKBehavior *script, CKBehavior *beh, const char *name = nullptr,
                                            int inPos = -1, int outPos = -1, int inputCnt = -1, int outputCnt = -1,
                                            int inputParamCnt = -1, int outputParamCnt = -1);
    inline CKBehaviorLink *FindPreviousLink(CKBehavior *script, CKBehavior *beh, const char *name = nullptr,
                                                int inPos = -1, int outPos = -1,int inputCnt = -1, int outputCnt = -1,
                                                int inputParamCnt = -1, int outputParamCnt = -1);
    inline CKBehavior *FindNextBB(CKBehavior *script, CKBehavior *beh, const char *name = nullptr,
                                      int inPos = -1, int outPos = -1, int inputCnt = -1, int outputCnt = -1,
                                      int inputParamCnt = -1, int outputParamCnt = -1);
    inline CKBehavior *FindPreviousBB(CKBehavior *script, CKBehavior *beh, const char *name = nullptr,
                                          int inPos = -1, int outPos = -1, int inputCnt = -1, int outputCnt = -1,
                                          int inputParamCnt = -1, int outputParamCnt = -1);
    inline CKBehaviorLink *FindNextLink(CKBehavior *script, CKBehaviorIO *io, const char *name = nullptr,
                                            int outPos = -1, int inputCnt = -1, int outputCnt = -1,
                                            int inputParamCnt = -1, int outputParamCnt = -1);
    inline CKBehaviorLink *FindPreviousLink(CKBehavior *script, CKBehaviorIO *io, const char *name = nullptr,
                                                int inPos = -1, int inputCnt = -1, int outputCnt = -1,
                                                int inputParamCnt = -1, int outputParamCnt = -1);
    inline CKBehavior *FindNextBB(CKBehavior *script, CKBehaviorIO *io, const char *name = nullptr, int outPos = -1,
                                      int inputCnt = -1, int outputCnt = -1, int inputParamCnt = -1,
                                      int outputParamCnt = -1);
    inline CKBehavior *FindPreviousBB(CKBehavior *script, CKBehaviorIO *io, const char *name = nullptr,
                                          int inPos = -1, int inputCnt = -1, int outputCnt = -1, int inputParamCnt = -1,
                                          int outputParamCnt = -1);
    inline CKBehavior *FindEndOfChain(CKBehavior *script, CKBehavior *beh);

    inline void DeleteLink(CKBehavior *script, CKBehaviorLink *link);
    inline void DeleteBB(CKBehavior *script, CKBehavior *beh);

    inline bool FindBB(CKBehavior *script, std::function<bool(CKBehavior *)> callback, const char *name, bool hierarchically,
                int inputCnt, int outputCnt, int inputParamCnt, int outputParamCnt) {
        int cnt = script->GetSubBehaviorCount();
        for (int i = 0; i < cnt; i++) {
            CKBehavior *beh = script->GetSubBehavior(i);
            if (hierarchically && beh->GetSubBehaviorCount() > 0) {
                if (!FindBB(beh, callback, name, hierarchically, inputCnt, outputCnt, inputParamCnt, outputParamCnt))
                    return false;
            }

            if ((!name || !strcmp(beh->GetName(), name)) && (inputCnt < 0 || beh->GetInputCount() == inputCnt) &&
                (outputCnt < 0 || beh->GetOutputCount() == outputCnt) &&
                (inputParamCnt < 0 || beh->GetInputParameterCount() == inputParamCnt) &&
                (outputParamCnt < 0 || beh->GetOutputParameterCount() == outputParamCnt))
                if (!callback(beh))
                    return false;
        }

        return true;
    }

    inline CKBehavior * FindFirstBB(CKBehavior *script, const char *name, bool hierarchically,
                            int inputCnt, int outputCnt, int inputParamCnt, int outputParamCnt) {
        CKBehavior *res = nullptr;
        FindBB(
            script, [&res](CKBehavior *beh) {
                res = beh;
                return false;
            },
            name, hierarchically,
            inputCnt, outputCnt, inputParamCnt, outputParamCnt);
        return res;
    }

    inline CKBehaviorLink * CreateLink(CKBehavior *script, CKBehavior *inBeh, CKBehavior *outBeh,
                               int inPos, int outPos, int delay) {
        return CreateLink(script, inBeh->GetOutput(inPos), outBeh->GetInput(outPos), delay);
    }

    inline CKBehaviorLink * CreateLink(CKBehavior *script, CKBehavior *inBeh, CKBehaviorIO *out, int inPos, int delay) {
        return CreateLink(script, inBeh->GetOutput(inPos), out, delay);
    }

    inline CKBehaviorLink * CreateLink(CKBehavior *script, CKBehaviorIO *in, CKBehavior *outBeh, int outPos, int delay) {
        return CreateLink(script, in, outBeh->GetInput(outPos), delay);
    }

    inline CKBehaviorLink * CreateLink(CKBehavior *script, CKBehaviorIO *in, CKBehaviorIO *out, int delay) {
        CKBehaviorLink *link = (CKBehaviorLink *) script->GetCKContext()->CreateObject(CKCID_BEHAVIORLINK);
        link->SetInitialActivationDelay(delay);
        link->ResetActivationDelay();
        link->SetInBehaviorIO(in);
        link->SetOutBehaviorIO(out);
        script->AddSubBehaviorLink(link);
        return link;
    }

    inline CKBehavior * CreateBB(CKBehavior *script, CKGUID guid, bool target) {
        CKBehavior *beh = (CKBehavior *) script->GetCKContext()->CreateObject(CKCID_BEHAVIOR);
        beh->InitFromGuid(guid);
        if (target)
            beh->UseTarget();
        if (script->AddSubBehavior(beh) != CK_OK) {
            script->GetCKContext()->DestroyObject(beh);
            return nullptr;
        }
        return beh;
    }

    inline void InsertBB(CKBehavior *script, CKBehaviorLink *link, CKBehavior *beh, int inPos, int outPos) {
        CreateLink(script, beh, link->GetOutBehaviorIO(), outPos);
        link->SetOutBehaviorIO(beh->GetInput(inPos));
    }

    inline CKParameterLocal * CreateLocalParameter(CKBehavior *script, const char *name, CKGUID type) {
        return script->CreateLocalParameter((CKSTRING) name, type);
    }

    inline CKParameterLocal * CreateParamObject(CKBehavior *script, const char *name, CKGUID guid, CKObject *value) {
        return CreateParamValue<CK_ID>(script, name, guid, CKOBJID(value));
    }

    inline CKParameterLocal * CreateParamString(CKBehavior *script, const char *name, const char *value) {
        CKParameterLocal *param = CreateLocalParameter(script, name, CKPGUID_STRING);
        param->SetStringValue((CKSTRING) value);
        return param;
    }

    inline void SetParamObject(CKParameter *param, CKObject *value) {
        CK_ID obj = CKOBJID(value);
        SetParamValue(param, obj);
    }

    inline void SetParamString(CKParameter *param, const char *value) {
        param->SetStringValue((CKSTRING) value);
    }

    inline CKObject * GetParamObject(CKParameter *param) {
        return param->GetValueObject();
    }

    const char *GetParamString(CKParameter *param) {
        return static_cast<const char *>(param->GetReadDataPtr());
    }

    inline CKBehaviorLink * FindNextLink(CKBehavior *script, CKBehavior *beh, const char *name, int inPos, int outPos,
                                 int inputCnt, int outputCnt, int inputParamCnt, int outputParamCnt) {
        int linkCnt = script->GetSubBehaviorLinkCount();
        for (int i = 0; i < linkCnt; i++) {
            CKBehaviorLink *link = script->GetSubBehaviorLink(i);
            CKBehaviorIO *in = link->GetInBehaviorIO();

            if (in->GetOwner() == beh && (inPos < 0 || beh->GetOutput(inPos) == in)) {
                CKBehaviorIO *out = link->GetOutBehaviorIO();
                CKBehavior *outBeh = out->GetOwner();

                if ((!name || !strcmp(outBeh->GetName(), name)) && (outPos < 0 || outBeh->GetInput(outPos) == out) &&
                    (inputCnt < 0 || outBeh->GetInputCount() == inputCnt) &&
                    (outputCnt < 0 || outBeh->GetOutputCount() == outputCnt) &&
                    (inputParamCnt < 0 || outBeh->GetInputParameterCount() == inputParamCnt) &&
                    (outputParamCnt < 0 || outBeh->GetOutputParameterCount() == outputParamCnt))
                    return link;
            }
        }

        return nullptr;
    }

    inline CKBehaviorLink * FindPreviousLink(CKBehavior *script, CKBehavior *beh, const char *name, int inPos, int outPos,
                                     int inputCnt, int outputCnt, int inputParamCnt, int outputParamCnt) {
        int linkCnt = script->GetSubBehaviorLinkCount();
        for (int i = 0; i < linkCnt; i++) {
            CKBehaviorLink *link = script->GetSubBehaviorLink(i);
            CKBehaviorIO *out = link->GetOutBehaviorIO();

            if (out->GetOwner() == beh && (outPos < 0 || beh->GetInput(outPos) == out)) {
                CKBehaviorIO *in = link->GetInBehaviorIO();
                CKBehavior *inBeh = in->GetOwner();

                if ((!name || !strcmp(inBeh->GetName(), name)) && (inPos < 0 || inBeh->GetOutput(inPos) == in) &&
                    (inputCnt < 0 || inBeh->GetInputCount() == inputCnt) &&
                    (outputCnt < 0 || inBeh->GetOutputCount() == outputCnt) &&
                    (inputParamCnt < 0 || inBeh->GetInputParameterCount() == inputParamCnt) &&
                    (outputParamCnt < 0 || inBeh->GetOutputParameterCount() == outputParamCnt))
                    return link;
            }
        }

        return nullptr;
    }

    inline CKBehavior * FindNextBB(CKBehavior *script, CKBehavior *beh, const char *name, int inPos, int outPos,
                           int inputCnt, int outputCnt, int inputParamCnt, int outputParamCnt) {
        CKBehaviorLink *link = FindNextLink(script, beh, name, inPos, outPos, inputCnt, outputCnt, inputParamCnt,
                                            outputParamCnt);
        return link ? link->GetOutBehaviorIO()->GetOwner() : nullptr;
    }

    inline CKBehavior * FindPreviousBB(CKBehavior *script, CKBehavior *beh, const char *name, int inPos, int outPos,
                               int inputCnt, int outputCnt, int inputParamCnt, int outputParamCnt) {
        CKBehaviorLink *link = FindPreviousLink(script, beh, name, inPos, outPos, inputCnt, outputCnt, inputParamCnt,
                                                outputParamCnt);
        return link ? link->GetInBehaviorIO()->GetOwner() : nullptr;
    }

    inline CKBehaviorLink * FindNextLink(CKBehavior *script, CKBehaviorIO *io, const char *name, int outPos,
                                 int inputCnt, int outputCnt, int inputParamCnt, int outputParamCnt) {
        int linkCnt = script->GetSubBehaviorLinkCount();
        for (int i = 0; i < linkCnt; i++) {
            CKBehaviorLink *link = script->GetSubBehaviorLink(i);
            CKBehaviorIO *in = link->GetInBehaviorIO();

            if (in == io) {
                CKBehaviorIO *out = link->GetOutBehaviorIO();
                CKBehavior *outBeh = out->GetOwner();

                if ((!name || !strcmp(outBeh->GetName(), name)) && (outPos < 0 || outBeh->GetInput(outPos) == out) &&
                    (inputCnt < 0 || outBeh->GetInputCount() == inputCnt) &&
                    (outputCnt < 0 || outBeh->GetOutputCount() == outputCnt) &&
                    (inputParamCnt < 0 || outBeh->GetInputParameterCount() == inputParamCnt) &&
                    (outputParamCnt < 0 || outBeh->GetOutputParameterCount() == outputParamCnt))
                    return link;
            }
        }

        return nullptr;
    }

    inline CKBehaviorLink * FindPreviousLink(CKBehavior *script, CKBehaviorIO *io, const char *name, int inPos,
                                     int inputCnt, int outputCnt, int inputParamCnt, int outputParamCnt) {
        int linkCnt = script->GetSubBehaviorLinkCount();
        for (int i = 0; i < linkCnt; i++) {
            CKBehaviorLink *link = script->GetSubBehaviorLink(i);
            CKBehaviorIO *out = link->GetOutBehaviorIO();

            if (out == io) {
                CKBehaviorIO *in = link->GetInBehaviorIO();
                CKBehavior *inBeh = in->GetOwner();

                if ((!name || !strcmp(inBeh->GetName(), name)) && (inPos < 0 || inBeh->GetOutput(inPos) == in) &&
                    (inputCnt < 0 || inBeh->GetInputCount() == inputCnt) &&
                    (outputCnt < 0 || inBeh->GetOutputCount() == outputCnt) &&
                    (inputParamCnt < 0 || inBeh->GetInputParameterCount() == inputParamCnt) &&
                    (outputParamCnt < 0 || inBeh->GetOutputParameterCount() == outputParamCnt))
                    return link;
            }
        }

        return nullptr;
    }

    inline CKBehavior * FindNextBB(CKBehavior *script, CKBehaviorIO *io, const char *name, int outPos,
                           int inputCnt, int outputCnt, int inputParamCnt, int outputParamCnt) {
        CKBehaviorLink *link = FindNextLink(script, io, name, outPos, inputCnt, outputCnt, inputParamCnt,
                                            outputParamCnt);
        return link ? link->GetOutBehaviorIO()->GetOwner() : nullptr;
    }

    inline CKBehavior * FindPreviousBB(CKBehavior *script, CKBehaviorIO *io, const char *name, int inPos,
                               int inputCnt, int outputCnt, int inputParamCnt, int outputParamCnt) {
        CKBehaviorLink *link = FindPreviousLink(script, io, name, inPos, inputCnt, outputCnt, inputParamCnt,
                                                outputParamCnt);
        return link ? link->GetInBehaviorIO()->GetOwner() : nullptr;
    }

    inline CKBehavior * FindEndOfChain(CKBehavior *script, CKBehavior *beh) {
        CKBehavior *next;
        while (true) {
            next = FindNextBB(script, beh);
            if (next)
                beh = next;
            else
                break;
        };
        return beh;
    }

    inline void DeleteLink(CKBehavior *script, CKBehaviorLink *link) {
        CKBehavior *nop = FindFirstBB(script, "__BML_DisabledLink");
        if (!nop) {
            nop = CreateBB(script, CKGUID(0x302561c4, 0xd282980));
            if (!nop) return;
            nop->SetName((CKSTRING) "__BML_DisabledLink");
        }
        CKBehaviorIO *nopin = nop->GetInput(0), *nopout = nop->GetOutput(0);
        link->SetInBehaviorIO(nopout);
        link->SetOutBehaviorIO(nopin);
    }

    inline void DeleteBB(CKBehavior *script, CKBehavior *beh) {
        beh->Activate(false);
        int linkCnt = script->GetSubBehaviorLinkCount();
        for (int i = 0; i < linkCnt; i++) {
            CKBehaviorLink *link = script->GetSubBehaviorLink(i);
            if (link->GetInBehaviorIO()->GetOwner() == beh || link->GetOutBehaviorIO()->GetOwner() == beh)
                DeleteLink(script, link);
        }
    }
};

#endif // BML_SCRIPTHELPER_H