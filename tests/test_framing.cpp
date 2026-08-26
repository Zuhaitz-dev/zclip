
#undef NDEBUG

#include "framing.hpp"
#include <cassert>
#include <print>
#include <string>

int main()
{
    std::println("Running Wire Framing unit tests...");

    // Test 1: Round-trip encode/decode of simple string
    {
        const std::string text = "Hello, Wire Protocol!";
        auto encoded = zclip::encode_frame(text);
        assert(encoded.size() == zclip::HeaderSize + text.size());

        zclip::FrameParser parser;
        parser.append(encoded.data(), encoded.size());

        auto result = parser.pop_frame();
        assert(result.has_value());
        assert(*result == text);
        assert(parser.buffered_bytes() == 0);
    }

    // Test 2: Unicode and Emoji preservation
    {
        const std::string unicode_text = "Sync 🥹 🥸 Unicode Test 123";
        auto encoded = zclip::encode_frame(unicode_text);

        zclip::FrameParser parser;
        parser.append(encoded.data(), encoded.size());

        auto result = parser.pop_frame();
        assert(result.has_value());
        assert(*result == unicode_text);
    }

    // Test 3: TCP Chunking / Fragmentation Simulation
    {
        const std::string long_text = "Simulating TCP chunk splitting across multiple socket reads...";
        auto encoded = zclip::encode_frame(long_text);

        zclip::FrameParser parser;

        parser.append(encoded.data(), 3);
        assert(!parser.pop_frame().has_value());

        parser.append(encoded.data() + 3, 10);
        assert(!parser.pop_frame().has_value());

        parser.append(encoded.data() + 13, encoded.size() - 13);
        auto result = parser.pop_frame();
        assert(result.has_value());
        assert(*result == long_text);
    }

    // Test 4: Multiple frames packed into a single network buffer
    {
        auto frame1 = zclip::encode_frame("First Packet");
        auto frame2 = zclip::encode_frame("Second Packet");

        zclip::FrameParser parser;
        parser.append(frame1.data(), frame1.size());
        parser.append(frame2.data(), frame2.size());

        auto res1 = parser.pop_frame();
        assert(res1.has_value() && *res1 == "First Packet");

        auto res2 = parser.pop_frame();
        assert(res2.has_value() && *res2 == "Second Packet");

        assert(!parser.pop_frame().has_value());
    }

    // Test 5: Empty payload frame
    {
        auto empty_frame = zclip::encode_frame("");
        zclip::FrameParser parser;
        parser.append(empty_frame.data(), empty_frame.size());

        auto res = parser.pop_frame();
        assert(res.has_value() && res->empty());
    }

    std::println("All Wire Framing tests passed successfully!");
}