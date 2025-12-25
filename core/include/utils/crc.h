//
// Created by ycm on 2025/12/25.
//

#ifndef BACKUPSUITE_CRC32_H
#define BACKUPSUITE_CRC32_H

#include <cstdint>

namespace crc
{
    uint32_t crc32(const char* data, size_t length);
}

#endif // BACKUPSUITE_CRC32_H
