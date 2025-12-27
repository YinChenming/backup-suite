//
// Created by ycm on 2025/10/16.
//

#ifndef BACKUPSUITE_HUFFMAN_H
#define BACKUPSUITE_HUFFMAN_H

#include <vector>
#include <cstdint>

#include "utils/streams.h"

namespace compress::huffman
{
    template<class symType, size_t kNumBitsMax, size_t kNumTableBits, size_t m_NumSymbols>
class Decoder
{
    uint32_t limits_[kNumBitsMax + 2 - kNumTableBits] = {};  // limits_[i] is the largest code with i + kNumTableBits bits
    // poses_[i] 表示长度为 i + kNumTableBits 的符号在 symbol_ 中的起始位置
    uint32_t poses_[kNumBitsMax - kNumTableBits] = {};   // poses_[i]
    // symbol_[i] 存对应位置的符号,包括2^kNumTableBits的快速查找表和其他剩余的符号
    symType symbol_[(1 << m_NumSymbols) + m_NumSymbols - (kNumTableBits + 1)] = {};
    uint16_t lens_[(1<<kNumTableBits) + sizeof(symType) == 1 ? 16 : 0] = {}; // lens_[i] 存对应位置的符号长度,只存储长度不超过kNumTableBits的符号
public:
    Decoder() {}
    explicit Decoder(const std::vector<int> &lens)
    {
        build(lens);
    }
    Decoder(const int lens[], const size_t len): Decoder(std::vector(lens, lens+len)){ }
    bool build(const int lens[], const size_t len)
    {
        return build(std::vector(lens, lens+len));
    }
    bool build(const std::vector<int> &lens)
    {
        std::vector<size_t> counts(kNumBitsMax + 1, 0);
        size_t i;
        for (i=0; i< m_NumSymbols; i++)
            ++counts[lens[i]];

        uint32_t sum = 0;
        for (i = 1; i <= kNumTableBits; i++)
        {
            sum <<= 1;
            sum += counts[i];
        }
        // sum 表示长度为i的前缀码所表示的符号在symbol_中的起始位置,在i<=kNumTableBits时
        // 会直接以前缀码作为索引(快速查找表);i>kNumTableBits时只会存储单个符号
        uint32_t startPos = sum;
        // startPos 表示长度为i的前缀码的起始位置
        limits_[0] = startPos;

        for (i = kNumTableBits + 1; i <= kNumBitsMax; i++)
        {
            startPos <<= 1;
            poses_[i - (kNumTableBits + 1)] = startPos - sum;
            const unsigned cnt = counts[i];
            counts[i] = sum;
            sum += cnt;
            startPos += cnt;
            // 转成MSB表示
            limits_[i - kNumTableBits] = startPos << (kNumBitsMax - i);
        }

        limits_[kNumBitsMax + 1 - kNumTableBits] = 1<<kNumBitsMax;

        // 在 7Zip 中,后面会对符号表做一些检查,这里先省略了
        // TODO: 实现简单的检查

        // 复用sum
        sum = 0;
        // 构建快速查找表
        for (i = 1; i <= kNumTableBits; i++)
        {
            // 转成MSB表示
            const int cnt = counts[i] << (kNumTableBits - i);
            counts[i] = static_cast<unsigned>(sum) >> (kNumTableBits - i);
            memset(lens_ + sum, i, cnt);
            sum += cnt;
        }
        uint32_t v = 0;
        // 构建慢速查找表
        for (i = 0; i < m_NumSymbols; i++, v += 1 + (1 << sizeof(symType) * 8 * 1))
        {
            const unsigned len = lens[i];
            // i没出现,跳过编码
            if (!len)
                continue;
            // offset即i的前缀码在symbol_中的位置
            const size_t offset = counts[len]++;
            if (len >= kNumTableBits)
                symbol_[offset] = static_cast<symType>(i);
            else
            {
                // 此处 7Zip 做了多字节填充优化,这里先省略了
                const size_t numCodes = 1 << (kNumTableBits - len);
                const size_t startIdx = offset << (kNumTableBits - len);
                for (size_t j = 0; j < numCodes; j++)
                    symbol_[startIdx + j] = static_cast<symType>(i);
            }
        }
        return true;
    }
    symType decode(ibstream& ibs)
    {
        // 先执行快速查找
        uint32_t peekedShort = ibs.read(kNumTableBits), peekedLong;
        if (peekedShort < limits_[0])
        {
            ibs.unget(kNumTableBits - lens_[peekedShort]);
            return symbol_[peekedShort];
        } else
        {
            peekedLong = (peekedShort << (kNumBitsMax - kNumTableBits)) | ibs.read(kNumBitsMax - kNumTableBits);
            size_t numBits = kNumTableBits + 1;
            while (numBits <= kNumBitsMax && peekedLong >= limits_[numBits - kNumTableBits])
                ++numBits;
            size_t symbolIdx = (peekedLong >> (kNumTableBits - numBits)) - poses_[numBits - (kNumTableBits + 1)];
            ibs.unget(kNumBitsMax - numBits);
            return symbol_[symbolIdx];
        }
    }
};
}

#endif // BACKUPSUITE_HUFFMAN_H
