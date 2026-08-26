
#include "loop_preventer.hpp"

namespace zclip
{

bool LoopPreventer::test_and_record(std::wstring_view text)
{
    if (m_suppress_local_capture.load(std::memory_order_acquire))
    {
        return false;
    }

    if (text.empty())
    {
        return false;
    }

    const std::uint64_t hash{hash_payload(text)};

    std::scoped_lock lock(m_mutex);

    for (const std::uint64_t cached_hash : m_recent_hashes)
    {
        if (cached_hash == hash)
        {
            return false;
        }
    }

    m_recent_hashes.at(m_cursor % RingBufferSize) = hash;
    m_cursor = (m_cursor + 1) % RingBufferSize;
    return true;
}

void LoopPreventer::record_remote(std::wstring_view text)
{
    if (text.empty())
    {
        return;
    }

    const std::uint64_t hash = hash_payload(text);

    std::scoped_lock lock(m_mutex);
    m_recent_hashes.at(m_cursor % RingBufferSize) = hash;
    m_cursor = (m_cursor + 1) % RingBufferSize;
}

void LoopPreventer::set_remote_suppression(bool suppress) noexcept
{
    m_suppress_local_capture.store(suppress, std::memory_order_release);
}

bool LoopPreventer::is_suppressed() const noexcept
{
    return m_suppress_local_capture.load(std::memory_order_acquire);
}

void LoopPreventer::reset() noexcept
{
    std::scoped_lock lock(m_mutex);
    m_recent_hashes.fill(0);
    m_cursor = 0;
    m_suppress_local_capture.store(false, std::memory_order_relaxed);
}

}  // namespace zclip