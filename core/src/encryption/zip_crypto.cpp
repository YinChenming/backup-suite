//
// Created by ycm on 2025/12/28.
//
#include "encryption/zip_crypto.h"
using namespace encryption;

void ZipCrypto::update_keys(const uint8_t byte) {
    keys[0] = crc32_update(keys[0], byte);
    keys[1] = (keys[1] + (keys[0] & 0xFF)) * 0x08088405 + 1;
    keys[2] = crc32_update(keys[2], static_cast<uint8_t>(keys[1] >> 24));
}
uint8_t ZipCrypto::decrypt_byte() const {
    const uint16_t temp = keys[2] | 2;
    return static_cast<uint8_t>((temp * (temp ^ 1)) >> 8);
}
void ZipCrypto::init(const std::vector<uint8_t>& password) {
    for (const uint8_t c : password) {
        update_keys(c);
    }
}
uint8_t ZipCrypto::decrypt(const uint8_t cipher_byte) {
    const uint8_t plain_byte = cipher_byte ^ decrypt_byte();
    update_keys(plain_byte); // 注意：解密时，更新 key 使用的是解密后的明文
    return plain_byte;
}
uint8_t ZipCrypto::encrypt(const uint8_t plain_byte) {
    const uint8_t cipher_byte = plain_byte ^ decrypt_byte();
    update_keys(plain_byte); // 加密时，更新 key 使用的也是明文
    return cipher_byte;
}
