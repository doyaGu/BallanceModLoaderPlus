#ifndef BML_NETWORK_SOCKETWORKER_H
#define BML_NETWORK_SOCKETWORKER_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "bml_network.h"

// Per-socket state managed by a dedicated thread
struct SocketEntry {
    uint32_t id = 0;
    BML_SocketType type = BML_SOCKET_TCP;
    SOCKET handle = INVALID_SOCKET;
    std::string host;
    uint16_t port = 0;
    BML_SocketCallback callback = nullptr;
    void *user_data = nullptr;
    std::thread worker;
    std::atomic<bool> running{false};

    // Send queue (for async sends from main thread)
    std::mutex send_mutex;
    std::vector<std::vector<char>> send_queue;

    // SendTo queue (UDP only)
    struct UdpDatagram {
        std::string host;
        uint16_t port;
        std::vector<char> data;
    };
    std::vector<UdpDatagram> sendto_queue;
};

// Event queued for main-thread delivery
struct SocketEventQueued {
    uint32_t id = 0;
    BML_SocketEvent event = BML_SOCKET_EVENT_ERROR;
    std::string data;
    std::string error;
    std::string remote_addr;
    uint16_t remote_port = 0;
    BML_SocketCallback callback = nullptr;
    void *user_data = nullptr;
};

class SocketWorker {
public:
    SocketWorker();
    ~SocketWorker();

    SocketWorker(const SocketWorker &) = delete;
    SocketWorker &operator=(const SocketWorker &) = delete;

    bool Start();
    void Stop();

    uint32_t AllocateId() { return m_NextId.fetch_add(1); }

    BML_Result Open(BML_SocketType type, const char *host, uint16_t port,
                    BML_SocketCallback callback, void *user_data, uint32_t id);
    BML_Result Send(uint32_t id, const void *data, size_t size);
    BML_Result SendTo(uint32_t id, const char *host, uint16_t port, const void *data, size_t size);
    BML_Result Close(uint32_t id);
    uint32_t OpenCount() const;

    void DrainEvents();

private:
    void TcpWorkerLoop(uint32_t id);
    void UdpWorkerLoop(uint32_t id);
    void PostEvent(const SocketEntry &entry, BML_SocketEvent event,
                   const std::string &data = {}, const std::string &error = {},
                   const std::string &addr = {}, uint16_t port = 0);
    SocketEntry *FindEntry(uint32_t id);
    void RemoveEntry(uint32_t id);

    mutable std::mutex m_Mutex;
    std::unordered_map<uint32_t, std::unique_ptr<SocketEntry>> m_Sockets;

    std::mutex m_EventMutex;
    std::vector<SocketEventQueued> m_Events;

    std::atomic<uint32_t> m_NextId{1};
    bool m_Initialized = false;
};

#endif // BML_NETWORK_SOCKETWORKER_H
