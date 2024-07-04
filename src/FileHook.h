#ifndef BML_FILEHOOK_H
#define BML_FILEHOOK_H

#include "CKFile.h"

#include "Macros.h"

CP_HOOK_CLASS(CKFile) {
public:
    CP_DECLARE_METHOD_HOOK(CKERROR, OpenFile, (CKSTRING filename, CK_LOAD_FLAGS flags));

    CP_DECLARE_METHOD_HOOK(CKERROR, OpenMemory, (void *MemoryBuffer, int BufferSize, CK_LOAD_FLAGS flags));

    CP_DECLARE_METHOD_HOOK(CKERROR, LoadFileData, (CKObjectArray *list));

    CP_DECLARE_METHOD_HOOK(CKERROR, LoadFile, (CKSTRING filename, CKObjectArray *list, CK_LOAD_FLAGS flags));
    CP_DECLARE_METHOD_HOOK(CKERROR, LoadMemory, (void *MemoryBuffer, int BufferSize, CKObjectArray *list, CK_LOAD_FLAGS flags));

    CP_DECLARE_METHOD_HOOK(CKERROR, StartSave, (CKSTRING filename, CKDWORD flags));
    CP_DECLARE_METHOD_HOOK(void, SaveObject, (CKObject *obj, CKDWORD flags));
    CP_DECLARE_METHOD_HOOK(void, SaveObjects, (CKObjectArray *array, CKDWORD flags));
    CP_DECLARE_METHOD_HOOK(void, SaveObjects2, (CK_ID *ids, int count, CKDWORD flags));
    CP_DECLARE_METHOD_HOOK(void, SaveObjects3, (CKObject **objs, int count, CKDWORD flags));
    CP_DECLARE_METHOD_HOOK(void, SaveObjectAsReference, (CKObject *obj));
    CP_DECLARE_METHOD_HOOK(CKERROR, EndSave, ());

    CP_DECLARE_METHOD_HOOK(CKBOOL, IncludeFile, (CKSTRING FileName, int SearchPathCategory));

    CP_DECLARE_METHOD_HOOK(CKBOOL, IsObjectToBeSaved, (CK_ID iID));

    CP_DECLARE_METHOD_HOOK(void, LoadAndSave, (CKSTRING filename, CKSTRING filename_new));
    CP_DECLARE_METHOD_HOOK(void, RemapManagerInt, (CKGUID Manager, int *ConversionTable, int TableSize));

    CP_DECLARE_METHOD_HOOK(void, ClearData, ());

    CP_DECLARE_METHOD_HOOK(CKERROR, ReadFileHeaders, (CKBufferParser **ParserPtr));
    CP_DECLARE_METHOD_HOOK(CKERROR, ReadFileData, (CKBufferParser **ParserPtr));
    CP_DECLARE_METHOD_HOOK(void, FinishLoading, (CKObjectArray *list, CKDWORD flags));

    CP_DECLARE_METHOD_HOOK(CKObject *, ResolveReference, (CKFileObject *Data));

    CP_DECLARE_METHOD_PTRS(CKFile, CKERROR, OpenFile, (CKSTRING filename, CK_LOAD_FLAGS flags));

    CP_DECLARE_METHOD_PTRS(CKFile, CKERROR, OpenMemory, (void *MemoryBuffer, int BufferSize, CK_LOAD_FLAGS flags));

    CP_DECLARE_METHOD_PTRS(CKFile, CKERROR, LoadFileData, (CKObjectArray *list));

    CP_DECLARE_METHOD_PTRS(CKFile, CKERROR, LoadFile, (CKSTRING filename, CKObjectArray *list, CK_LOAD_FLAGS flags));
    CP_DECLARE_METHOD_PTRS(CKFile, CKERROR, LoadMemory, (void *MemoryBuffer, int BufferSize, CKObjectArray *list, CK_LOAD_FLAGS flags));

    CP_DECLARE_METHOD_PTRS(CKFile, CKERROR, StartSave, (CKSTRING filename, CKDWORD flags));
    CP_DECLARE_METHOD_PTRS(CKFile, void, SaveObject, (CKObject *obj, CKDWORD flags));
    CP_DECLARE_METHOD_PTRS(CKFile, void, SaveObjects, (CKObjectArray *array, CKDWORD flags));
    CP_DECLARE_METHOD_PTRS(CKFile, void, SaveObjects2, (CK_ID *ids, int count, CKDWORD flags));
    CP_DECLARE_METHOD_PTRS(CKFile, void, SaveObjects3, (CKObject **objs, int count, CKDWORD flags));
    CP_DECLARE_METHOD_PTRS(CKFile, void, SaveObjectAsReference, (CKObject *obj));
    CP_DECLARE_METHOD_PTRS(CKFile, CKERROR, EndSave, ());

    CP_DECLARE_METHOD_PTRS(CKFile, CKBOOL, IncludeFile, (CKSTRING FileName, int SearchPathCategory));

    CP_DECLARE_METHOD_PTRS(CKFile, CKBOOL, IsObjectToBeSaved, (CK_ID iID));

    CP_DECLARE_METHOD_PTRS(CKFile, void, LoadAndSave, (CKSTRING filename, CKSTRING filename_new));
    CP_DECLARE_METHOD_PTRS(CKFile, void, RemapManagerInt, (CKGUID Manager, int *ConversionTable, int TableSize));

    CP_DECLARE_METHOD_PTRS(CKFile, void, ClearData, ());

    CP_DECLARE_METHOD_PTRS(CKFile, CKERROR, ReadFileHeaders, (CKBufferParser **ParserPtr));
    CP_DECLARE_METHOD_PTRS(CKFile, CKERROR, ReadFileData, (CKBufferParser **ParserPtr));
    CP_DECLARE_METHOD_PTRS(CKFile, void, FinishLoading, (CKObjectArray *list, CKDWORD flags));

    CP_DECLARE_METHOD_PTRS(CKFile, CKObject *, ResolveReference, (CKFileObject *Data));

    static bool InitHooks();
    static void ShutdownHooks();
};

#endif // BML_FILEHOOK_H
