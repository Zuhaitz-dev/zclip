/*
 * TODO: add proper headers later...
 */

#pragma once

/*
 * Notes related to these in 'clipboard_guard.hpp'.
 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <atomic>
#include <functional>
#include <semaphore>
#include <string>
#include <syncstream>
#include <thread>

namespace zclip
{

/*
 * clang-tidy warned here because we were copying and that added some overhead...
 * Thanks, clang-tidy.
 */
using ClipboardCallback = std::function<void(const std::wstring& text)>;

class ClipboardListener
{
public:
    explicit ClipboardListener(ClipboardCallback on_change);
    ~ClipboardListener();

    /*
     * Rule of Five.
     */
    ClipboardListener(const ClipboardListener&) = delete;
    ClipboardListener& operator=(const ClipboardListener&) = delete;
    ClipboardListener(ClipboardListener&&) noexcept = delete;
    ClipboardListener& operator=(ClipboardListener&&) noexcept = delete;

    void start();
    void stop();

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    void message_loop();

    ClipboardCallback m_callback;
    std::thread m_worker_thread;
    std::atomic<bool> m_running{false};
    std::atomic<HWND> m_hwnd{nullptr};
    std::atomic<DWORD> m_thread_id{0};

    /*
     * So we are gonna use a binary semaphore to ensure the message loop and queue are initialized
     * before 'start()' returns... This way we can prevent the possible race condition.
     */
    std::binary_semaphore m_ready_signal{0};
};

}  // namespace zclip