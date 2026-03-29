#include "SocketWorker.h"

#include <cstdio>

#pragma comment(lib, "ws2_32.lib")

static constexpr int kRecvBufferSize = 8192;
static constexpr int kSelectTimeoutMs = 100;

static std::string FormatAddress(const sockaddr_storage &addr) {
    char buf[INET6_ADDRSTRLEN] = {};
    if (addr.ss_family == AF_INET) {
        inet_ntop(AF_INET, &reinterpret_cast<const sockaddr_in &>(addr).sin_addr, buf, sizeof(buf));
    } else if (addr.ss_family == AF_INET6) {
        inet_ntop(AF_INET6, &reinterpret_cast<const sockaddr_in6 &>(addr).sin6_addr, buf, sizeof(buf));
    }
    return buf;
}

static uint16_t GetPort(const sockaddr_storage &addr) {
    if (addr.ss_family == AF_INET)
        return ntohs(reinterpret_cast<const sockaddr_in &>(addr).sin_port);
    if (addr.ss_family == AF_INET6)
        return ntohs(reinterpret_cast<const sockaddr_in6 &>(addr).sin6_port);
    return 0;
}

static std::string WsaErrorString(int code) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Winsock error %d", code);
    return buf;
}

// ============================================================================
// SocketWorker -- Lifecycle
// ============================================================================

SocketWorker::SocketWorker() = default;

SocketWorker::~SocketWorker() {
    Stop();
}

bool SocketWorker::Start() {
    if (m_Initialized) return true;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    m_Initialized = true;
    return true;
}

void SocketWorker::Stop() {
    // Close all sockets -- this will cause worker threads to exit
    std::vector<uint32_t> ids;
    {
        std::lock_guard lock(m_Mutex);
        for (auto &[id, entry] : m_Sockets) ids.push_back(id);
    }
    for (uint32_t id : ids) Close(id);

    if (m_Initialized) {
        WSACleanup();
        m_Initialized = false;
    }

    std::lock_guard lock(m_EventMutex);
    m_Events.clear();
}

// ============================================================================
// SocketWorker -- Open / Send / Close
// ============================================================================

BML_Result SocketWorker::Open(BML_SocketType type, const char *host, uint16_t port,
                               BML_SocketCallback callback, void *user_data, uint32_t id) {
    if (!host || !callback) return BML_RESULT_INVALID_ARGUMENT;
    if (!m_Initialized) return BML_RESULT_NOT_INITIALIZED;

    auto entry = std::make_unique<SocketEntry>();
    entry->id = id;
    entry->type = type;
    entry->host = host;
    entry->port = port;
    entry->callback = callback;
    entry->user_data = user_data;
    entry->running.store(true);

    auto *rawPtr = entry.get();
    {
        std::lock_guard lock(m_Mutex);
        m_Sockets[id] = std::move(entry);
    }

    if (type == BML_SOCKET_TCP) {
        rawPtr->worker = std::thread(&SocketWorker::TcpWorkerLoop, this, id);
    } else {
        rawPtr->worker = std::thread(&SocketWorker::UdpWorkerLoop, this, id);
    }

    return BML_RESULT_OK;
}

BML_Result SocketWorker::Send(uint32_t id, const void *data, size_t size) {
    if (!data || size == 0) return BML_RESULT_INVALID_ARGUMENT;
    auto *entry = FindEntry(id);
    if (!entry) return BML_RESULT_NOT_FOUND;

    std::vector<char> buf(static_cast<const char *>(data), static_cast<const char *>(data) + size);
    std::lock_guard lock(entry->send_mutex);
    if (entry->send_queue_bytes + size > SocketEntry::kMaxSendQueueBytes)
        return BML_RESULT_WOULD_BLOCK;
    entry->send_queue.push_back(std::move(buf));
    entry->send_queue_bytes += size;
    return BML_RESULT_OK;
}

BML_Result SocketWorker::SendTo(uint32_t id, const char *host, uint16_t port,
                                 const void *data, size_t size) {
    if (!host || !data || size == 0) return BML_RESULT_INVALID_ARGUMENT;
    auto *entry = FindEntry(id);
    if (!entry || entry->type != BML_SOCKET_UDP) return BML_RESULT_NOT_FOUND;

    SocketEntry::UdpDatagram dgram;
    dgram.host = host;
    dgram.port = port;
    dgram.data.assign(static_cast<const char *>(data), static_cast<const char *>(data) + size);

    std::lock_guard lock(entry->send_mutex);
    if (entry->send_queue_bytes + size > SocketEntry::kMaxSendQueueBytes)
        return BML_RESULT_WOULD_BLOCK;
    entry->sendto_queue.push_back(std::move(dgram));
    entry->send_queue_bytes += size;
    return BML_RESULT_OK;
}

BML_Result SocketWorker::Close(uint32_t id) {
    std::unique_ptr<SocketEntry> entry;
    {
        std::lock_guard lock(m_Mutex);
        auto it = m_Sockets.find(id);
        if (it == m_Sockets.end()) return BML_RESULT_NOT_FOUND;
        entry = std::move(it->second);
        m_Sockets.erase(it);
    }

    entry->running.store(false);
    if (entry->handle != INVALID_SOCKET) {
        closesocket(entry->handle);
        entry->handle = INVALID_SOCKET;
    }
    if (entry->worker.joinable()) entry->worker.join();
    return BML_RESULT_OK;
}

uint32_t SocketWorker::OpenCount() const {
    std::lock_guard lock(m_Mutex);
    return static_cast<uint32_t>(m_Sockets.size());
}

// ============================================================================
// SocketWorker -- Event Delivery
// ============================================================================

void SocketWorker::DrainEvents() {
    std::vector<SocketEventQueued> events;
    {
        std::lock_guard lock(m_EventMutex);
        events.swap(m_Events);
    }

    for (const auto &ev : events) {
        if (!ev.callback) continue;

        BML_SocketEventData data = BML_SOCKET_EVENT_DATA_INIT;
        data.socket_id = ev.id;
        data.event = ev.event;
        data.data = ev.data.empty() ? nullptr : ev.data.c_str();
        data.data_size = ev.data.size();
        data.error = ev.error.empty() ? nullptr : ev.error.c_str();
        data.remote_addr = ev.remote_addr.empty() ? nullptr : ev.remote_addr.c_str();
        data.remote_port = ev.remote_port;

        ev.callback(&data, ev.user_data);
    }
}

void SocketWorker::PostEvent(const SocketEntry &entry, BML_SocketEvent event,
                              const std::string &data, const std::string &error,
                              const std::string &addr, uint16_t port) {
    SocketEventQueued ev;
    ev.id = entry.id;
    ev.event = event;
    ev.data = data;
    ev.error = error;
    ev.remote_addr = addr;
    ev.remote_port = port;
    ev.callback = entry.callback;
    ev.user_data = entry.user_data;

    std::lock_guard lock(m_EventMutex);
    m_Events.push_back(std::move(ev));
}

SocketEntry *SocketWorker::FindEntry(uint32_t id) {
    std::lock_guard lock(m_Mutex);
    auto it = m_Sockets.find(id);
    return it != m_Sockets.end() ? it->second.get() : nullptr;
}

void SocketWorker::RemoveEntry(uint32_t id) {
    std::lock_guard lock(m_Mutex);
    m_Sockets.erase(id);
}

// ============================================================================
// SocketWorker -- TCP Thread
// ============================================================================

void SocketWorker::TcpWorkerLoop(uint32_t id) {
    auto *entry = FindEntry(id);
    if (!entry) return;

    // Resolve address
    addrinfo hints{}, *result = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char portStr[8];
    std::snprintf(portStr, sizeof(portStr), "%u", entry->port);

    if (getaddrinfo(entry->host.c_str(), portStr, &hints, &result) != 0 || !result) {
        PostEvent(*entry, BML_SOCKET_EVENT_ERROR, {}, WsaErrorString(WSAGetLastError()));
        return;
    }

    SOCKET sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET) {
        PostEvent(*entry, BML_SOCKET_EVENT_ERROR, {}, WsaErrorString(WSAGetLastError()));
        freeaddrinfo(result);
        return;
    }

    if (connect(sock, result->ai_addr, static_cast<int>(result->ai_addrlen)) == SOCKET_ERROR) {
        PostEvent(*entry, BML_SOCKET_EVENT_ERROR, {}, WsaErrorString(WSAGetLastError()));
        closesocket(sock);
        freeaddrinfo(result);
        return;
    }

    sockaddr_storage peerAddr{};
    std::memcpy(&peerAddr, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);

    entry->handle = sock;
    PostEvent(*entry, BML_SOCKET_EVENT_CONNECTED, {}, {},
              FormatAddress(peerAddr), GetPort(peerAddr));

    // I/O loop using select
    char recvBuf[kRecvBufferSize];
    while (entry->running.load()) {
        // Flush send queue
        {
            std::vector<std::vector<char>> toSend;
            {
                std::lock_guard lock(entry->send_mutex);
                toSend.swap(entry->send_queue);
                entry->send_queue_bytes = 0;
            }
            for (auto &buf : toSend) {
                int sent = send(sock, buf.data(), static_cast<int>(buf.size()), 0);
                if (sent == SOCKET_ERROR) {
                    PostEvent(*entry, BML_SOCKET_EVENT_ERROR, {}, WsaErrorString(WSAGetLastError()));
                    entry->running.store(false);
                    break;
                }
            }
        }
        if (!entry->running.load()) break;

        // Check for incoming data with timeout
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(sock, &readSet);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = kSelectTimeoutMs * 1000;

        int sel = select(0, &readSet, nullptr, nullptr, &tv);
        if (sel > 0 && FD_ISSET(sock, &readSet)) {
            int received = recv(sock, recvBuf, kRecvBufferSize, 0);
            if (received > 0) {
                PostEvent(*entry, BML_SOCKET_EVENT_DATA,
                          std::string(recvBuf, received), {},
                          FormatAddress(peerAddr), GetPort(peerAddr));
            } else if (received == 0) {
                PostEvent(*entry, BML_SOCKET_EVENT_CLOSED);
                break;
            } else {
                PostEvent(*entry, BML_SOCKET_EVENT_ERROR, {}, WsaErrorString(WSAGetLastError()));
                break;
            }
        } else if (sel == SOCKET_ERROR) {
            PostEvent(*entry, BML_SOCKET_EVENT_ERROR, {}, WsaErrorString(WSAGetLastError()));
            break;
        }
    }

    closesocket(sock);
    entry->handle = INVALID_SOCKET;
}

// ============================================================================
// SocketWorker -- UDP Thread
// ============================================================================

void SocketWorker::UdpWorkerLoop(uint32_t id) {
    auto *entry = FindEntry(id);
    if (!entry) return;

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        PostEvent(*entry, BML_SOCKET_EVENT_ERROR, {}, WsaErrorString(WSAGetLastError()));
        return;
    }

    // Bind to ephemeral port
    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = INADDR_ANY;
    bindAddr.sin_port = 0;
    if (bind(sock, reinterpret_cast<sockaddr *>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR) {
        PostEvent(*entry, BML_SOCKET_EVENT_ERROR, {}, WsaErrorString(WSAGetLastError()));
        closesocket(sock);
        return;
    }

    entry->handle = sock;

    // Resolve default target (host:port from Open) once
    sockaddr_storage defaultTarget{};
    int defaultTargetLen = 0;
    {
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        char portStr[8];
        std::snprintf(portStr, sizeof(portStr), "%u", entry->port);
        if (getaddrinfo(entry->host.c_str(), portStr, &hints, &res) == 0 && res) {
            defaultTargetLen = static_cast<int>(res->ai_addrlen);
            std::memcpy(&defaultTarget, res->ai_addr, res->ai_addrlen);
            freeaddrinfo(res);
        }
    }

    PostEvent(*entry, BML_SOCKET_EVENT_CONNECTED);

    char recvBuf[kRecvBufferSize];
    while (entry->running.load()) {
        // Flush send queue
        {
            std::vector<std::vector<char>> toSend;
            std::vector<SocketEntry::UdpDatagram> toSendTo;
            {
                std::lock_guard lock(entry->send_mutex);
                toSend.swap(entry->send_queue);
                toSendTo.swap(entry->sendto_queue);
                entry->send_queue_bytes = 0;
            }

            // Plain sends (to the cached default target)
            for (auto &buf : toSend) {
                if (defaultTargetLen > 0) {
                    sendto(sock, buf.data(), static_cast<int>(buf.size()), 0,
                           reinterpret_cast<const sockaddr *>(&defaultTarget), defaultTargetLen);
                }
            }

            // SendTo datagrams (per-call DNS, different targets)
            for (auto &dgram : toSendTo) {
                addrinfo hints{}, *res = nullptr;
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_DGRAM;
                char portStr[8];
                std::snprintf(portStr, sizeof(portStr), "%u", dgram.port);
                if (getaddrinfo(dgram.host.c_str(), portStr, &hints, &res) == 0 && res) {
                    sendto(sock, dgram.data.data(), static_cast<int>(dgram.data.size()), 0,
                           res->ai_addr, static_cast<int>(res->ai_addrlen));
                    freeaddrinfo(res);
                }
            }
        }

        // Receive with timeout
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(sock, &readSet);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = kSelectTimeoutMs * 1000;

        int sel = select(0, &readSet, nullptr, nullptr, &tv);
        if (sel > 0 && FD_ISSET(sock, &readSet)) {
            sockaddr_storage from{};
            int fromLen = sizeof(from);
            int received = recvfrom(sock, recvBuf, kRecvBufferSize, 0,
                                     reinterpret_cast<sockaddr *>(&from), &fromLen);
            if (received > 0) {
                PostEvent(*entry, BML_SOCKET_EVENT_DATA,
                          std::string(recvBuf, received), {},
                          FormatAddress(from), GetPort(from));
            } else if (received == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err != WSAEWOULDBLOCK && err != WSAECONNRESET) {
                    PostEvent(*entry, BML_SOCKET_EVENT_ERROR, {}, WsaErrorString(err));
                    break;
                }
            }
        }
    }

    closesocket(sock);
    entry->handle = INVALID_SOCKET;
}
