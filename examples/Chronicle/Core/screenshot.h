#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ECS/system.h"
#include "Renderer/renderer.h"

namespace chronicle
{
    // The screen as it stood, for a bug report: caught inside a frame by the renderer's own read
    // (SaveCurrentFrameEvent → SavedFrameData, at the drawable's size), turned the right way up,
    // fitted within a size and written as a JPEG data URL

    constexpr int ScreenLongest = 1280;
    constexpr int ScreenQuality = 80;

    // The rows in the other order: OpenGL reads the bottom row first, an image starts with the top
    std::vector<unsigned char> flipRows(const std::vector<unsigned char>& rgba, int w, int h);

    // Shrinks the frame so that its longer side is at most `longest`, keeping its ratio; each pixel
    // is the average of the pixels that fall in it. A frame already within it is left alone
    void fitWithin(std::vector<unsigned char>& rgba, int& w, int& h, int longest = ScreenLongest);

    // "data:image/jpeg;base64,..." of an RGBA frame (the alpha is dropped); "" when it cannot be written
    std::string jpegBase64(const std::vector<unsigned char>& rgba, int w, int h, int quality = ScreenQuality);

    // Asks the renderer for the next frame and answers with it as a data URL. The renderer reads
    // the frame while it draws, before the swap, so the picture is the screen as the player saw it.
    // One request at a time: a request made while one waits takes its place
    struct FrameGrab : public pg::System<pg::Listener<pg::SavedFrameData>, pg::StoragePolicy>
    {
        virtual std::string getSystemName() const override { return "Frame Grab"; }

        // Sends SaveCurrentFrameEvent and ReRendererAll (the renderer draws only on a change); `done`
        // is called with the data URL from the frame that comes back
        void request(std::function<void(std::string dataUrl)> done);

        // A request waits for its frame
        bool waiting() const { return static_cast<bool>(pending); }

        // The request waiting is forgotten: its `done` is never called (its asker is gone, or gave up
        // waiting). The frame asked for still comes, and is no one's
        void cancel() { pending = nullptr; }

        void onEvent(const pg::SavedFrameData& event) override;

    private:
        std::function<void(std::string)> pending;
    };
}
