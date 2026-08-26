
#undef NDEBUG

#include "net_peer.hpp"

#include <cassert>
#include <chrono>
#include <future>
#include <print>
#include <string>
#include <thread>

int main()
{
    std::println("Running NetPeer unit tests...");

    zclip::TcpPeer server;
    zclip::TcpPeer client;

    const uint16_t test_port = 27777;

    // 1. Start listening on the server
    auto listen_res = server.listen(test_port);
    assert(listen_res.has_value());

    // 2. Setup server callback to fulfill a promise when data arrives
    std::promise<std::string> server_promise;
    auto server_future = server_promise.get_future();
    server.set_on_frame_received([&](const std::string& msg) {
        server_promise.set_value(msg);
    });

    // 3. Connect the client to the server
    auto connect_res = client.connect("127.0.0.1", test_port);
    assert(connect_res.has_value());

    // Give the server's background accept() thread a moment to swap to the client socket
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 4. Test Client -> Server transmission
    const std::string client_msg = "Hello from the client!";
    bool client_sent = client.send_payload(client_msg);
    assert(client_sent);

    // Wait up to 2 seconds for the server to receive it
    auto server_status = server_future.wait_for(std::chrono::seconds(2));
    assert(server_status == std::future_status::ready);
    assert(server_future.get() == client_msg);
    std::println("  [OK] Client -> Server");

    // 5. Setup client callback
    std::promise<std::string> client_promise;
    auto client_future = client_promise.get_future();
    client.set_on_frame_received([&](const std::string& msg) {
        client_promise.set_value(msg);
    });

    // 6. Test Server -> Client transmission
    const std::string server_msg = "Acknowledged by server!";
    bool server_sent = server.send_payload(server_msg);
    assert(server_sent);

    // Wait up to 2 seconds for the client to receive it
    auto client_status = client_future.wait_for(std::chrono::seconds(2));
    assert(client_status == std::future_status::ready);
    assert(client_future.get() == server_msg);
    std::println("  [OK] Server -> Client");

    // 7. Cleanup
    client.disconnect();
    server.disconnect();

    std::println("All NetPeer tests passed successfully!");
}