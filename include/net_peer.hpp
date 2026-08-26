
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "framing.hpp"

namespace zclip
{

enum class NetError
{
    WsaStartupFailed,
    SocketCreationError,
    BindFailed,
    ListenFailed,
    ConnectFailed,
    ConnectionClosed
};

using FrameReceivedCallback = std::function<void(const std::string& utf8_payload)>;

/*
 * this handles the asynchronous TCP connection.
 * Which either is incoming as a server or outgoing as a client.
 */
class TcpPeer
{
public:
    TcpPeer();
    ~TcpPeer();

    TcpPeer(const TcpPeer&) = delete;
    TcpPeer& operator=(const TcpPeer&) = delete;

    TcpPeer(TcpPeer&&) = delete;
    TcpPeer& operator=(TcpPeer&&) = delete;

    [[nodiscard]] std::expected<void, NetError> listen(uint16_t port);

    [[nodiscard]] std::expected<void, NetError> connect(const std::string& ip, uint16_t port);

    void disconnect() noexcept;

    bool send_payload(std::string_view utf8_payload);

    /*
     * This registers the callback for when a complete frame is parsed
     * from the network...
     */
    void set_on_frame_received(FrameReceivedCallback callback)
    {
        m_on_frame_received = std::move(callback);
    }

    void set_on_disconnected(std::function<void()> on_disconnected)
    {
        m_on_disconnected = std::move(on_disconnected);
    }

    void wait() const;

private:
    void receive_loop();

    SOCKET m_socket{INVALID_SOCKET};
    std::atomic<bool> m_running{false};
    std::thread m_rx_thread;

    FrameParser m_parser;
    FrameReceivedCallback m_on_frame_received;
    std::function<void()> m_on_disconnected;
};

}  // namespace zclip