/**
 * @file ScriptHelper.h
 * @brief Header-only script manipulation utilities for BML_Event module
 */

#ifndef BML_EVENT_SCRIPTHELPER_H
#define BML_EVENT_SCRIPTHELPER_H

#include <functional>

#include "CKAll.h"

namespace BML_Event {
    namespace ScriptHelper {
        //-----------------------------------------------------------------------------
        // Find Building Blocks
        //-----------------------------------------------------------------------------

        inline bool FindBB(CKBehavior *script, std::function<bool(CKBehavior *)> callback, const char *name = nullptr,
                           bool hierarchically = false,
                           int inputCnt = -1, int outputCnt = -1, int inputParamCnt = -1, int outputParamCnt = -1) {
            int cnt = script->GetSubBehaviorCount();
            for (int i = 0; i < cnt; i++) {
                CKBehavior *beh = script->GetSubBehavior(i);
                if (hierarchically && beh->GetSubBehaviorCount() > 0) {
                    if (!FindBB(beh, callback, name, hierarchically, inputCnt, outputCnt, inputParamCnt,
                                outputParamCnt))
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

        inline CKBehavior *FindFirstBB(CKBehavior *script, const char *name = nullptr, bool hierarchically = false,
                                       int inputCnt = -1, int outputCnt = -1, int inputParamCnt = -1,
                                       int outputParamCnt = -1) {
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

        //-----------------------------------------------------------------------------
        // Create and Manage Links
        //-----------------------------------------------------------------------------

        inline CKBehaviorLink *CreateLink(CKBehavior *script, CKBehaviorIO *in, CKBehaviorIO *out, int delay = 0) {
            CKBehaviorLink *link = (CKBehaviorLink *) script->GetCKContext()->CreateObject(CKCID_BEHAVIORLINK);
            link->SetInitialActivationDelay(delay);
            link->ResetActivationDelay();
            link->SetInBehaviorIO(in);
            link->SetOutBehaviorIO(out);
            script->AddSubBehaviorLink(link);
            return link;
        }

        inline CKBehaviorLink *CreateLink(CKBehavior *script, CKBehavior *inBeh, CKBehavior *outBeh,
                                          int inPos = 0, int outPos = 0, int delay = 0) {
            return CreateLink(script, inBeh->GetOutput(inPos), outBeh->GetInput(outPos), delay);
        }

        inline CKBehaviorLink *CreateLink(CKBehavior *script, CKBehavior *inBeh, CKBehaviorIO *out,
                                          int inPos = 0, int delay = 0) {
            return CreateLink(script, inBeh->GetOutput(inPos), out, delay);
        }

        inline CKBehaviorLink *CreateLink(CKBehavior *script, CKBehaviorIO *in, CKBehavior *outBeh,
                                          int outPos = 0, int delay = 0) {
            return CreateLink(script, in, outBeh->GetInput(outPos), delay);
        }

        //-----------------------------------------------------------------------------
        // Create Building Blocks
        //-----------------------------------------------------------------------------

        inline CKBehavior *CreateBB(CKBehavior *script, CKGUID guid, bool target = false) {
            CKBehavior *beh = (CKBehavior *) script->GetCKContext()->CreateObject(CKCID_BEHAVIOR);
            beh->InitFromGuid(guid);
            if (target)
                beh->UseTarget();
            script->AddSubBehavior(beh);
            return beh;
        }

        inline void InsertBB(CKBehavior *script, CKBehaviorLink *link, CKBehavior *beh, int inPos = 0, int outPos = 0) {
            CreateLink(script, beh, link->GetOutBehaviorIO(), outPos);
            link->SetOutBehaviorIO(beh->GetInput(inPos));
        }

        //-----------------------------------------------------------------------------
        // Parameter Helpers
        //-----------------------------------------------------------------------------

        inline CKParameterLocal *CreateLocalParameter(CKBehavior *script, const char *name, CKGUID type) {
            return script->CreateLocalParameter((CKSTRING) name, type);
        }

        template <typename T>
        inline CKParameterLocal *CreateParamValue(CKBehavior *script, const char *name, CKGUID guid, T value) {
            CKParameterLocal *param = CreateLocalParameter(script, name, guid);
            param->SetValue(&value, sizeof(T));
            return param;
        }

        inline CKParameterLocal *CreateParamObject(CKBehavior *script, const char *name, CKGUID guid, CKObject *value) {
            return CreateParamValue<CK_ID>(script, name, guid, CKOBJID(value));
        }

        inline CKParameterLocal *CreateParamString(CKBehavior *script, const char *name, const char *value) {
            CKParameterLocal *param = CreateLocalParameter(script, name, CKPGUID_STRING);
            param->SetStringValue((CKSTRING) value);
            return param;
        }

        template <typename T>
        inline void SetParamValue(CKParameter *param, T value) {
            param->SetValue(&value, sizeof(T));
        }

        inline void SetParamObject(CKParameter *param, CKObject *value) {
            CK_ID obj = CKOBJID(value);
            SetParamValue(param, obj);
        }

        inline void SetParamString(CKParameter *param, const char *value) {
            param->SetStringValue((CKSTRING) value);
        }

        template <typename T>
        inline T GetParamValue(CKParameter *param) {
            T res;
            param->GetValue(&res);
            return res;
        }

        inline CKObject *GetParamObject(CKParameter *param) {
            return param->GetValueObject();
        }

        inline const char *GetParamString(CKParameter *param) {
            return static_cast<const char *>(param->GetReadDataPtr());
        }

        //-----------------------------------------------------------------------------
        // Navigation Helpers
        //-----------------------------------------------------------------------------

        inline CKBehaviorLink *FindNextLink(CKBehavior *script, CKBehavior *beh, const char *name = nullptr,
                                            int inPos = -1, int outPos = -1, int inputCnt = -1, int outputCnt = -1,
                                            int inputParamCnt = -1, int outputParamCnt = -1) {
            int linkCnt = script->GetSubBehaviorLinkCount();
            for (int i = 0; i < linkCnt; i++) {
                CKBehaviorLink *link = script->GetSubBehaviorLink(i);
                CKBehaviorIO *in = link->GetInBehaviorIO();

                if (in->GetOwner() == beh && (inPos < 0 || beh->GetOutput(inPos) == in)) {
                    CKBehaviorIO *out = link->GetOutBehaviorIO();
                    CKBehavior *outBeh = out->GetOwner();

                    if ((!name || !strcmp(outBeh->GetName(), name)) && (outPos < 0 || outBeh->GetInput(outPos) == out)
                        &&
                        (inputCnt < 0 || outBeh->GetInputCount() == inputCnt) &&
                        (outputCnt < 0 || outBeh->GetOutputCount() == outputCnt) &&
                        (inputParamCnt < 0 || outBeh->GetInputParameterCount() == inputParamCnt) &&
                        (outputParamCnt < 0 || outBeh->GetOutputParameterCount() == outputParamCnt))
                        return link;
                }
            }

            return nullptr;
        }

        inline CKBehaviorLink *FindPreviousLink(CKBehavior *script, CKBehavior *beh, const char *name = nullptr,
                                                int inPos = -1, int outPos = -1, int inputCnt = -1, int outputCnt = -1,
                                                int inputParamCnt = -1, int outputParamCnt = -1) {
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

        inline CKBehavior *FindNextBB(CKBehavior *script, CKBehavior *beh, const char *name = nullptr,
                                      int inPos = -1, int outPos = -1, int inputCnt = -1, int outputCnt = -1,
                                      int inputParamCnt = -1, int outputParamCnt = -1) {
            CKBehaviorLink *link = FindNextLink(script, beh, name, inPos, outPos, inputCnt, outputCnt, inputParamCnt,
                                                outputParamCnt);
            return link ? link->GetOutBehaviorIO()->GetOwner() : nullptr;
        }

        inline CKBehavior *FindPreviousBB(CKBehavior *script, CKBehavior *beh, const char *name = nullptr,
                                          int inPos = -1, int outPos = -1, int inputCnt = -1, int outputCnt = -1,
                                          int inputParamCnt = -1, int outputParamCnt = -1) {
            CKBehaviorLink *link = FindPreviousLink(script, beh, name, inPos, outPos, inputCnt, outputCnt,
                                                    inputParamCnt,
                                                    outputParamCnt);
            return link ? link->GetInBehaviorIO()->GetOwner() : nullptr;
        }

        inline CKBehaviorLink *FindNextLink(CKBehavior *script, CKBehaviorIO *io, const char *name = nullptr,
                                            int outPos = -1, int inputCnt = -1, int outputCnt = -1,
                                            int inputParamCnt = -1, int outputParamCnt = -1) {
            int linkCnt = script->GetSubBehaviorLinkCount();
            for (int i = 0; i < linkCnt; i++) {
                CKBehaviorLink *link = script->GetSubBehaviorLink(i);
                CKBehaviorIO *in = link->GetInBehaviorIO();

                if (in == io) {
                    CKBehaviorIO *out = link->GetOutBehaviorIO();
                    CKBehavior *outBeh = out->GetOwner();

                    if ((!name || !strcmp(outBeh->GetName(), name)) && (outPos < 0 || outBeh->GetInput(outPos) == out)
                        &&
                        (inputCnt < 0 || outBeh->GetInputCount() == inputCnt) &&
                        (outputCnt < 0 || outBeh->GetOutputCount() == outputCnt) &&
                        (inputParamCnt < 0 || outBeh->GetInputParameterCount() == inputParamCnt) &&
                        (outputParamCnt < 0 || outBeh->GetOutputParameterCount() == outputParamCnt))
                        return link;
                }
            }

            return nullptr;
        }

        inline CKBehaviorLink *FindPreviousLink(CKBehavior *script, CKBehaviorIO *io, const char *name = nullptr,
                                                int inPos = -1, int inputCnt = -1, int outputCnt = -1,
                                                int inputParamCnt = -1, int outputParamCnt = -1) {
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

        inline CKBehavior *FindNextBB(CKBehavior *script, CKBehaviorIO *io, const char *name = nullptr, int outPos = -1,
                                      int inputCnt = -1, int outputCnt = -1, int inputParamCnt = -1,
                                      int outputParamCnt = -1) {
            CKBehaviorLink *link = FindNextLink(script, io, name, outPos, inputCnt, outputCnt, inputParamCnt,
                                                outputParamCnt);
            return link ? link->GetOutBehaviorIO()->GetOwner() : nullptr;
        }

        inline CKBehavior *FindPreviousBB(CKBehavior *script, CKBehaviorIO *io, const char *name = nullptr,
                                          int inPos = -1, int inputCnt = -1, int outputCnt = -1, int inputParamCnt = -1,
                                          int outputParamCnt = -1) {
            CKBehaviorLink *link = FindPreviousLink(script, io, name, inPos, inputCnt, outputCnt, inputParamCnt,
                                                    outputParamCnt);
            return link ? link->GetInBehaviorIO()->GetOwner() : nullptr;
        }

        inline CKBehavior *FindEndOfChain(CKBehavior *script, CKBehavior *beh) {
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

        //-----------------------------------------------------------------------------
        // Delete Helpers
        //-----------------------------------------------------------------------------

        inline void DeleteLink(CKBehavior *script, CKBehaviorLink *link) {
            static CKBehavior *nop = nullptr;
            static CKBehaviorIO *nopin = nullptr, *nopout = nullptr;
            if (!nop) {
                nop = (CKBehavior *) script->GetCKContext()->CreateObject(CKCID_BEHAVIOR);
                nop->InitFromGuid(CKGUID(0x302561c4, 0xd282980));
                nopin = nop->GetInput(0);
                nopout = nop->GetOutput(0);
            }

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
    } // namespace ScriptHelper
}     // namespace BML_Event

#endif // BML_EVENT_SCRIPTHELPER_H
