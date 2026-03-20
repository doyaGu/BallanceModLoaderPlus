#ifndef BML_NETWORK_H
#define BML_NETWORK_H

#include "bml_errors.h"
#include "bml_interface.h"
#include "bml_types.h"

BML_BEGIN_CDECLS

#define BML_NETWORK_INTERFACE_ID "bml.network"

/* ========================================================================
 * Common
 * ======================================================================== */

typedef uint32_t BML_HttpRequestId;
typedef uint32_t BML_SocketId;

/* ========================================================================
 * HTTP -- Flags, Request, Response, Callbacks
 * ======================================================================== */

#define BML_HTTP_FLAG_NONE              0u
#define BML_HTTP_FLAG_NO_REDIRECT       (1u << 0)
#define BML_HTTP_FLAG_INSECURE          (1u << 1)
#define BML_HTTP_FLAG_DOWNLOAD          (1u << 2)

typedef struct BML_HttpRequest {
    size_t struct_size;
    const char *url;
    const char *method;
    const char *body;
    size_t body_size;
    const char *const *headers;
    uint32_t timeout_ms;
    uint32_t flags;
    uint32_t max_retries;
    size_t max_body_size;
    const char *output_path;
} BML_HttpRequest;

#define BML_HTTP_REQUEST_INIT { sizeof(BML_HttpRequest), NULL, "GET", NULL, 0, NULL, 0, 0, 0, 0, NULL }

typedef struct BML_HttpResponse {
    size_t struct_size;
    int32_t status_code;
    const char *body;
    size_t body_size;
    const char *content_type;
    const char *const *headers;
    const char *error;
    BML_HttpRequestId request_id;
    const char *final_url;
} BML_HttpResponse;

#define BML_HTTP_RESPONSE_INIT { sizeof(BML_HttpResponse), 0, NULL, 0, NULL, NULL, NULL, 0, NULL }

typedef void (*BML_HttpCallback)(const BML_HttpResponse *response, void *user_data);
typedef void (*BML_HttpProgressCallback)(BML_HttpRequestId request_id,
                                          uint64_t downloaded, uint64_t total,
                                          void *user_data);

/* ========================================================================
 * Socket -- Types, Events, Callbacks
 * ======================================================================== */

typedef enum BML_SocketType {
    BML_SOCKET_TCP = 0,
    BML_SOCKET_UDP = 1,
    _BML_SOCKET_TYPE_FORCE_32BIT = 0x7FFFFFFF
} BML_SocketType;

typedef enum BML_SocketEvent {
    BML_SOCKET_EVENT_CONNECTED = 0,
    BML_SOCKET_EVENT_DATA      = 1,
    BML_SOCKET_EVENT_CLOSED    = 2,
    BML_SOCKET_EVENT_ERROR     = 3,
    _BML_SOCKET_EVENT_FORCE_32BIT = 0x7FFFFFFF
} BML_SocketEvent;

typedef struct BML_SocketEventData {
    size_t struct_size;
    BML_SocketId socket_id;
    BML_SocketEvent event;
    const char *data;
    size_t data_size;
    const char *error;
    const char *remote_addr;
    uint16_t remote_port;
} BML_SocketEventData;

#define BML_SOCKET_EVENT_DATA_INIT { sizeof(BML_SocketEventData), 0, BML_SOCKET_EVENT_ERROR, NULL, 0, NULL, NULL, 0 }

typedef void (*BML_SocketCallback)(const BML_SocketEventData *event, void *user_data);

/* ========================================================================
 * Sub-Interface: HTTP
 * ======================================================================== */

typedef struct BML_HttpSubInterface {
    BML_InterfaceHeader header;
    BML_Result (*RequestAsync)(const BML_HttpRequest *request,
                               BML_HttpCallback callback,
                               BML_HttpProgressCallback progress,
                               void *user_data,
                               BML_HttpRequestId *out_id);
    BML_Result (*CancelRequest)(BML_HttpRequestId id);
    uint32_t (*GetPendingCount)(void);
} BML_HttpSubInterface;

/* ========================================================================
 * Sub-Interface: Socket
 * ======================================================================== */

typedef struct BML_SocketSubInterface {
    BML_InterfaceHeader header;
    BML_Result (*Open)(BML_SocketType type,
                       const char *host,
                       uint16_t port,
                       BML_SocketCallback callback,
                       void *user_data,
                       BML_SocketId *out_id);
    BML_Result (*Send)(BML_SocketId id, const void *data, size_t size);
    BML_Result (*SendTo)(BML_SocketId id, const char *host, uint16_t port,
                         const void *data, size_t size);
    BML_Result (*Close)(BML_SocketId id);
    uint32_t (*GetOpenCount)(void);
} BML_SocketSubInterface;

/* ========================================================================
 * Root Network Interface
 * ======================================================================== */

/**
 * @brief Network interface published by BML_Network module.
 *
 * Provides sub-interfaces for HTTP and Socket operations.
 * All callbacks fire on the main game thread.
 *
 * Acquire via Services().Acquire<BML_NetworkInterface>().
 *
 * @code
 * auto net = Services().Acquire<BML_NetworkInterface>();
 * net->Http->RequestAsync(&req, cb, nullptr, data, &id);
 * net->Socket->Open(BML_SOCKET_TCP, "host", 80, cb, data, &sid);
 * @endcode
 */
typedef struct BML_NetworkInterface {
    BML_InterfaceHeader header;
    const BML_HttpSubInterface *Http;       /**< HTTP client sub-interface */
    const BML_SocketSubInterface *Socket;   /**< TCP/UDP socket sub-interface */
} BML_NetworkInterface;

BML_END_CDECLS

#endif /* BML_NETWORK_H */
