
#include "framing.hpp"

#include <algorithm>
#include <bit>
#include <cstring>

namespace zclip
{

namespace
{

[[nodiscard]] constexpr std::uint32_t host_to_network32(std::uint32_t value) noexcept
{
    if constexpr (std::endian::native == std::endian::little)
    {
        return std::byteswap(value);
    }
    return value;
}

[[nodiscard]] constexpr std::uint32_t network_to_host32(std::uint32_t value) noexcept
{
    if constexpr (std::endian::native == std::endian::little)
    {
        return std::byteswap(value);
    }
    return value;
}

}  // namespace

std::vector<std::uint8_t> encode_frame(std::string_view payload)
{
    if (payload.size() > MaxPayloadSize)
    {
        return {};
    }

    const std::uint32_t payload_len = static_cast<std::uint32_t>(payload.size());
    const std::uint32_t net_len = host_to_network32(payload_len);

    std::vector<uint8_t> frame(HeaderSize + payload.size());
    std::memcpy(frame.data(), &net_len, HeaderSize);

    if (!payload.empty())
    {
        std::memcpy(frame.data() + HeaderSize, payload.data(), payload.size());
    }

    return frame;
}

void FrameParser::append(const std::uint8_t* data, std::size_t size)
{
    if (nullptr != data && size > 0)
    {
        m_buffer.insert(m_buffer.end(), data, data + size);
    }
}

std::optional<std::string> FrameParser::pop_frame()
{
    if (m_buffer.size() < HeaderSize)
    {
        return std::nullopt;
    }

    std::uint32_t net_len{};
    std::memcpy(&net_len, m_buffer.data(), HeaderSize);
    const std::uint32_t payload_len{network_to_host32(net_len)};

    if (payload_len > MaxPayloadSize)
    {
        /*
         * Protocol corruption or oversized frame,
         * so we clear the buffer to protect process.
         */
        m_buffer.clear();
        return std::nullopt;
    }

    const std::size_t total_required{HeaderSize + payload_len};
    if (m_buffer.size() < total_required)
    {
        /*
         * Frame payload not fully received yet.
         */
        return std::nullopt;
    }

    std::string payload(reinterpret_cast<const char*>(m_buffer.data() + HeaderSize), payload_len);

    /*
     * We erase extracted frame from buffer.
     */
    m_buffer.erase(m_buffer.begin(),
                   m_buffer.begin() + static_cast<std::ptrdiff_t>(total_required));

    return payload;
}

void FrameParser::reset() noexcept
{
    m_buffer.clear();
}

size_t FrameParser::buffered_bytes() const noexcept
{
    return m_buffer.size();
}

}  // namespace zclip