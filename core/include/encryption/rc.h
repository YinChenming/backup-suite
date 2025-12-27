//
// Created by ycm on 2025/12/28.
//

#ifndef BACKUPSUITE_RC_H
#define BACKUPSUITE_RC_H
#include <vector>
#include <cstdint>
#include <algorithm>

#include "api.h"

namespace encryption
{
    class BACKUP_SUITE_API RC4 {
        uint8_t S[256]{};
        int i_ = 0;
        int j_ = 0;

    public:
        // 1. KSA：密钥调度算法，初始化 S 盒
        explicit RC4(const std::vector<uint8_t>& key);

        // 2. PRGA：伪随机生成算法，加密/解密一个字节
        uint8_t process_byte(uint8_t input);
        uint8_t encrypt(uint8_t);
        uint8_t decrypt(uint8_t);
        void process(uint8_t* data, size_t len);
    };
}
#endif // BACKUPSUITE_RC_H
