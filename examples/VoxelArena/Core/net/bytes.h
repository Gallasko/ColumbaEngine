#pragma once

#include <cstdint>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "glm/glm.hpp"

namespace arena::net
{
    /** Little-endian writer for messages. */
    class Writer
    {
    public:
        std::vector<uint8_t> data;

        void u8(uint8_t v) { data.push_back(v); }
        void u16(uint16_t v) { u8(static_cast<uint8_t>(v)); u8(static_cast<uint8_t>(v >> 8)); }
        void u32(uint32_t v) { u16(static_cast<uint16_t>(v)); u16(static_cast<uint16_t>(v >> 16)); }
        void u64(uint64_t v) { u32(static_cast<uint32_t>(v)); u32(static_cast<uint32_t>(v >> 32)); }
        void i8(int8_t v) { u8(static_cast<uint8_t>(v)); }
        void i16(int16_t v) { u16(static_cast<uint16_t>(v)); }
        void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
        void f32(float v) { uint32_t u; std::memcpy(&u, &v, 4); u32(u); }
        void boolean(bool v) { u8(v ? 1 : 0); }
        void vec3(const glm::vec3& v) { f32(v.x); f32(v.y); f32(v.z); }
        void str(const std::string& s) { u16(static_cast<uint16_t>(s.size())); data.insert(data.end(), s.begin(), s.end()); }

        /** A unit direction in 6 bytes. */
        void dir(const glm::vec3& v)
        {
            i16(static_cast<int16_t>(glm::clamp(v.x, -1.0f, 1.0f) * 32767.0f));
            i16(static_cast<int16_t>(glm::clamp(v.y, -1.0f, 1.0f) * 32767.0f));
            i16(static_cast<int16_t>(glm::clamp(v.z, -1.0f, 1.0f) * 32767.0f));
        }
    };

    /** Reads what Writer wrote. A read past the end returns zeros and sets failed: a bad message is dropped, never trusted. */
    class Reader
    {
    public:
        Reader(const uint8_t* data, size_t size) : p(data), end(data + size) {}
        explicit Reader(const std::vector<uint8_t>& v) : Reader(v.data(), v.size()) {}

        bool failed = false;

        bool more() const { return p < end; }
        size_t left() const { return static_cast<size_t>(end - p); }

        uint8_t u8() { if (p >= end) { failed = true; return 0; } return *p++; }
        uint16_t u16() { uint16_t a = u8(); return static_cast<uint16_t>(a | (u8() << 8)); }
        uint32_t u32() { uint32_t a = u16(); return a | (static_cast<uint32_t>(u16()) << 16); }
        uint64_t u64() { uint64_t a = u32(); return a | (static_cast<uint64_t>(u32()) << 32); }
        int8_t i8() { return static_cast<int8_t>(u8()); }
        int16_t i16() { return static_cast<int16_t>(u16()); }
        int32_t i32() { return static_cast<int32_t>(u32()); }
        float f32() { uint32_t u = u32(); float v; std::memcpy(&v, &u, 4); return std::isfinite(v) ? v : (failed = true, 0.0f); }
        bool boolean() { return u8() != 0; }
        glm::vec3 vec3() { float x = f32(), y = f32(); return glm::vec3(x, y, f32()); }

        std::string str()
        {
            const uint16_t n = u16();

            if (left() < n)
            {
                failed = true;
                return "";
            }

            std::string s(reinterpret_cast<const char*>(p), n);
            p += n;
            return s;
        }

        glm::vec3 dir()
        {
            const float x = i16() / 32767.0f, y = i16() / 32767.0f;
            return glm::vec3(x, y, i16() / 32767.0f);
        }

    private:
        const uint8_t* p;
        const uint8_t* end;
    };
}
