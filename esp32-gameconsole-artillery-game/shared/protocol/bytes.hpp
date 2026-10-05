#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace artillery {

/** Explicit little-endian cursor — no host struct casts (MSVC vs P4 padding). */
class ByteWriter {
public:
    ByteWriter(uint8_t* buf, size_t cap) : buf_(buf), cap_(cap) {}

    size_t size() const { return n_; }
    size_t remaining() const { return n_ <= cap_ ? cap_ - n_ : 0; }
    bool ok() const { return ok_; }

    bool u8(uint8_t v)
    {
        if (n_ + 1 > cap_) {
            ok_ = false;
            return false;
        }
        buf_[n_++] = v;
        return true;
    }

    bool u16(uint16_t v)
    {
        if (n_ + 2 > cap_) {
            ok_ = false;
            return false;
        }
        buf_[n_++] = static_cast<uint8_t>(v);
        buf_[n_++] = static_cast<uint8_t>(v >> 8);
        return true;
    }

    bool i16(int16_t v) { return u16(static_cast<uint16_t>(v)); }

    bool u32(uint32_t v)
    {
        if (n_ + 4 > cap_) {
            ok_ = false;
            return false;
        }
        buf_[n_++] = static_cast<uint8_t>(v);
        buf_[n_++] = static_cast<uint8_t>(v >> 8);
        buf_[n_++] = static_cast<uint8_t>(v >> 16);
        buf_[n_++] = static_cast<uint8_t>(v >> 24);
        return true;
    }

    bool i32(int32_t v) { return u32(static_cast<uint32_t>(v)); }

    bool bytes(const void* data, size_t len)
    {
        if (n_ + len > cap_) {
            ok_ = false;
            return false;
        }
        if (len > 0 && data != nullptr) {
            std::memcpy(buf_ + n_, data, len);
        }
        n_ += len;
        return true;
    }

    /** Fixed wire field, zero-padded (not length-prefixed). */
    bool cstr(const char* s, size_t wire_cap)
    {
        if (n_ + wire_cap > cap_) {
            ok_ = false;
            return false;
        }
        size_t i = 0;
        if (s != nullptr) {
            for (; i < wire_cap && s[i]; ++i) {
                buf_[n_ + i] = static_cast<uint8_t>(s[i]);
            }
        }
        for (; i < wire_cap; ++i) {
            buf_[n_ + i] = 0;
        }
        n_ += wire_cap;
        return true;
    }

private:
    uint8_t* buf_ = nullptr;
    size_t cap_ = 0;
    size_t n_ = 0;
    bool ok_ = true;
};

class ByteReader {
public:
    ByteReader(const uint8_t* buf, size_t len) : buf_(buf), len_(len) {}

    size_t tell() const { return n_; }
    size_t remaining() const { return n_ <= len_ ? len_ - n_ : 0; }
    bool ok() const { return ok_; }

    bool u8(uint8_t* out)
    {
        if (n_ + 1 > len_) {
            ok_ = false;
            return false;
        }
        *out = buf_[n_++];
        return true;
    }

    bool u16(uint16_t* out)
    {
        if (n_ + 2 > len_) {
            ok_ = false;
            return false;
        }
        *out = static_cast<uint16_t>(buf_[n_]) | (static_cast<uint16_t>(buf_[n_ + 1]) << 8);
        n_ += 2;
        return true;
    }

    bool i16(int16_t* out)
    {
        uint16_t v = 0;
        if (!u16(&v)) {
            return false;
        }
        *out = static_cast<int16_t>(v);
        return true;
    }

    bool u32(uint32_t* out)
    {
        if (n_ + 4 > len_) {
            ok_ = false;
            return false;
        }
        *out = static_cast<uint32_t>(buf_[n_]) | (static_cast<uint32_t>(buf_[n_ + 1]) << 8) |
               (static_cast<uint32_t>(buf_[n_ + 2]) << 16) | (static_cast<uint32_t>(buf_[n_ + 3]) << 24);
        n_ += 4;
        return true;
    }

    bool i32(int32_t* out)
    {
        uint32_t v = 0;
        if (!u32(&v)) {
            return false;
        }
        *out = static_cast<int32_t>(v);
        return true;
    }

    bool bytes(void* out, size_t n)
    {
        if (n_ + n > len_) {
            ok_ = false;
            return false;
        }
        if (n > 0 && out != nullptr) {
            std::memcpy(out, buf_ + n_, n);
        }
        n_ += n;
        return true;
    }

    /**
     * Read a fixed wire_cap field into out (out must have room for wire_cap+1).
     * Copies all non-zero-padded bytes so an 8-char token fills out[0..7].
     */
    bool cstr(char* out, size_t wire_cap)
    {
        if (out == nullptr || n_ + wire_cap > len_) {
            ok_ = false;
            return false;
        }
        size_t i = 0;
        for (; i < wire_cap; ++i) {
            const char c = static_cast<char>(buf_[n_ + i]);
            out[i] = c;
            if (c == 0) {
                break;
            }
        }
        out[i] = 0;
        // If the field used every byte (no embedded NUL), still terminate after wire_cap.
        if (i == wire_cap) {
            out[wire_cap] = 0;
        }
        n_ += wire_cap;
        return true;
    }

    const uint8_t* ptr() const { return buf_ + n_; }

    bool skip(size_t n)
    {
        if (n_ + n > len_) {
            ok_ = false;
            return false;
        }
        n_ += n;
        return true;
    }

private:
    const uint8_t* buf_ = nullptr;
    size_t len_ = 0;
    size_t n_ = 0;
    bool ok_ = true;
};

}  // namespace artillery
