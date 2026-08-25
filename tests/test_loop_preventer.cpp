#include "loop_preventer.hpp"
#include <cassert>
#include <print>

int main()
{
    std::println("Running LoopPreventer unit tests...");

    zclip::LoopPreventer preventer;

    // Test 1: First-time string passes
    assert(preventer.test_and_record(L"hello world") == true);

    // Test 2: Immediate duplicate is dropped
    assert(preventer.test_and_record(L"hello world") == false);

    // Test 3: New unique string passes
    assert(preventer.test_and_record(L"different text") == true);

    // Test 4: Empty string is dropped
    assert(preventer.test_and_record(L"") == false);

    // Test 5: Remote suppression flag drops all events
    preventer.set_remote_suppression(true);
    assert(preventer.test_and_record(L"brand new text") == false);
    preventer.set_remote_suppression(false);

    // Test 6: Remote pre-seeding suppresses subsequent local capture
    preventer.record_remote(L"synced from peer");
    assert(preventer.test_and_record(L"synced from peer") == false);

    std::println("All LoopPreventer tests passed successfully!");
}