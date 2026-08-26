#undef NDEBUG

#include "crypto.hpp"

#include <cassert>
#include <print>
#include <string>

int main()
{
    std::println("Running Crypto unit tests...");

    // Test 1: Round-trip simple ASCII payload.
    {
        const std::string text = "Hello, secure clipboard!";
        const std::string psk = "hunter2";

        auto encrypted = zclip::encrypt_payload(text, psk);
        assert(encrypted.has_value());
        assert(encrypted->size() == 12 + text.size() + 16);  // IV + ciphertext + tag.

        auto decrypted = zclip::decrypt_payload(*encrypted, psk);
        assert(decrypted.has_value());
        assert(*decrypted == text);
    }

    // Test 2: Round-trip Unicode (UTF-8) payload with emoji and CJK.
    {
        const std::string text = "Synchronizing 🥹 Unicode émojis → 日本語 🎉";
        const std::string psk = "unicode-secret";

        auto encrypted = zclip::encrypt_payload(text, psk);
        assert(encrypted.has_value());

        auto decrypted = zclip::decrypt_payload(*encrypted, psk);
        assert(decrypted.has_value());
        assert(*decrypted == text);
    }

    // Test 3: Random IV means the same plaintext+PSK yields different ciphertexts.
    {
        const std::string text = "repeat me";
        const std::string psk = "same-secret";

        auto first = zclip::encrypt_payload(text, psk);
        auto second = zclip::encrypt_payload(text, psk);
        assert(first.has_value() && second.has_value());
        assert(*first != *second);
    }

    // Test 4: Wrong PSK is rejected by GCM authentication.
    {
        const std::string text = "secret message";
        const std::string psk = "correct-password";
        const std::string wrong_psk = "wrong-password";

        auto encrypted = zclip::encrypt_payload(text, psk);
        assert(encrypted.has_value());

        auto decrypted = zclip::decrypt_payload(*encrypted, wrong_psk);
        assert(!decrypted.has_value());
    }

    // Test 5: Tampered ciphertext is rejected.
    {
        const std::string text = "don't touch my payload";
        const std::string psk = "secret";

        auto encrypted = zclip::encrypt_payload(text, psk);
        assert(encrypted.has_value());

        (*encrypted)[12] ^= 0xFF;  // Flip one byte in the ciphertext body.

        auto decrypted = zclip::decrypt_payload(*encrypted, psk);
        assert(!decrypted.has_value());
    }

    // Test 6: Payload too short to hold IV + tag is rejected.
    {
        const std::string psk = "secret";

        auto encrypted = zclip::encrypt_payload("abc", psk);
        assert(encrypted.has_value());

        auto truncated =
            zclip::decrypt_payload(std::span<const uint8_t>(encrypted->data(), 10), psk);
        assert(!truncated.has_value());
    }

    std::println("All Crypto tests passed successfully!");
}