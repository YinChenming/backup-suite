//
// Created by ycm on 2025/12/25.
//

#ifndef BACKUPSUITE_CRC32_H
#define BACKUPSUITE_CRC32_H

#include <cstdint>

#include "api.h"

namespace crc
{
    BACKUP_SUITE_API uint32_t crc32(const char* data, size_t length);
    /*
     * for Zip Encrypto
     */
    BACKUP_SUITE_API uint32_t crc32_update(uint32_t crc, uint8_t data);
}

#endif // BACKUPSUITE_CRC32_H
