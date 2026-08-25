
/*
 * This module is gonna change quite a bit, for now we
 * are mainly focusing on verification, then we will polish.
 *
 * I will actually start a little summary for now of each stage:
 *  -> Phase 1: We did the CMAKE setup, the RAII clipboard read/write wrappers,
 *              and the Win32 'WM_CLIPBOARDUPDATE' loop.
 *      Notes:  Clipboard events can be registered twice, or more in Chromium, or
 *              Word, or VS Code. It is about how they operate with OS events.
 *              This is not a bug on our side, in the next phase we will work on
 *              loop prevention and deduplication among other things.
 */

#include "clipboard_guard.hpp"
#include "clipboard_listener.hpp"
#include <iostream>  // Because we don't have a proper replacement for 'std::cin' yet
#include <print>

/*
 * So before I was like, let's just use 'std::wcout, but it is having issues
 * with stuff like emojis... So let's actually transcode this. So we will move
 * to UTF-8 strings that <print> supports.
 * TODO: we will have to make a proper 'include/utils/' and so on to declutter
 *       the main module...
 */
std::string utf16_to_utf8(std::wstring_view wstr)
{
    if (wstr.empty())
    {
        return {};
    }

    int size_needed = ::WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
                                            nullptr, 0, nullptr, nullptr);

    std::string str_to(size_needed, 0);

    ::WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), str_to.data(),
                          size_needed, nullptr, nullptr);

    return str_to;
}

int main()
{
    std::println("=> Phase 1: Win32 clipboard verification...");
    std::println("Listening for clipboard events... Copy any text to test. Press Enter to quit.");

    zclip::ClipboardListener listener([](const std::wstring& text) {
        std::string utf8_text = utf16_to_utf8(text);
        std::println("[EVENT] Captured clipboard update: {}", utf8_text);
    });

    listener.start();
    std::cin.get();
    listener.stop();

    std::println("Listener cleanly stopped.");
}