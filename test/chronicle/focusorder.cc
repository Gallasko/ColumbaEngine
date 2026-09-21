#include "stdafx.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <gtest/gtest.h>

#include "UI/focusorder.h"

#include "ECS/entitysystem.h"
#include "UI/focusable.h"
#include "Input/inputcomponent.h"

#include "mocklogger.h"

using namespace chronicle;

namespace pg
{
    namespace test
    {
        namespace
        {
            struct FocusRecorder : public System<Listener<KeyboardFocusChangedEvent>, StoragePolicy>
            {
                std::string getSystemName() const override { return "Focus Recorder"; }
                void onEvent(const KeyboardFocusChangedEvent& e) override { events.push_back(e); }
                std::vector<KeyboardFocusChangedEvent> events;
            };

            struct FocusFixture
            {
                EntitySystem ecs;
                FocusableSystem* focus = nullptr;
                FocusOrderSystem* order = nullptr;
                FocusRecorder* recorder = nullptr;

                FocusFixture()
                {
                    ecs.createSystem<PositionComponentSystem>();
                    focus = ecs.createSystem<FocusableSystem>();
                    order = ecs.createSystem<FocusOrderSystem>();
                    recorder = ecs.createSystem<FocusRecorder>();
                }

                void pump() { ecs.executeOnce(); ecs.executeOnce(); ecs.executeOnce(); }

                _unique_id makeFocusable()
                {
                    auto e = ecs.createEntity();
                    ecs.attach<PositionComponent>(e);
                    ecs.attach<FocusableComponent>(e);
                    pump();
                    order->add(e.id);
                    return e.id;
                }

                void tab(Uint16 mod = 0) { ecs.sendEvent(OnSDLScanCode{SDL_SCANCODE_TAB, mod}); pump(); }
                void click() { ecs.sendEvent(OnMouseClick{Point2D{0, 0}, static_cast<MouseButton>(1)}); pump(); }
            };
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(focusorder_test, tab_next_prev_wrap)
        {
            MockLogger logger;
            FocusFixture s;
            const _unique_id a = s.makeFocusable();
            const _unique_id b = s.makeFocusable();

            s.tab();
            EXPECT_EQ(s.order->current(), a);
            EXPECT_TRUE(s.order->keyboardFocus());

            s.tab();
            EXPECT_EQ(s.order->current(), b);

            s.tab();   // wraps
            EXPECT_EQ(s.order->current(), a);

            s.tab(KMOD_SHIFT);   // previous -> b
            EXPECT_EQ(s.order->current(), b);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(focusorder_test, from_nothing_first_and_last)
        {
            MockLogger logger;
            FocusFixture s;
            const _unique_id a = s.makeFocusable();
            const _unique_id b = s.makeFocusable();

            s.tab();
            EXPECT_EQ(s.order->current(), a);   // Tab from nothing -> first

            FocusFixture s2;
            const _unique_id c = s2.makeFocusable();
            const _unique_id d = s2.makeFocusable();
            (void)c;
            s2.tab(KMOD_SHIFT);
            EXPECT_EQ(s2.order->current(), d);  // Shift-Tab from nothing -> last
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(focusorder_test, tab_skips_disabled)
        {
            MockLogger logger;
            FocusFixture s;
            const _unique_id a = s.makeFocusable();
            const _unique_id b = s.makeFocusable();
            const _unique_id c = s.makeFocusable();

            s.order->setEnabled(b, false);

            s.tab();
            EXPECT_EQ(s.order->current(), a);
            s.tab();
            EXPECT_EQ(s.order->current(), c);   // b is skipped
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(focusorder_test, focus_calls_focusable_system)
        {
            MockLogger logger;
            FocusFixture s;
            const _unique_id a = s.makeFocusable();
            s.tab();
            EXPECT_EQ(s.focus->currentFocus, a);   // focus() sent OnFocus
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        TEST(focusorder_test, click_clears_keyboard_keeps_current)
        {
            MockLogger logger;
            FocusFixture s;
            const _unique_id a = s.makeFocusable();

            s.tab();
            EXPECT_TRUE(s.order->keyboardFocus());
            const size_t before = s.recorder->events.size();

            s.click();
            EXPECT_FALSE(s.order->keyboardFocus());
            EXPECT_EQ(s.order->current(), a);   // current stays

            ASSERT_GT(s.recorder->events.size(), before);
            EXPECT_FALSE(s.recorder->events.back().keyboard);
        }
    }
}
