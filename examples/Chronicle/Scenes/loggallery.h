#pragma once

#include <string>
#include <vector>

#include "Scene/scenemanager.h"

#include "UI/themesystem.h"
#include "UI/label.h"

namespace chronicle
{
    // Dev scene: three event logs from res/chronicle/ui/loggallery.yaml. The buttons append to
    // the column's log and the wide one from the scene's own handler, as the Life screen will
    // from the rules: "A month passes" (a note, a gain or a coin at the next month),
    // "Milestone", "Loss", "Scroll to end" and "Clear". A log follows a new row only when its
    // view was already at the end; the echo shows whether the column's log is. T toggles theme.
    struct LogGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        void appendToAll(const std::string& text, int kind, const std::string& figure);

        void showEnd();

        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;

        std::vector<pg::_unique_id> logs;        // The column's log first, then the wide one

        int monthsPassed = 0;          // Past 16.9, the last entry in the file
        int shownEnd = -1;             // What the echo last said: -1 unknown, 0 reading up, 1 at the end

        Label echo;
        Label endLabel;
    };
}
