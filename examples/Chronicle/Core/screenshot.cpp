#include "stdafx.h"

#include "screenshot.h"

#include <cstring>

#include "ECS/entitysystem.h"

// No other file of Chronicle's targets writes images: the writer is compiled here, kept to this file
// (static) so that it can never meet another game's copy of it
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "Helpers/stbi_image_write.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace chronicle
{
    namespace
    {
        std::string base64(const std::string& bytes)
        {
            static const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

            std::string out;
            out.reserve((bytes.size() + 2) / 3 * 4);

            size_t i = 0;
            for (; i + 2 < bytes.size(); i += 3)
            {
                const uint32_t n = (static_cast<unsigned char>(bytes[i]) << 16) | (static_cast<unsigned char>(bytes[i + 1]) << 8) | static_cast<unsigned char>(bytes[i + 2]);
                out += alphabet[(n >> 18) & 63];
                out += alphabet[(n >> 12) & 63];
                out += alphabet[(n >> 6) & 63];
                out += alphabet[n & 63];
            }

            const size_t rest = bytes.size() - i;
            if (rest == 1)
            {
                const uint32_t n = static_cast<unsigned char>(bytes[i]) << 16;
                out += alphabet[(n >> 18) & 63];
                out += alphabet[(n >> 12) & 63];
                out += "==";
            }
            else if (rest == 2)
            {
                const uint32_t n = (static_cast<unsigned char>(bytes[i]) << 16) | (static_cast<unsigned char>(bytes[i + 1]) << 8);
                out += alphabet[(n >> 18) & 63];
                out += alphabet[(n >> 12) & 63];
                out += alphabet[(n >> 6) & 63];
                out += '=';
            }

            return out;
        }

        void appendBytes(void* context, void* data, int size)
        {
            static_cast<std::string*>(context)->append(static_cast<const char*>(data), static_cast<size_t>(size));
        }

        bool wholeFrame(const std::vector<unsigned char>& rgba, int w, int h)
        {
            return w > 0 and h > 0 and rgba.size() >= static_cast<size_t>(w) * h * 4;
        }
    }

    std::vector<unsigned char> flipRows(const std::vector<unsigned char>& rgba, int w, int h)
    {
        if (not wholeFrame(rgba, w, h))
            return rgba;

        const size_t row = static_cast<size_t>(w) * 4;

        std::vector<unsigned char> flipped(row * h);

        for (int y = 0; y < h; ++y)
            std::memcpy(flipped.data() + row * y, rgba.data() + row * (h - 1 - y), row);

        return flipped;
    }

    void fitWithin(std::vector<unsigned char>& rgba, int& w, int& h, int longest)
    {
        if (longest <= 0 or not wholeFrame(rgba, w, h) or (w <= longest and h <= longest))
            return;

        // The longer side becomes `longest`, the other keeps the ratio
        int nw, nh;
        if (w >= h)
        {
            nw = longest;
            nh = std::max(1, static_cast<int>((static_cast<int64_t>(h) * longest + w / 2) / w));
        }
        else
        {
            nh = longest;
            nw = std::max(1, static_cast<int>((static_cast<int64_t>(w) * longest + h / 2) / h));
        }

        std::vector<unsigned char> out(static_cast<size_t>(nw) * nh * 4);

        for (int y = 0; y < nh; ++y)
        {
            const int y0 = static_cast<int>(static_cast<int64_t>(y) * h / nh);
            const int y1 = std::max(y0 + 1, static_cast<int>(static_cast<int64_t>(y + 1) * h / nh));

            for (int x = 0; x < nw; ++x)
            {
                const int x0 = static_cast<int>(static_cast<int64_t>(x) * w / nw);
                const int x1 = std::max(x0 + 1, static_cast<int>(static_cast<int64_t>(x + 1) * w / nw));

                uint32_t sum[4] = { 0, 0, 0, 0 };

                for (int sy = y0; sy < y1; ++sy)
                {
                    const unsigned char* p = rgba.data() + (static_cast<size_t>(sy) * w + x0) * 4;

                    for (int sx = x0; sx < x1; ++sx, p += 4)
                    {
                        sum[0] += p[0];
                        sum[1] += p[1];
                        sum[2] += p[2];
                        sum[3] += p[3];
                    }
                }

                const uint32_t count = static_cast<uint32_t>((y1 - y0) * (x1 - x0));
                unsigned char* q = out.data() + (static_cast<size_t>(y) * nw + x) * 4;

                for (int c = 0; c < 4; ++c)
                    q[c] = static_cast<unsigned char>((sum[c] + count / 2) / count);
            }
        }

        rgba.swap(out);
        w = nw;
        h = nh;
    }

    std::string jpegBase64(const std::vector<unsigned char>& rgba, int w, int h, int quality)
    {
        if (not wholeFrame(rgba, w, h))
            return "";

        std::string jpeg;

        // Four components: the writer leaves the alpha out
        if (stbi_write_jpg_to_func(appendBytes, &jpeg, w, h, 4, rgba.data(), quality) == 0 or jpeg.empty())
            return "";

        return "data:image/jpeg;base64," + base64(jpeg);
    }

    void FrameGrab::request(std::function<void(std::string dataUrl)> done)
    {
        pending = std::move(done);

        ecsRef->sendEvent(pg::SaveCurrentFrameEvent{});
        ecsRef->sendEvent(pg::ReRendererAll{});
    }

    void FrameGrab::onEvent(const pg::SavedFrameData& event)
    {
        if (not pending)
            return;

        auto done = std::move(pending);
        pending = nullptr;

        int w = event.width;
        int h = event.height;

        auto pixels = flipRows(event.pixels, w, h);
        fitWithin(pixels, w, h);

        // A frame that could not be read answers "": the report goes without its picture
        done(jpegBase64(pixels, w, h));
    }
}
