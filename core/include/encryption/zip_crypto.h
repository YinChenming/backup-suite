//
// Created by ycm on 2025/12/28.
//

#ifndef BACKUPSUITE_ZIP_CRYPTO_H
#define BACKUPSUITE_ZIP_CRYPTO_H

#include <vector>
#include <cstdint>

#include "utils/crc.h"
#include "api.h"

namespace encryption
{
    using namespace crc;
    class BACKUP_SUITE_API ZipCrypto {
        uint32_t keys[3] = {
            0x12345678,
            0x23456789,
            0x34567890
        };

        // 内部核心更新函数
        void update_keys(uint8_t byte);
        // 生成当前伪随机字节
        uint8_t decrypt_byte() const;
    public:
        ZipCrypto() = default;
        explicit ZipCrypto(const std::vector<uint8_t>& keys) { init(keys); }
        ZipCrypto(const ZipCrypto& other)
        {
            keys[0] = other.keys[0];
            keys[1] = other.keys[1];
            keys[2] = other.keys[2];
        }
        ZipCrypto operator=(const ZipCrypto& other)
        {
            keys[0] = other.keys[0];
            keys[1] = other.keys[1];
            keys[2] = other.keys[2];
            return *this;
        }
        ZipCrypto(ZipCrypto&& other) noexcept
        {
            keys[0] = other.keys[0];
            keys[1] = other.keys[1];
            keys[2] = other.keys[2];
        }

        // 初始化 Key
        void init(const std::vector<uint8_t>& password);
        // 解密一个字节
        uint8_t decrypt(const uint8_t cipher_byte);
        // 加密一个字节
        uint8_t encrypt(const uint8_t plain_byte);
    };
}

#endif // BACKUPSUITE_ZIP_CRYPTO_H
