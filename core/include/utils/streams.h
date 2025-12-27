//
// Created by ycm on 2025/10/2.
//

#ifndef BACKUPSUITE_BSTREAM_H
#define BACKUPSUITE_BSTREAM_H

#include <iostream>
#include <filesystem/entities.h>

#include "api.h"

class BACKUP_SUITE_API ibstream
{
    std::istream& is_;
    uint8_t buffer_ = 0;
    int bits_left_ = 0;
public:
    explicit ibstream(std::istream& is): is_(is)
    {
        flush();
    }
    ~ibstream() = default;
    void flush()
    {
        buffer_ = is_.good() ? is_.get() : 0;
        bits_left_ = 0;
    }
    bool read_bit()
    {
        if (!bits_left_)
        {
            flush();
        }
        const bool bit = (buffer_ >> (--bits_left_)) & 1;
        return bit;
    }
    uint32_t read(int n)
    {
        uint32_t result = 0;
        while (n--)
        {
            result = (result << 1) | read_bit();
        }
        return result;
    }
    void unget_bit()
    {
        if (bits_left_ < 8)
        {
            ++bits_left_;
        } else
        {
            is_.unget().unget() >> buffer_;
            bits_left_ = 1;
        }
    }
    void unget(int n)
    {
        while (n--)
            unget_bit();
    }
    ibstream& operator >>(bool &value)
    {
        value = read_bit();
        return *this;
    }
    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    ibstream& operator >>(T &value)
    {
        value = static_cast<T>(read(sizeof(T) * 8));
        return *this;
    }
    [[nodiscard]] bool good() const
    {
        return is_.good();
    }
    [[nodiscard]] bool eof() const
    {
        return is_.eof();
    }
};


class BACKUP_SUITE_API obstream
{
    std::ostream& os_;
    uint8_t buffer_;
    int bits_left_;
public:
    explicit obstream(std::ostream& os): os_(os), buffer_(0), bits_left_(0) { }
    ~obstream()
    {
        flush();
    }
    void flush()
    {
        os_.put(static_cast<char>(buffer_));
        buffer_ = bits_left_ = 8;
        os_.flush();
    }
    void write_bit(const bool bit)
    {
        if (bit)
        {
            buffer_ |= (1 << --bits_left_);
        }
        if (bits_left_ == 0)
        {
            flush();
        }
    }
    template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    void write(const T bits, int n=-1)
    {
        if (n==-1)
            n = sizeof(T) * 8;
        while (n--)
        {
            write_bit(bits & 1);
            bits >>= 1;
        }
    }
    obstream& operator <<(const bool &value)
    {
        write_bit(value);
        return *this;
    }
    [[nodiscard]] bool good() const
    {
        return os_.good();
    }
    [[nodiscard]] bool eof() const
    {
        return os_.eof();
    }
};

class BACKUP_SUITE_API IstreamBuf: public std::streambuf
{
    std::ifstream &file_;
    int offset_;
    size_t size_;
    static constexpr size_t buffer_size_ = 8192;
    char buffer_[buffer_size_] = {};
public:
    explicit IstreamBuf(std::ifstream &ifs, int offset, size_t size):
        file_(ifs), offset_(offset), size_(size)
    {
        setg(nullptr, nullptr, nullptr);
        if (offset < 0 || size == 0 || !ifs.is_open() || ifs.eof())
        {
            offset_ = size_ = 0;
            return;
        }
    }
    ~IstreamBuf() override = default;
    int_type underflow() override
    {
        if (gptr() < egptr())
        {
            return traits_type::to_int_type(*gptr());
        }
        file_.seekg(offset_, std::ios::beg);
        size_t to_read = std::min(buffer_size_, size_);
        if (!file_.read(buffer_, to_read))
        {
            return traits_type::eof();
        }
        offset_ += to_read;
        size_ -= to_read;
        if (to_read == 0 || file_.eof() || file_.gcount() != static_cast<std::streamsize>(to_read))
        {
            return traits_type::eof();
        }
        setg(buffer_, buffer_, buffer_ + to_read);
        return traits_type::to_int_type(*gptr());
    }
};
class BACKUP_SUITE_API FileEntityIstream: public std::istream
{
    FileEntityMeta meta_;
    std::ifstream &file_;
    std::unique_ptr<IstreamBuf> buffer_;
public:
    FileEntityIstream(std::ifstream &ifs, const int offset, const FileEntityMeta &meta):
        std::istream(nullptr), meta_(meta), file_(ifs), buffer_(std::make_unique<IstreamBuf>(ifs, offset, meta.size))
    {
        rdbuf(buffer_.get());
        if (!meta_.size || !ifs.is_open() || ifs.eof())
        {
            setstate(std::ios::eofbit);
        }
    }
    ~FileEntityIstream() override = default;
    FileEntityMeta get_meta() { return meta_; }
};

#endif // BACKUPSUITE_BSTREAM_H
