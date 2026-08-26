#undef NDEBUG

#include "net_peer.hpp"

#include <chrono>
#include <cstdlib>
#include <future>
#include <print>
#include <string>
#include <thread>

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

    // Shift to a more obscure dynamic port to avoid CI collisions
    const uint16_t test_port = 54321; 

    // 1. Start listening on the server
    auto listen_res = server.listen(test_port);
    REQUIRE(listen_res.has_value(), "Server failed to listen. Port might be in use.");

    // 2. Setup server callback
    std::promise<std::string> server_promise;
    auto server_future = server_promise.get_future();
    server.set_on_frame_received([&](const std::string& msg) {
        server_promise.set_value(msg);
    });

    // 3. Connect the client to the server
    auto connect_res = client.connect("127.0.0.1", test_port);
    REQUIRE(connect_res.has_value(), "Client failed to connect to server.");

    // Give the server's background accept() thread more time to wake up on slow CI
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // 4. Test Client -> Server transmission
    const std::string client_msg = "Hello from the client!";
    bool client_sent = client.send_payload(client_msg);
    REQUIRE(client_sent, "Client failed to send payload.");

    // Wait up to 3 seconds for the server to receive it
    auto server_status = server_future.wait_for(std::chrono::seconds(3));
    REQUIRE(server_status == std::future_status::ready, "Server timed out waiting for message.");
    REQUIRE(server_future.get() == client_msg, "Server received incorrect message.");
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
    REQUIRE(server_sent, "Server failed to send payload.");

    // Wait up to 3 seconds for the client to receive it
    auto client_status = client_future.wait_for(std::chrono::seconds(3));
    REQUIRE(client_status == std::future_status::ready, "Client timed out waiting for message.");
    REQUIRE(client_future.get() == server_msg, "Client received incorrect message.");
    std::println("  [OK] Server -> Client");

    // 7. Cleanup
    client.disconnect();
    server.disconnect();

    std::println("All NetPeer tests passed successfully!");
    return 0;
}