
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>

namespace zclip
{

/*
 * The zero-allocation 64-bit FNV-1a hash for UTF-16 payload deduplication...
 * There isn't much to say, it is just implementing the algorithm.
 */
[[nodiscard]] constexpr uint64_t hash_payload(std::wstring_view str) noexcept
{
    constexpr uint64_t fnv_offset = 14695981039346656037ULL;
    constexpr uint64_t fnv_prime = 1099511628211ULL;

    uint64_t hash = fnv_offset;
    for (const wchar_t c : str)
    {
        hash ^= static_cast<uint64_t>(c);
        hash *= fnv_prime;
    }
    return hash;
}

/*
 * Deduplicates multi-format OS notifications.
 * & supresses infinite network echoes...
 */
class LoopPreventer
{
public:
    LoopPreventer() = default;
    ~LoopPreventer() = default;

    LoopPreventer(const LoopPreventer&) = delete;
    LoopPreventer& operator=(const LoopPreventer&) = delete;
    LoopPreventer(LoopPreventer&&) = delete;
    LoopPreventer& operator=(LoopPreventer&&) = delete;

    // Evaluates whether the incoming text is fresh or a recent duplicate...
    [[nodiscard]] bool test_and_record(std::wstring_view text);

    void record_remote(std::wstring_view text);

    void set_remote_suppresion(bool suppress) noexcept;

    [[nodiscard]] bool is_suppressed() const noexcept;

    /*
     * I will use this mainly for testing later.
     */
    void reset() noexcept;

private:
    static constexpr size_t RingBufferSize = 8;
    std::array<std::uint64_t, RingBufferSize> m_recent_hashes{};
    size_t m_cursor{0};
    mutable std::mutex m_mutex;
    std::atomic<bool> m_suppress_local_capture{false};
};

}  // namespace zclip
