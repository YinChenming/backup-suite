//
// Created by ycm on 2025/12/28.
//
#include <gtest/gtest.h>

#include "encryption/rc.h"
#include "encryption/zip_crypto.h"

TEST(encrypt, ZipCrypto)
{
    using namespace encryption;
    const std::string password = "password";
    const std::vector<uint8_t> key_vec(password.begin(), password.end());
    ZipCrypto zip_crypto_encrypt(key_vec);
    ZipCrypto zip_crypto_decrypt(key_vec);

    const std::string plaintext = "Zip Crypto Encryption Test Data";
    std::vector<uint8_t> ciphertext;
    ciphertext.reserve(plaintext.size());

    // 加密
    for (const auto& ch : plaintext) {
        const uint8_t cipher_byte = zip_crypto_encrypt.encrypt(static_cast<uint8_t>(ch));
        ciphertext.push_back(cipher_byte);
    }

    // 解密
    std::string decrypted_text;
    decrypted_text.reserve(ciphertext.size());
    for (const auto& cipher_byte : ciphertext) {
        const uint8_t plain_byte = zip_crypto_decrypt.decrypt(cipher_byte);
        decrypted_text.push_back(static_cast<char>(plain_byte));
    }

    EXPECT_EQ(plaintext, decrypted_text);
}
TEST(encrypt, RC4)
{
    using namespace encryption;
    const std::string password = "securekey";
    const std::vector<uint8_t> key_vec(password.begin(), password.end());
    RC4 rc4_encrypt(key_vec);
    RC4 rc4_decrypt(key_vec);
    const std::string plaintext = "RC4 Encryption Test Data";
    std::vector<uint8_t> ciphertext;
    ciphertext.reserve(plaintext.size());
    // 加密
    for (const auto& ch : plaintext) {
        const uint8_t cipher_byte = rc4_encrypt.encrypt(static_cast<uint8_t>(ch));
        ciphertext.push_back(cipher_byte);
    }
    // 解密
    std::string decrypted_text;
    decrypted_text.reserve(ciphertext.size());
    for (const auto& cipher_byte : ciphertext) {
        const uint8_t plain_byte = rc4_decrypt.decrypt(cipher_byte);
        decrypted_text.push_back(static_cast<char>(plain_byte));
    }
    EXPECT_EQ(plaintext, decrypted_text);
}