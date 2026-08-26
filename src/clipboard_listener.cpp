
#include "clipboard_listener.hpp"
#include "clipboard_guard.hpp"

namespace zclip
{

namespace
{
/*
 * Custom window class for the message sink...
 */
constexpr const wchar_t* WindowClassName = L"ZClip_MessageSinkWindow";
}  // namespace

ClipboardListener::ClipboardListener(ClipboardCallback on_change) : m_callback(std::move(on_change))
{
}

ClipboardListener::~ClipboardListener()
{
    stop();
}

void ClipboardListener::start()
{
    if (m_running.exchange(true))
    {
        return;
    }

    m_worker_thread = std::thread([this]() { message_loop(); });

    /*
     * So we block until the message loop is properly initialized and listening.
     * This way we prevent that m_hwnd and m_thread_id are in invalid states.
     */
    m_ready_signal.acquire();
}

void ClipboardListener::stop()
{
    if (!m_running.exchange(false))
    {
        return;
    }

    const HWND hwnd = m_hwnd.load();

    if (nullptr != hwnd)
    {
        ::PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }

    if (m_worker_thread.joinable())
    {
        m_worker_thread.join();
    }

    m_hwnd.store(nullptr);
    m_thread_id.store(0);
}

LRESULT CALLBACK ClipboardListener::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (WM_NCCREATE == msg)
    {
        /*
         * We extract 'this' pointer passed via 'CreateWindowEx lpParam'.
         */
        auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                            reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
        return ::DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    auto* listener = reinterpret_cast<ClipboardListener*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (WM_CLIPBOARDUPDATE == msg && nullptr != listener)
    {
        /*
         * Notification received from Windows subsystem...
         */
        auto result = read_text_utf16();
        if (result.has_value() && listener->m_callback)
        {
            listener->m_callback(*result);
        }

        return 0;
    }

    if (WM_CLOSE == msg)
    {
        ::DestroyWindow(hwnd);
        return 0;
    }

    if (WM_DESTROY == msg)
    {
        ::PostQuitMessage(0);
        return 0;
    }

    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

void ClipboardListener::message_loop()
{
    m_thread_id.store(::GetCurrentThreadId());
    HINSTANCE h_inst = ::GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);

    /*
     * Long Pointer to the Windows Procedure function.
     * I __love__ Windows and their APIs...
     */
    wc.lpfnWndProc = window_proc;
    wc.hInstance = h_inst;
    wc.lpszClassName = WindowClassName;

    ::RegisterClassExW(&wc);

    HWND hwnd = ::CreateWindowExW(0,                // DWORD        dwExStyle
                                  WindowClassName,  // LPCWSTR      lpClassName
                                  L"ZClipMsgSink",  // LPCWSTR      lpWindowName
                                  0,                // DWORD        dwStyle
                                  0,                // int          X
                                  0,                // int          Y
                                  0,                // int          nWidth
                                  0,                // int          nHeight
                                  HWND_MESSAGE,     // HWND         hWndParent
                                  nullptr,          // HMENU        hMenu
                                  h_inst,           // HInstance    hInstance
                                  this              // LPVOID       lpParam
    );

    m_hwnd.store(hwnd);

    if (nullptr != hwnd)
    {
        ::AddClipboardFormatListener(hwnd);
    }

    /*
     * Window and queue are done so we can unblock 'start()'.
     */
    m_ready_signal.release();

    /*
     * GetMessageW blocks until an OS event arrives,
     * so we are not busy waiting.
     */
    MSG msg;
    while (::GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }

    if (nullptr != hwnd)
    {
        ::RemoveClipboardFormatListener(hwnd);
    }

    ::UnregisterClassW(WindowClassName, h_inst);
}

}  // namespace zclip