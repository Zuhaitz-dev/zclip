
#include "net_peer.hpp"

#include <cstdio>
#include <print>
#include <system_error>

/*
 * There isn't really much to mention here, well, at least compared to C
 * luckily we already have algebraic sum types, say 'std::expected' (C++23),
 * or 'std::optional' (C++17 I think), ...
 *
 * It is a bit functional, but I am not really a big fan of OOP, I can handle it,
 * sure, but performance-wise it is usually more efficient to treat everything
 * as transformations over objects. This idea is not really completely and solely
 * functional, but it is more present there.
 *
 * With that and locality, and cache size and so on in mind we can get to a pretty good
 * DOD project. I mean, in this case it doesn't really matter, as this is asynchronous,
 * and even if synchronous, whichever CPU speed gains disappear when we put networking
 * in the middle...
 *
 * Either way, pretty much standard, main issue was the API as I am used to POSIX, but yeah.
 */
namespace zclip
{

TcpPeer::TcpPeer()
{
    WSADATA wsa_data;
    if (0 != ::WSAStartup(MAKEWORD(2, 2), &wsa_data))
    {
        std::println(stderr, "[Net] WSAStartup failed.");
    }
}

TcpPeer::~TcpPeer()
{
    disconnect();
    ::WSACleanup();
}

std::expected<void, NetError> TcpPeer::listen(uint16_t port)
{
    disconnect();

    m_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == m_socket)
    {
        return std::unexpected(NetError::SocketCreationError);
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = ::htons(port);

    if (SOCKET_ERROR ==
        ::bind(m_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)))
    {
        disconnect();
        return std::unexpected(NetError::BindFailed);
    }

    if (SOCKET_ERROR == ::listen(m_socket, SOMAXCONN))
    {
        disconnect();
        return std::unexpected(NetError::ListenFailed);
    }

    m_running.store(true);
    m_rx_thread = std::thread([this]() {
        SOCKET client_sock = ::accept(m_socket, nullptr, nullptr);
        if (INVALID_SOCKET != client_sock)
        {
            ::closesocket(m_socket);
            m_socket = client_sock;
            receive_loop();
        }
    });

    return {};
}

std::expected<void, NetError> TcpPeer::connect(const std::string& ip, uint16_t port)
{
    disconnect();

    m_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (INVALID_SOCKET == m_socket)
    {
        return std::unexpected(NetError::SocketCreationError);
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = ::htons(port);

    if (::inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0)
    {
        disconnect();
        return std::unexpected(NetError::ConnectFailed);
    }

    if (SOCKET_ERROR ==
        ::connect(m_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)))
    {
        disconnect();
        return std::unexpected(NetError::ConnectFailed);
    }

    m_running.store(true);
    m_rx_thread = std::thread(&TcpPeer::receive_loop, this);

    return {};
}

void TcpPeer::disconnect() noexcept
{
    if (m_running.exchange(false))
    {
        if (INVALID_SOCKET != m_socket)
        {
            ::shutdown(m_socket, SD_BOTH);
            ::closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }

        if (m_rx_thread.joinable())
        {
            m_rx_thread.join();
        }
    }
    m_parser.reset();
}

bool TcpPeer::send_payload(std::string_view utf8_payload)
{
    if (!m_running.load() || INVALID_SOCKET == m_socket)
    {
        return false;
    }

    const auto frame = encode_frame(utf8_payload);
    if (frame.empty())
    {
        return false;
    }

    int total_sent = 0;
    const int frame_size = static_cast<int>(frame.size());
    const char* data_ptr = reinterpret_cast<const char*>(frame.data());

    while (total_sent < frame_size)
    {
        const int bytes_sent = ::send(m_socket, data_ptr + total_sent, frame_size - total_sent, 0);
        if (SOCKET_ERROR == bytes_sent)
        {
            return false;
        }
        total_sent += bytes_sent;
    }

    return true;
}

void TcpPeer::receive_loop()
{
    std::vector<uint8_t> rx_buffer(4096);

    while (m_running.load())
    {
        const int bytes_read = ::recv(m_socket, reinterpret_cast<char*>(rx_buffer.data()),
                                      static_cast<int>(rx_buffer.size()), 0);

        if (bytes_read > 0)
        {
            m_parser.append(rx_buffer.data(), bytes_read);

            while (auto payload = m_parser.pop_frame())
            {
                if (m_on_frame_received)
                {
                    m_on_frame_received(*payload);
                }
            }
        }
        else if (0 == bytes_read || WSAEWOULDBLOCK != ::WSAGetLastError())
        {
            break;
        }
    }

    m_running.store(false);
}

}  // namespace zclip