
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
 *              to "recover reasonably from a temporary connection loss". Before, we pretty much
 *              just gave up with the client if it lost the connection, and we accept a new one
 *              when the server loses connection because we explicitly closed the listening socket
 *              after the first client connected... This subsection is just to fit with the
 *              requirements given, in a proper way.
 *  -> Phase 4: So dealing with CNG, we went for AES-256-GCM, maybe an overkill... It was not the
 *              most pleasant part. Quite verbose, and it is just pretty much following the specific
 *              algo. For that, and also for portability reasons, I would surely move to a different
 *              library, but as I stated in the header module, this is not really meant to be in
 *              prod. I have also been handling some of the tidy warnings. Most of them were simple
 *              name conventions, then some modernizing parts (with locks, and casts mainly), and
 *              so on. I am still having some warnings, in the crypto part, but I require the
 *              const casts over there, and over here, in the main module, as the
 *              cognitive/cyclomatic complexity of the main function has increased with the
 *              argument parsing section by a while. So we are above the 25 max threshold for
 *              control flow. I mean, I could surely move that logic to a static inline helper,
 *              but it is not the main priority.
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

    peer.set_on_disconnected(
        []() { std::println(stderr, "[Net] Connection lost! Initiating recovery..."); });

    bool is_server = false;
    std::string psk;
    std::string target_ip = "127.0.0.1";  // We default to localhost.
    std::uint16_t target_port = 6767;     // Sorry.

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help")
        {
            std::println("zclip - Peer-to-Peer Clipboard Sync");
            std::println("Usage: zclip [OPTIONS]\n");
            std::println("Options:");
            std::println(
                "  --server          Run as the listening server. (Defaults to client mode)");
            std::println(
                "  --ip <address>    Target IP address (Client mode only). Default: 127.0.0.1");
            std::println("  --port <number>   TCP port to bind or connect to. Default: 6767");
            std::println("  --secret <psk>    Enable AES-256-GCM encryption using this password.");
            std::println("  -h, --help        Show this help message.\n");
            std::println("Examples:");
            std::println("  Server: zclip --server --port 7777 --secret \"hunter2\"");
            std::println("  Client: zclip --ip 192.168.1.50 --port 7777 --secret \"hunter2\"");
            return 0;
        }
        else if (arg == "--server")
        {
            is_server = true;
        }
        else if (arg == "--secret" && i + 1 < argc)
        {
            psk = argv[++i];  // We treat the next arg as the password.
        }
        else if (arg == "--ip" && i + 1 < argc)
        {
            target_ip = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            target_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
    }

    if (!psk.empty())
    {
        std::println("[Sec] AES-256-GCM Encryption ENABLED.");
        peer.set_psk(psk);
    }
    else
    {
        std::println("[Sec] WARNING: No --secret provided. Traffic is UNENCRYPTED.");
    }

    std::println("Sync engine active. Press Ctrl+C to terminate.");

    listener.start();

    while (true)
    {
        if (is_server)
        {
            std::println("[Net] Starting server on port {}...", target_port);
            if (peer.listen(target_port).has_value())
            {
                peer.wait();  // So we block the main thread until the connection drops...
            }
            else
            {
                std::println(stderr, "[Error] Server failed to bind to port {}.", target_port);
            }
        }
        else
        {
            std::println("[Net] Connecting to client on {}:{}...", target_ip, target_port);
            if (peer.connect(target_ip, target_port).has_value())
            {
                std::println("[Net] Connected to {}:{}", target_ip, target_port);
                peer.wait();  // Same here.
            }
            else
            {
                std::println(stderr, "[Error] Failed to connect to {}:{}.", target_ip, target_port);
            }
        }

        std::println("[Net] Retrying in 3 seconds...");
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    // Unreachable under normal Ctrl+C exit, but good practice!
    listener.stop();
}