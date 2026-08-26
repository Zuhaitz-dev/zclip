
#include "crypto.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>

#include <memory>

#pragma comment(lib, "bcrypt.lib")

/*
 * This is going to be extremely secure, to the program,
 * not to me, I am cooked.
 * 
 * To make this clear, I am obviously checking external sources,
 * I haven't had to deal with CNG that many times before.
*/

namespace zclip
{

namespace
{
    
// So helpers to safely close CNG handles.
struct BCryptHandleDeleter 
{
    void operator()(void* handle) const 
    {
        if (handle)
        {
            ::BCryptCloseAlgorithmProvider(handle, 0);
        }
    }
};
struct BCryptKeyDeleter 
{
    void operator()(void* handle) const 
    {
        if (handle) 
        {
            ::BCryptDestroyKey(handle);
        }
    }
};

using AlgHandle = std::unique_ptr<void, BCryptHandleDeleter>;
using KeyHandle = std::unique_ptr<void, BCryptKeyDeleter>;

// This hashes a variable length PSK into 32 bytes for AES-256...
std::expected<std::vector<uint8_t>, std::string> derive_key(std::string_view psk)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (::BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
    {
        return std::unexpected("Failed to open SHA256 provider.");
    }
    AlgHandle alg(hAlg);

    std::vector<uint8_t> hash(32, 0);
    if (::BCryptHash(alg.get(), nullptr, 0,
                     reinterpret_cast<PUCHAR>(const_cast<char*>(psk.data())),
                     static_cast<ULONG>(psk.size()), hash.data(), static_cast<ULONG>(hash.size())) < 0)
    {
        return std::unexpected("Failed to hash PSK.");
    }
    return hash;
}

} // namespace

std::expected<std::vector<uint8_t>, std::string> encrypt_payload(std::string_view plaintext, std::string_view psk)
{
    auto key_bytes = derive_key(psk);
    if (!key_bytes.has_value()) 
    {
        return std::unexpected(key_bytes.error());
    }

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (::BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0)
    {
        return std::unexpected("Failed to open AES provider.");
    }
    
    AlgHandle alg(hAlg);

    if (::BCryptSetProperty(alg.get(), BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0) < 0)
    {
        return std::unexpected("Failed to set GCM mode.");
    }

    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (::BCryptGenerateSymmetricKey(alg.get(), &hKey, nullptr, 0, key_bytes->data(), static_cast<ULONG>(key_bytes->size()), 0) < 0)
    {
        return std::unexpected("Failed to generate symmetric key.");
    }
    
    KeyHandle key(hKey);

    /*
     * Generate 12-byte IV (nonce)...
    */
    std::vector<uint8_t> iv(12, 0);
    ::BCryptGenRandom(nullptr, iv.data(), static_cast<ULONG>(iv.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = iv.data();
    authInfo.cbNonce = static_cast<ULONG>(iv.size());
    
    std::vector<uint8_t> tag(16, 0);
    authInfo.pbTag = tag.data();
    authInfo.cbTag = static_cast<ULONG>(tag.size());

    ULONG cipherLen = 0;
    if (::BCryptEncrypt(key.get(), reinterpret_cast<PUCHAR>(const_cast<char*>(plaintext.data())), static_cast<ULONG>(plaintext.size()),
                        &authInfo, iv.data(), static_cast<ULONG>(iv.size()), nullptr, 0, &cipherLen, 0) < 0)
    {
        return std::unexpected("Failed to get ciphertext length.");
    }

    std::vector<uint8_t> ciphertext(cipherLen, 0);
    if (::BCryptEncrypt(key.get(), reinterpret_cast<PUCHAR>(const_cast<char*>(plaintext.data())), static_cast<ULONG>(plaintext.size()),
                        &authInfo, iv.data(), static_cast<ULONG>(iv.size()), ciphertext.data(), static_cast<ULONG>(ciphertext.size()), &cipherLen, 0) < 0)
    {
        return std::unexpected("Failed to encrypt data.");
    }

    /* 
     * The layout of the output should look like:
     * [IV (12 bytes)] + [Ciphertext] + [MAC Tag (16 bytes)]
    */
    std::vector<uint8_t> final_payload;
    final_payload.reserve(iv.size() + ciphertext.size() + tag.size());
    final_payload.insert(final_payload.end(), iv.begin(), iv.end());
    final_payload.insert(final_payload.end(), ciphertext.begin(), ciphertext.end());
    final_payload.insert(final_payload.end(), tag.begin(), tag.end());

    return final_payload;
}

std::expected<std::string, std::string> decrypt_payload(std::span<const uint8_t> payload, std::string_view psk)
{
    /*
     * 12 here is for IV and 16 for the MAC Tag...
    */
    if (payload.size() < 12 + 16) 
    {
        return std::unexpected("Payload too small (missing IV or Tag).");
    }

    auto key_bytes = derive_key(psk);
    if (!key_bytes.has_value()) 
    {
        return std::unexpected(key_bytes.error());
    }

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (::BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0)
    {
        return std::unexpected("Failed to open AES provider.");
    }
    
    AlgHandle alg(hAlg);

    if (::BCryptSetProperty(alg.get(), BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0) < 0)
    {
        return std::unexpected("Failed to set GCM mode.");
    }

    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (::BCryptGenerateSymmetricKey(alg.get(), &hKey, nullptr, 0, key_bytes->data(), static_cast<ULONG>(key_bytes->size()), 0) < 0)
    {
        return std::unexpected("Failed to generate symmetric key.");
    }
    
    KeyHandle key(hKey);

    /*
     * The extract layout be like:
     * [IV (12 bytes)] + [Ciphertext] + [MAC Tag (16 bytes)]
    */
    std::vector<uint8_t> iv(payload.begin(), payload.begin() + 12);
    std::vector<uint8_t> tag(payload.end() - 16, payload.end());
    std::span<const uint8_t> ciphertext = payload.subspan(12, payload.size() - 28);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = iv.data();
    authInfo.cbNonce = static_cast<ULONG>(iv.size());
    authInfo.pbTag = tag.data();
    authInfo.cbTag = static_cast<ULONG>(tag.size());

    ULONG plainLen = 0;
    if (::BCryptDecrypt(key.get(), reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(ciphertext.data())), static_cast<ULONG>(ciphertext.size()),
                        &authInfo, iv.data(), static_cast<ULONG>(iv.size()), nullptr, 0, &plainLen, 0) < 0)
    {
        return std::unexpected("Failed to authenticate or get plaintext length. (Wrong PSK or tampered data)");
    }

    std::string plaintext(plainLen, '\0');
    if (::BCryptDecrypt(key.get(), reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(ciphertext.data())), static_cast<ULONG>(ciphertext.size()),
                        &authInfo, iv.data(), static_cast<ULONG>(iv.size()), reinterpret_cast<PUCHAR>(plaintext.data()), static_cast<ULONG>(plaintext.size()), &plainLen, 0) < 0)
    {
        return std::unexpected("Failed to decrypt data.");
    }

    return plaintext;
}

} // namespace zclip