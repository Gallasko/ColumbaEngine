#pragma once

#include <string>
#include <unordered_map>

#include "Scene/scenemanager.h"

#include "UI/themesystem.h"
#include "UI/label.h"
#include "UI/activityrow.h"

namespace chronicle
{
    // Dev scene: the activity list and a side panel from res/chronicle/ui/activitygallery.yaml.
    // The side panel shows the activity at work; selecting a row shows that one instead, as
    // chosen, and confirming it (the button, a second click, or Enter) starts it: the row goes
    // running in the list and the one that ran goes back to idle. Advance a month moves the
    // running activity through activity.running.percent; Meet the requirement writes
    // activity.train.squire.req.0.current. T toggles theme, R reduced motion.
    struct ActivityGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        void advanceMonth();

        void start(const std::string& id);

        void showRunning();

        void show(const ActivityRowSpec& what, ActivityState state, const std::string& heading);

        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;

        pg::_unique_id listId = 0;

        pg::_unique_id sideId = 0;

        pg::_unique_id nowId = 0;

        std::string runningId = "study.letters";   // "" when nothing is at work

        std::unordered_map<std::string, std::string> eachOf;   // What a finished row said before DONE

        int month = 3;                 // Of the running activity's months
        int months = 6;
        int strength = 15;             // The Squire row's first requirement

        Label echo;
        Label motionLabel;
    };
}
