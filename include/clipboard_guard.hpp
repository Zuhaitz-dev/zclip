/*
 * TODO: add proper headers later...
 */

#pragma once

/*
 * I don't really rely on win32 that much, I am mostly with POSIX,
 * but I never forget this one because for some reason the name makes me laugh.
 * Gotta say it is easier than defining the '_POSIX_C_SOURCE', although maybe that
 * gives you greater control and if it is clearer if you want a version or another.
 * I prefer to expose than to exclude.
 *
 * Also, note for myself, shall we consider using imports actually? It is "modern C++",
 * but it is not that common in codebases (although it clearly has its compile-time speed benefits).
 * I remember Stroustrup mentioned it when I went to one of his conferences. But I don't know...
 * Maybe I will work on that later, with a little abstraction layer.
 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

/*
 * Also useful to reduce polution (and the name is clearer and more direct).
 */
#ifndef NOMINMAX
#define NOMINMAX
#endif

/*
 * I tend to dislike putting platform-specific modules raw, I usually design a Platform Abstraction
 * Layer, aka PAL, but the requirement mentioned Windows-only, might change this later tho. And we
 * could change the other macros up there too, at least encapsulate them properly.
 */
#include <windows.h>

/*
 * Another of the reasons to use C++23: https://en.cppreference.com/cpp/utility/expected
 * This will make the logger that we should make later easier.
 */
#include <expected>

#include <chrono>
#include <concepts>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace zclip
{

enum class ClipboardError
{
    LockFailed,
    FormatUnavailable,
    AllocationFailed,
    MemoryLockFailed,
    EmptyPayload
};

/*
 * The RAII Guard for OpenClipboard and CloseClipboard...
 */
class ClipboardSession
{
public:
    static constexpr int DefaultMaxRetries = 5;
    static constexpr std::chrono::milliseconds DefaultRetryDelay{10};

    explicit ClipboardSession(HWND owner = nullptr, int max_retries = DefaultMaxRetries,
                              std::chrono::milliseconds retry_delay = DefaultRetryDelay);

    ~ClipboardSession();

    /*
     * Classic Rule of Five... We focus on moves, remove copies.
     */
    ClipboardSession(const ClipboardSession&) = delete;
    ClipboardSession& operator=(const ClipboardSession&) = delete;

    ClipboardSession(ClipboardSession&& other) noexcept : m_open(std::exchange(other.m_open, false))
    {
    }

    ClipboardSession& operator=(ClipboardSession&& other) noexcept
    {
        if (this != &other)
        {
            if (m_open)
            {
                ::CloseClipboard();
            }

            m_open = std::exchange(other.m_open, false);
        }
<<<<<<< Updated upstream
=======

        return *this;  // I forgot.
>>>>>>> Stashed changes
    }

    /*
     * I like the attributes syntax, mainly as also a C dev, because before C23
     * they were extensions (some quite available everywhere), but the syntax was
     * '__attribute__((...))' instead of '[[...]]', problem is, this actually added
     * namespaces to C (for like '[[gnu::packed]]'), at least for attributes,
     * and it feels quite blasphemous.
     */
    [[nodiscard]] bool is_open() const noexcept { return m_open; }

    [[nodiscard]] explicit operator bool() const noexcept { return m_open; }

private:
    bool m_open{false};
};

/*
 * RAII wrapper around GlobalLock & GlobalUnlock...
 */
template <typename T> class GlobalMemoryLock
{
public:
    explicit GlobalMemoryLock(HGLOBAL handle)
        : m_handle(handle), m_ptr(handle ? static_cast<T*>(::GlobalLock(handle)) : nullptr)
    {
    }

    ~GlobalMemoryLock() { reset(); }

    /*
     * Rule of Five!
     */
    GlobalMemoryLock(const GlobalMemoryLock&) = delete;
    GlobalMemoryLock& operator=(const GlobalMemoryLock&) = delete;

    GlobalMemoryLock(GlobalMemoryLock&& other) noexcept
        : m_handle(std::exchange(other.m_handle, nullptr)),
          m_ptr(std::exchange(other.m_ptr, nullptr))
    {
    }

    GlobalMemoryLock& operator=(GlobalMemoryLock&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            m_handle = std::exchange(other.m_handle, nullptr);
            m_ptr = std::exchange(other.m_ptr, nullptr);
        }
        return *this;
    }

    void reset() noexcept
    {
        if (m_ptr && m_handle)
        {
            ::GlobalUnlock(m_handle);
            m_ptr = nullptr;
            m_handle = nullptr;
        }
    }

    [[nodiscard]] T* get() const noexcept { return m_ptr; }

    [[nodiscard]] explicit operator bool() const noexcept { return m_ptr != nullptr; }

private:
    HGLOBAL m_handle{nullptr};
    T* m_ptr{nullptr};
};

/*
 * Some utilities for safe Unicode read/write. Time to use some algebraic sum types.
 *
 * Also, little note, time to share some history... So I know why wide strings were introduced in C,
 * and it is pretty much the same for C++. In C, an amendment was made, only one, the C94/C95, and
 * that introduced the wide strings, and the digraphs. For the digraphs, there's a Stroustrup paper
 * you can check: https://www.open-std.org/jtc1/sc22/wg14/www/docs/n117.pdf
 *
 * So for digraphs, mainly Denmark was putting pressure to the ISO committee. For wide strings (or
 * wide chars) it was mainly Japan from what I read (page 7):
 * https://www.open-std.org/jtc1/sc22/wg14/www/docs/n224.pdf Also here you have a proposed wording:
 * https://www.open-std.org/jtc1/sc22/wg14/www/docs/n020.pdf There's also a letter directly from the
 * Japanese part of the committee, but I don't find it now.
 *
 * Point is, funnily, if we all had waited a few years more, we could have had proper Unicode
 * directly, but this was too important to wait for.
 */
[[nodiscard]] std::expected<std::wstring, ClipboardError> read_text_utf16();
[[nodiscard]] std::expected<void, ClipboardError> write_text_utf16(std::wstring_view text);

}  // namespace zclip