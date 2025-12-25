//
// Created by ycm on 2025/12/26.
//
#include "filesystem/entities.h"

// copied from Windows API <winnt.h>
#define FILE_ATTRIBUTE_READONLY 0x00000001
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_REPARSE_POINT 0x00000400

void update_file_entity_meta(FileEntityMeta& meta)
{
    // from windows attributes to posix mode
    if (!meta.posix_mode)
    {
        if (meta.windows_attributes & FILE_ATTRIBUTE_READONLY)
        {
            meta.posix_mode = 0444; // 只读权限
        } else
        {
            meta.posix_mode = 0777; // 读写权限
        }
    }
    // from posix mode to windows attributes
    if (meta.posix_mode & 0222) // 可写
        meta.windows_attributes &= ~FILE_ATTRIBUTE_READONLY;
    else
        meta.windows_attributes |= FILE_ATTRIBUTE_READONLY;
    if (meta.type == FileEntityType::Directory)
        meta.windows_attributes |= FILE_ATTRIBUTE_DIRECTORY;
    else
        meta.windows_attributes &= ~FILE_ATTRIBUTE_DIRECTORY;
    if (meta.type == FileEntityType::SymbolicLink)
        meta.windows_attributes |= FILE_ATTRIBUTE_REPARSE_POINT;
    else
        meta.windows_attributes &= ~FILE_ATTRIBUTE_REPARSE_POINT;
}
