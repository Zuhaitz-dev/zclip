
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
 *  -> Phase 2: Protocol framing, deduplication, and loop prevention.
 *      Phase 2.1: So here we are fixing the note above, also for example, if you
 *              are receiving clipboard data over the network and applying it
 *              can trigger synthetic OS capture events...
 *      Phase 2.2: protocol framing it is, find more information over 'include/framing.hpp'
 *              and 'src/framing.cpp'. It was quite simple, pretty much just making sure
 *              the frames are correct.
 *  -> Phase 3: So this section is about networking, server client, TCP, easy peasy.
 *              If I had to point out an issue, it is just that we are relying a lot on
 *              platform-specific libraries. Say WinSock2 now for example.
 *              By itself, it is not an issue within the mentioned requirements, but I did
 *              want to work on a proper PAL later, to support different platforms.
 *              It is going to be messy, this is one of those things that you must do from
 *              the beginning, I am afraid.
 *              I also thought about implementing a proper APE (Actually Portable Executable)
 *              but the fact that we are going for Win32 instead of POSIX makes it less portable.
 *              Also, if we actually went for a PAL, these two concepts would collide as the
 *              APE checks must be at run-time, not at compile-time.
 *              It is solvable, Cosmopolitan does offer run-time check alternatives, but
 *              it is an overkill for this task, it would be a task in itself.
 */

#include "clipboard_guard.hpp"
#include "clipboard_listener.hpp"
#include "loop_preventer.hpp"
#include "net_peer.hpp"

#include <cstdio>
#include <iostream>  // Because we don't have a proper replacement for 'std::cin' yet
#include <print>

namespace
{

/*
 * So before I was like, let's just use 'std::wcout', but it is having issues
 * with stuff like emojis... So let's actually transcode this. So we will move
 * to UTF-8 strings that <print> supports.
 * TODO: we will have to make a proper 'include/utils/' and so on to declutter
 *       the main module...
 */
[[nodiscard]] std::string utf16_to_utf8(std::wstring_view wstr)
{
    if (wstr.empty())
    {
        return {};
    }

    int size_needed = ::WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
                                            nullptr, 0, nullptr, nullptr);

    if (size_needed <= 0)
    {
        return {};
    }

    std::string str_to(static_cast<std::size_t>(size_needed), '\0');

    ::WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), str_to.data(),
                          size_needed, nullptr, nullptr);

    return str_to;
}

[[nodiscard]] std::wstring utf8_to_utf16(std::string_view str)
{
    if (str.empty())
    {
        return {};
    }

    const int size_needed =
        ::MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);

    if (size_needed <= 0)
    {
        return {};
    }

    std::wstring wstr_to(static_cast<size_t>(size_needed), L'\0');

    ::MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), wstr_to.data(),
                          size_needed);

    return wstr_to;
}

}  // namespace

int main(int argc, char* argv[])
{
    ::SetConsoleOutputCP(CP_UTF8);

    zclip::LoopPreventer preventer;
    zclip::TcpPeer peer;

    // Network -> local clipboard.
    peer.set_on_frame_received([&preventer](const std::string& utf8_payload) {
        std::println("[Net] Received payload from peer. Applying to OS...");

        const std::wstring wtext = utf8_to_utf16(utf8_payload);

        preventer.record_remote(wtext);
        preventer.set_remote_suppression(true);

        auto result = zclip::write_text_utf16(wtext);

        preventer.set_remote_suppression(false);

        if (!result.has_value())
        {
            std::println(stderr, "[Error] Failed to write to OS clipboard.");
        }
    });

    // Local clipboard -> network.
    zclip::ClipboardListener listener([&preventer, &peer](std::wstring_view wtext) {
        if (!preventer.test_and_record(wtext))
        {
            return;
        }

        std::println("[OS] Captured fresh clipboard update. Sending to peer...");
        peer.send_payload(utf16_to_utf8(wtext));
    });

    constexpr std::int32_t PortCl = 6767;   // Sorry.

    // Simple CLI for P2P routing, but we will improve this... TODO yeah
    // Also hardcoding ports is... meh? We gotta change that.
    if (argc > 1 && "--server" == std::string(argv[1]))
    {
        std::println("Starting server on port {}...", PortCl);

        if (!peer.listen(PortCl).has_value())
        {
            return 1;
        }
    }
    else
    {
        std::println("Connecting to client on 127.0.0.1:{}...", PortCl);

        if (!peer.connect("127.0.0.1", PortCl).has_value())
        {
            return 1;
        }
    }

    std::println("Sync engine active. Press Enter to terminate.");

    listener.start();
    std::cin.get();

    listener.stop();
    peer.disconnect();

    std::println("Terminated cleanly.");
}