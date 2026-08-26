
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zclip
{

/*
 * This is a little detail, that is quite niche, but if we are pedantic,
 * we must always use the std namespace for these C types. It is weird for me ngl.
 * If I am not wrong, by the standard since C++11, libraries like <cstdint>
 * must have these types in the std namespace, but it doesn't forbid them from being in the
 * global namespace... So in most implementations it is just a wrapper of <stdint.h>,
 * but it is not necessarily true always.
 */
inline constexpr std::size_t HeaderSize{sizeof(std::uint32_t)};
inline constexpr std::uint32_t MaxPayloadSize{
    10 * 1024 * 1024};  // 10 MB of safety threshold should be alright.

/*
 * So we gotta deal with the networking part later, but we are in the upper layer of OSI first...
 * Therefore we gotta talk of frames. Ours will look like this:
 *
 *  +-----------------------+-------------------------------+
 *  | Length (4 bytes)      | Payload (Length bytes, UTF-8) |
 *  | Big-Endian uint32_t   | Raw text data                 |
 *  +-----------------------+-------------------------------+
 *
 * This step requires care, that's why we use integral types with exact byte size in different
 * platforms (we don't want to deal with the cases like 'long' in Linux vs 'long' in Windows). The
 * max payload size is mainly to prevent malicious memory allocation or DoS on the receiving
 * socket... And we go for Big-Endian.
 *
 * Fun fact, the term comes from Gulliver's Travels, 1726.
 * You can thank Lilliputians now.
 * https://www.ling.upenn.edu/courses/Spring_2003/ling538/Lecnotes/ADfn1.htm
 */
[[nodiscard]] std::vector<std::uint8_t> encode_frame(std::string_view payload);

class FrameParser
{
public:
    FrameParser() = default;

    void append(const std::uint8_t* data, std::size_t size);

    [[nodiscard]] std::optional<std::string> pop_frame();

    void reset() noexcept;

    [[nodiscard]] std::size_t buffered_bytes() const noexcept;

private:
    /*
     * It is midnight now, but I know that if it was a bit later, I would
     * end up writing a 'std::vector<bool>'. God have mercy if that happened.
     */
    std::vector<std::uint8_t> m_buffer;
};

}  // namespace zclip