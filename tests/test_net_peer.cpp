/*
 * This test is a bit annoying, but it's okay, I am having fun.
*/

#undef NDEBUG

#include "net_peer.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <print>
#include <string>
#include <thread>
#include <mutex>

#define REQUIRE(cond, msg) \
    do { \
        if (!(cond)) { \
            std::println(stderr, "[FATAL] {} (Line {})", msg, __LINE__); \
            std::exit(1); \
        } \
    } while(false)

int main()
{
    std::println("Running NetPeer unit tests...");

    zclip::TcpPeer server;
    zclip::TcpPeer client;

    const uint16_t test_port = 54321; 

    // 1. Start Server
    REQUIRE(server.listen(test_port).has_value(), "Server failed to listen.");

    // 2. Setup Server Callback (Crash-proof)
    std::atomic<bool> server_received_flag{false};
    std::string server_msg_received;
    std::mutex server_mtx;
    
    server.set_on_frame_received([&](const std::string& msg) {
        std::lock_guard<std::mutex> lock(server_mtx);
        server_msg_received = msg;
        server_received_flag.store(true);
    });

    // 3. Start Client
    REQUIRE(client.connect("127.0.0.1", test_port).has_value(), "Client connect failed.");
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // 4. Test Client -> Server
    const std::string expected_server_msg = "Hello from the client!";
    REQUIRE(client.send_payload(expected_server_msg), "Client send failed.");

    // Spin-wait for up to 3 seconds
    for (int i{}; i < 30 && !server_received_flag.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    REQUIRE(server_received_flag.load(), "Server timed out.");
    REQUIRE(server_msg_received == expected_server_msg, "Server got wrong message.");
    std::println("  [OK] Client -> Server");

    // 5. Setup Client Callback
    std::atomic<bool> client_received_flag{false};
    std::string client_msg_received;
    std::mutex client_mtx;

    client.set_on_frame_received([&](const std::string& msg) {
        std::lock_guard<std::mutex> lock(client_mtx);
        client_msg_received = msg;
        client_received_flag.store(true);
    });

    // 6. Test Server -> Client
    const std::string expected_client_msg = "Acknowledged by server!";
    REQUIRE(server.send_payload(expected_client_msg), "Server send failed.");

    for (int i{}; i < 30 && !client_received_flag.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    REQUIRE(client_received_flag.load(), "Client timed out.");
    REQUIRE(client_msg_received == expected_client_msg, "Client got wrong message.");
    std::println("  [OK] Server -> Client");

    // 7. Cleanup
    client.disconnect();
    server.disconnect();
    
    std::println("All NetPeer tests passed successfully!");
    return 0;
}