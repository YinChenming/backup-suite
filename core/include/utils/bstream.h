//
// Created by ycm on 2025/10/2.
//

#ifndef BACKUPSUITE_BSTREAM_H
#define BACKUPSUITE_BSTREAM_H

#include <iostream>

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

#endif // BACKUPSUITE_BSTREAM_H
