#pragma once

#include <string>
#include <vector>

#include "Scene/scenemanager.h"

#include "UI/themesystem.h"
#include "UI/label.h"

namespace chronicle
{
    // Dev scene: seven window meters from res/chronicle/ui/windowgallery.yaml. Every meter
    // follows life.age; the two Squire meters also follow window.squire.state and
    // window.squire.note, which the scene words the way windows.pg will: the months left and
    // the attempts that still fit, closing the door itself when the age passes 22. "+6 months",
    // "Close the Squire door", "Reopen" and "Reset" write those facts. T toggles theme, R
    // reduced motion.
    struct WindowGallery : public pg::Scene
    {
        virtual void init() override;

    private:
        void writeSquire();

        pg::ThemeSystem* theme = nullptr;

        pg::_unique_id backgroundId = 0;

        std::vector<pg::_unique_id> meters;      // Every meter on the page: they share life.age
        std::vector<pg::_unique_id> squires;     // The two Squire meters

        int monthsLived = 0;           // Months past the starting 20.2
        bool squireClosed = false;

        Label echo;
        Label motionLabel;
    };
}
