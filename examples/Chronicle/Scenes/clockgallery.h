#pragma once

#include "Scene/scenemanager.h"

#include "UI/themesystem.h"
#include "UI/label.h"

namespace chronicle
{
    // Dev scene: four life clocks from res/chronicle/ui/clockgallery.yaml. The `life` clock
    // follows GameDataView (life.age, activity.running.months, life.next.label, life.next.in);
    // the buttons write those paths the way the game will when a month passes or an activity
    // starts. T toggles theme, R reduced motion.
    struct ClockGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        void advanceMonth();

        void updateNext(bool relabel);

        void setChoir(bool closed);

        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;

        pg::_unique_id lifeId = 0;

        pg::_unique_id choirButtonId = 0;

        int monthsLived = 0;           // Months past the starting 17.4
        int runningMonths = 9;
        size_t nextIndex = 0;          // The milestone the right-hand line names
        size_t choirIndex = 0;
        bool choirClosed = true;

        Label echo;
        Label motionLabel;
    };
}
