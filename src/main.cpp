
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
 *      Phase 3.2: This is just a little change, the specifications mentioned the fact that it has
 * to "recover reasonably from a temporary connection loss". Before, we pretty much just gave up
 * with the client if it lost the connection, and we accept a new one when the server loses
 * connection because we explicitly closed the listening socket after the first client connected...
 *              This subsection is just to fit with the requirements given, in a proper way.
 */

#include "clipboard_guard.hpp"
#include "clipboard_listener.hpp"
#include "loop_preventer.hpp"

#include <iostream>  // Because we don't have a proper replacement for 'std::cin' yet
#include <print>

namespace
{

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

}  // namespace

int main()
{
    ::SetConsoleOutputCP(CP_UTF8);

    std::println("=> Phase 2.1: Clipboard monitoring with loop and duplicate prevention...");
    std::println(
        "Listening for clipboard events... Duplicate/multi-format OS echoes are suppressed.");
    std::println("Press Enter to terminate.");

    zclip::LoopPreventer preventer;

    zclip::ClipboardListener listener([&preventer](const std::wstring& text) {
        if (!preventer.test_and_record(text))
        {
            return;
        }

        std::string utf8_text = utf16_to_utf8(text);
        std::println("[EVENT] Captured clipboard update: {}", utf8_text);
    });

<<<<<<< Updated upstream
    listener.start();
    std::cin.get();
    listener.stop();

    std::println("Listener cleanly stopped.");
=======
    peer.set_on_disconnected(
        []() { std::println(stderr, "[Net] Connection lost! Initiating recovery..."); });

    constexpr std::int32_t PortCl = 6767;  // Sorry.
    const bool is_server = (argc > 1 && "--server" == std::string(argv[1]));

    std::println("Sync engine active. Press Ctrl+C to terminate.");

    listener.start();

    while (true)
    {
        if (is_server)
        {
            std::println("[Net] Starting server on port {}...", PortCl);
            if (peer.listen(PortCl).has_value())
            {
                peer.wait();  // So we block the main thread until the connection drops...
            }
            else
            {
                std::println(stderr, "[Error] Server failed to bind to port {}.", PortCl);
            }
        }
        else
        {
            std::println("[Net] Connecting to client on 127.0.0.1:{}...", PortCl);
            if (peer.connect("127.0.0.1", PortCl).has_value())
            {
                peer.wait();  // Same here.
            }
            else
            {
                std::println(stderr, "[Error] Failed to connect to 127.0.0.1:{}.", PortCl);
            }
        }

        std::println("[Net] Retrying in 3 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    // Unreachable under normal Ctrl+C exit, but good practice!
    listener.stop();
>>>>>>> Stashed changes
}