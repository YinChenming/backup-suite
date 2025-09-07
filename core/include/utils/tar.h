//
// Created by ycm on 25-9-5.
//

#ifndef TAR_H
#define TAR_H
#include <istream>

struct TarFileHeader
{
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type_flag;
    char link_name[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char device_major[8];
    char device_minor[8];
    char prefix[155];
    char padding[12];
};

#endif // TAR_H
