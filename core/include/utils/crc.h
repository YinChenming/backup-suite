//
// Created by ycm on 2025/12/25.
//

#ifndef BACKUPSUITE_CRC32_H
#define BACKUPSUITE_CRC32_H

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "api.h"

namespace crc
{
    BACKUP_SUITE_API uint32_t crc32(const char* data, size_t length);
    /*
     * for Zip Encrypto
     */
    BACKUP_SUITE_API uint32_t crc32_update(uint32_t crc, uint8_t data);
    class BACKUP_SUITE_API CRC32
    {
        uint32_t crc32_ = 0xFFFFFFFF;
    public:
        CRC32() = default;
        void update(uint8_t data);
        uint32_t finalize() const;
        template<typename T>
        std::enable_if_t<(std::is_integral_v<T> && sizeof(T)==1) || std::is_same_v<T, std::byte>> update(const T* data, const size_t length)
        {
            for (size_t i = 0; i < length; i++) update(static_cast<uint8_t>(data[i]));
        }
    };
}

#endif // BACKUPSUITE_CRC32_H
