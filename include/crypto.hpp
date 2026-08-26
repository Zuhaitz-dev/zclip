
#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/*
 * For encryption we are gonna use the Windows Cryptography API.
 * We are gonna implement AES-256-GCM authenticated encryption.
 * We could use other dependencies and so on, that could be discussed
 * in a different context, but I think for the objective of this task 
 * this is surely better.
 * 
 * It is gonna be verbose, though, so... Let's deal with it I am afraid.
*/
namespace zclip
{

/*
 * Hashes the PSK to 256 bits (SHA-256) and encrypts the payload using AES-256-GCM.
*/
[[nodiscard]] std::expected<std::vector<uint8_t>, std::string> encrypt_payload(
    std::string_view plaintext, std::string_view psk);

/*
 * Decrypts and authenticates an AES-256-GCM ciphertext payload.
*/
[[nodiscard]] std::expected<std::string, std::string> decrypt_payload(
    std::span<const uint8_t> ciphertext, std::string_view psk);

} // namespace zclip