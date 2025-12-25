//
// Created by ycm on 2025/12/25.
//

#ifndef BACKUPSUITE_FS_DELETER_H
#define BACKUPSUITE_FS_DELETER_H

namespace fs_deleter
{
    template <typename T>
    struct BACKUP_SUITE_API FStreamDeleter
    {
        virtual ~FStreamDeleter() = default;
        using pointer = T*;

        [[nodiscard]] virtual bool need_delete() const
        {
            return true;
        }

        void operator()(T* stream) const
        {
            if (stream && stream->is_open())
            {
                stream->close();
            }
            if (need_delete())
                delete stream;
        }
    };
    using IFStreamPointer = std::unique_ptr<std::ifstream, FStreamDeleter<std::ifstream>>;
    using OFStreamPointer = std::unique_ptr<std::ofstream, FStreamDeleter<std::ofstream>>;
}

#endif // BACKUPSUITE_FS_DELETER_H
