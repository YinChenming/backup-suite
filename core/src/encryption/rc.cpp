//
// Created by ycm on 2025/12/28.
//
#include "encryption/rc.h"
using namespace encryption;

RC4::RC4(const std::vector<uint8_t>& key)  {
    for (int i = 0; i < 256; ++i) {
        S[i] = static_cast<uint8_t>(i);
    }
    int j = 0;
    for (int i = 0; i < 256; ++i) {
        j = (j + S[i] + static_cast<uint8_t>(key[i % key.size()])) % 256;
        std::swap(S[i], S[j]);
    }
    // 重置 PRGA 指针
    i_ = 0;
    j_ = 0;
}
uint8_t RC4::encrypt(const uint8_t plain_byte) { return process_byte(plain_byte); }
uint8_t RC4::decrypt(const uint8_t plain_byte) { return process_byte(plain_byte); }
uint8_t RC4::process_byte(const uint8_t input) {
    i_ = (i_ + 1) % 256;
    j_ = (j_ + S[i_]) % 256;
    std::swap(S[i_], S[j_]);

    // 生成密钥流字节 K
    const uint8_t k = S[(S[i_] + S[j_]) % 256];

    // 异或运算完成加/解密
    return input ^ k;
}
void RC4::process(uint8_t* data, const size_t len) {
    for (size_t k = 0; k < len; ++k) {
        data[k] = process_byte(data[k]);
    }
}
