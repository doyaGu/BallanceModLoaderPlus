#ifndef BML_INPUT_CONTROL_H
#define BML_INPUT_CONTROL_H

#include "bml_errors.h"
#include "bml_interface.h"
#include "bml_types.h"

BML_BEGIN_CDECLS

BML_DECLARE_HANDLE(BML_InputCaptureToken);

#define BML_INPUT_CAPTURE_INTERFACE_ID "bml.input.capture"

typedef enum BML_InputCaptureFlags {
    BML_INPUT_CAPTURE_FLAG_NONE = 0,
    BML_INPUT_CAPTURE_FLAG_BLOCK_KEYBOARD = 1 << 0,
    BML_INPUT_CAPTURE_FLAG_BLOCK_MOUSE = 1 << 1
} BML_InputCaptureFlags;

typedef struct BML_InputCaptureDesc {
    size_t struct_size;
    uint32_t flags;
    int cursor_visible;
    int32_t priority;
} BML_InputCaptureDesc;

#define BML_INPUT_CAPTURE_DESC_INIT { sizeof(BML_InputCaptureDesc), 0u, -1, 0 }

typedef struct BML_InputCaptureState {
    size_t struct_size;
    uint32_t effective_flags;
    int cursor_visible;
} BML_InputCaptureState;

#define BML_INPUT_CAPTURE_STATE_INIT { sizeof(BML_InputCaptureState), 0u, 0 }

typedef struct BML_InputCaptureInterface {
    BML_InterfaceHeader header;
    BML_Result (*AcquireCapture)(const BML_InputCaptureDesc *desc, BML_InputCaptureToken *out_token);
    BML_Result (*UpdateCapture)(BML_InputCaptureToken token, const BML_InputCaptureDesc *desc);
    BML_Result (*ReleaseCapture)(BML_InputCaptureToken token);
    BML_Result (*QueryEffectiveState)(BML_InputCaptureState *out_state);
} BML_InputCaptureInterface;

BML_END_CDECLS

#endif /* BML_INPUT_CONTROL_H */
