
#include "clipboard_guard.hpp"

#include <cstring>

namespace zclip
{

ClipboardSession::ClipboardSession(HWND owner, int max_retries,
                                   std::chrono::milliseconds retry_delay)
{
    for (int attempt{}; attempt < max_retries; ++attempt)
    {
        if (::OpenClipboard(owner))
        {
            m_open = true;
            return;
        }
        std::this_thread::sleep_for(retry_delay);
    }
}

ClipboardSession::~ClipboardSession()
{
    if (m_open)
    {
        ::CloseClipboard();
    }
}

std::expected<std::wstring, ClipboardError> read_text_utf16()
{
    ClipboardSession session;

    if (!session.is_open())
    {
        return std::unexpected(ClipboardError::LockFailed);
    }

    if (!::IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        return std::unexpected(ClipboardError::FormatUnavailable);
    }

    HANDLE handle = ::GetClipboardData(CF_UNICODETEXT);

    /*
     * I like the "Yoda style" when dealing with '==', because
     * it is quite frequent to forget about the extra '=', and while
     * most compilers warn about it nowadays, it is better to get
     * a compile-time error by default than just a warning.
     */
    if (nullptr == handle)
    {
        return std::unexpected(ClipboardError::AllocationFailed);
    }

    GlobalMemoryLock<const wchar_t> lock(static_cast<HGLOBAL>(handle));

    if (!lock)
    {
        return std::unexpected(ClipboardError::MemoryLockFailed);
    }

    return std::wstring(lock.get());
}

std::expected<void, ClipboardError> write_text_utf16(std::wstring_view text)
{
    if (text.empty())
    {
        return std::unexpected(ClipboardError::EmptyPayload);
    }

    /*
     * I like a lot of stuff related to C, but I hate its strings, which C++
     * also adopted. Sure, it makes sense, mainly when memory is scarce, but
     * something like UCSD (Pascal) strings.
     *
     * You can actually use them in C/C++ funnily, with '-fpascal-strings' you can do
     * something like:
     *      const unsigned char a[] = "\pHello";
     *
     * But to be honest it is useless because the C standard library is how it is.
     * It is good if you want to get some external Pascal routines, though,
     * but that's quite niche.
     *
     * More info here:
     * https://leopard-adc.pepas.com/documentation/DeveloperTools/gcc-3.3/gcc/Pascal-Strings.html
     * Note: I checked the current documentation for the current version and I don't find it...
     *       Therefore maybe it was removed. So be careful.
     */
    const size_t byte_count = (text.size() + 1) * sizeof(wchar_t);

    /*
     * Things with POSIX and Win32... Their API is so C-ish, which I do not mind,
     * but wherever you want to go for RAIIs and exceptions and whichever, you end up
     * with the same pattern as a 'malloc'/'free' in this case.
     */
    HGLOBAL h_global = ::GlobalAlloc(GMEM_MOVEABLE, byte_count);
    if (!h_global)
    {
        return std::unexpected(ClipboardError::AllocationFailed);
    }

    {
        GlobalMemoryLock<wchar_t> lock(h_global);
        if (!lock)
        {
            ::GlobalFree(h_global);
            return std::unexpected(ClipboardError::MemoryLockFailed);
        }

        /*
         * I could use something more efficient, but the compiler will optimize it to
         * the specific '__builtin_memcpy' available (in MSVC I think it is different).
         * Note: just checked it and we could force it with an intrinsic pragma.
         *       But '/O2' and '/Ox' already activates '/Oi' which does the replacement.
         */
        std::memcpy(lock.get(), text.data(), text.size() * sizeof(wchar_t));
        lock.get()[text.size()] = L'\0';
    }

    ClipboardSession session;

    if (!session.is_open())
    {
        ::GlobalFree(h_global);
        return std::unexpected(ClipboardError::LockFailed);
    }

    if (!::EmptyClipboard())
    {
        ::GlobalFree(h_global);
        return std::unexpected(ClipboardError::LockFailed);
    }

    if (!::SetClipboardData(CF_UNICODETEXT, h_global))
    {
        ::GlobalFree(h_global);
        return std::unexpected(ClipboardError::AllocationFailed);
    }

    /*
     * The OS now owns the HGLOBAL.
     */
    return {};
}

}  // namespace zclip