#include "stdafx.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

#include <gtest/gtest.h>

#include "UI/switch.h"
#include "UI/radiocard.h"
#include "UI/wordfield.h"
#include "Core/settings.h"
#include "Scenes/lifesave.h"

#include "ECS/entitysystem.h"
#include "UI/themesystem.h"
#include "UI/ttftext.h"
#include "UI/iconsystem.h"
#include "UI/sizer.h"
#include "UI/focusable.h"
#include "Input/inputcomponent.h"
#include "Input/sdlevents.h"
#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"

#include "mocklogger.h"

using namespace chronicle;

// The Options tab's own pieces: the settings file, the save's version, and the three controls the
// page is made of. The page itself, and what Export, Load and Wipe do, are LifeScene's (lifescene.cc).
namespace pg
{
    namespace test
    {
        namespace
        {
            struct OptionsRecorder : public System<Listener<SwitchToggledEvent>, Listener<RadioChosenEvent>, Listener<WordChangedEvent>, StoragePolicy>
            {
                std::string getSystemName() const override { return "Options Recorder"; }
                void onEvent(const SwitchToggledEvent& e) override { switches.push_back(e); }
                void onEvent(const RadioChosenEvent& e) override { radios.push_back(e); }
                void onEvent(const WordChangedEvent& e) override { words.push_back(e); }
                std::vector<SwitchToggledEvent> switches;
                std::vector<RadioChosenEvent> radios;
                std::vector<WordChangedEvent> words;
            };

            struct OptionsFixture
            {
                EntitySystem ecs;
                MasterRenderer renderer;
                ThemeSystem* theme = nullptr;
                FocusOrderSystem* order = nullptr;
                OptionsRecorder* recorder = nullptr;

                OptionsFixture()
                {
                    ecs.createSystem<PositionComponentSystem>();
                    ecs.createSystem<LayoutSystem>();
                    ecs.succeed<PositionComponentSystem, LayoutSystem>();
                    ecs.createSystem<TTFTextSystem>(&renderer);
                    ecs.createSystem<Simple2DObjectSystem>(&renderer);
                    ecs.createSystem<RoundedRect2DObjectSystem>(&renderer);
                    ecs.createSystem<HatchRect2DObjectSystem>(&renderer);
                    ecs.createSystem<DottedLine2DObjectSystem>(&renderer);
                    ecs.createSystem<StrokeRect2DObjectSystem>(&renderer);
                    ecs.createSystem<IconSystem>(&renderer);
                    ecs.createSystem<MouseHoverSystem>();
                    ecs.createSystem<FocusableSystem>();
                    order = ecs.createSystem<FocusOrderSystem>();
                    theme = ecs.createSystem<ThemeSystem>();
                    ecs.createSystem<SwitchSystem>();
                    ecs.createSystem<RadioCardSystem>();
                    ecs.createSystem<WordFieldSystem>();
                    ecs.succeed<MouseHoverSystem, SwitchSystem>();
                    ecs.succeed<MouseHoverSystem, RadioCardSystem>();
                    ecs.succeed<MouseHoverSystem, WordFieldSystem>();
                    recorder = ecs.createSystem<OptionsRecorder>();
                    theme->loadTheme("chronicle/tokens.json", "fonts");
                }

                void pump() { ecs.executeOnce(); ecs.executeOnce(); ecs.executeOnce(); }
                void hover(float x, float y) { ecs.sendEvent(OnMouseMove{Point2D{x, y}, nullptr}); pump(); }
                void press(float x, float y) { ecs.sendEvent(OnMouseClick{Point2D{x, y}, static_cast<MouseButton>(1)}); pump(); }
                void release(float x, float y) { ecs.sendEvent(OnMouseRelease{Point2D{x, y}, static_cast<MouseButton>(1)}); pump(); }
                void click(float x, float y) { hover(x, y); press(x, y); release(x, y); }
                void key(SDL_Scancode k) { ecs.sendEvent(OnSDLScanCode{k, 0}); pump(); }
                void type(const std::string& text) { ecs.sendEvent(OnSDLTextInput{text}); pump(); }

                void place(EntityRef root, float x = 100.0f, float y = 100.0f)
                {
                    root->get<PositionComponent>()->setX(x);
                    root->get<PositionComponent>()->setY(y);
                    pump();
                }

                CompRef<PositionComponent> pos(EntityRef e) { return ecs.getEntity(e.id)->get<PositionComponent>(); }
                std::string element(EntityRef e) { return ecs.getEntity(e.id)->get<ThemeComponent>()->element; }
                float centreX(EntityRef e) { return pos(e)->x + pos(e)->width / 2.0f; }
                float centreY(EntityRef e) { return pos(e)->y + pos(e)->height / 2.0f; }
            };

            // A folder of its own for each test, empty
            std::filesystem::path scratch()
            {
                const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
                const auto folder = std::filesystem::temp_directory_path() / (std::string("chronicle-options-") + info->name());

                std::error_code ec;
                std::filesystem::remove_all(folder, ec);
                std::filesystem::create_directories(folder, ec);

                return folder;
            }

            std::string readAll(const std::filesystem::path& path)
            {
                std::ifstream file(path);
                std::stringstream text;
                text << file.rdbuf();
                return text.str();
            }

            void writeAll(const std::filesystem::path& path, const std::string& text)
            {
                std::ofstream file(path, std::ios::trunc);
                file << text;
            }
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The settings are kept in the save's own file, an object beside the life. The game's save
        // writes both; an export writes the life alone; a burnt chronicle leaves the settings alone.
        // A file without them, or of another kind, reads as the defaults, and so does a theme the
        // game has not
        TEST(options_test, settings_live_in_the_save_file)
        {
            MockLogger logger;
            const auto folder = scratch();
            const std::string path = (folder / "deep" / "life.sz").string();

            Settings defaults;
            EXPECT_FALSE(defaults.beginOnFirstClick);
            EXPECT_EQ(defaults.theme, "day");

            // Missing: the defaults
            Settings missing;
            EXPECT_FALSE(missing.load(path));
            EXPECT_EQ(missing.theme, "day");

            // The game's save: the life and the settings, in our own format, the file's folder made
            Settings chosen;
            chosen.beginOnFirstClick = true;
            chosen.theme = "candle";

            LifeSave life = boyOfSeven();
            life.name = "Wenna of the Mill";
            ASSERT_TRUE(life.save(path, &chosen));

            const std::string text = readAll(path);
            EXPECT_NE(text.find("life: LifeSave {"), std::string::npos) << text.substr(0, 200);
            EXPECT_NE(text.find("settings: Settings {"), std::string::npos);
            EXPECT_NE(text.find("theme: __PGSA string {candle}"), std::string::npos);
            EXPECT_EQ(text.find("settings: settings:"), std::string::npos);

            Settings read;
            ASSERT_TRUE(read.load(path));
            EXPECT_TRUE(read.beginOnFirstClick);
            EXPECT_EQ(read.theme, "candle");

            LifeSave back;
            ASSERT_TRUE(back.load(path));
            EXPECT_EQ(back.name, "Wenna of the Mill");

            // Load takes the life out of it and nothing else
            LifeSave opened;
            EXPECT_EQ(LifeSave::open(path, opened), LifeSave::Opened::Ok);

            // Written again and again, the file stays whole
            for (int i = 0; i < 3; ++i)
                ASSERT_TRUE(back.save(path, &chosen));

            ASSERT_TRUE(read.load(path));
            EXPECT_EQ(read.theme, "candle");
            ASSERT_TRUE(back.load(path));
            EXPECT_EQ(back.name, "Wenna of the Mill");

            // An export: the life alone
            const std::string exported = (folder / "chronicle-wenna-life1.sz").string();
            ASSERT_TRUE(back.save(exported));
            EXPECT_FALSE(read.load(exported));
            EXPECT_EQ(read.theme, "day");
            EXPECT_EQ(LifeSave::open(exported, opened), LifeSave::Opened::Ok);

            // Burnt: the settings alone, and no life
            ASSERT_TRUE(chosen.save(path));
            ASSERT_TRUE(read.load(path));
            EXPECT_TRUE(read.beginOnFirstClick);
            EXPECT_FALSE(back.load(path));
            EXPECT_EQ(LifeSave::open(path, opened), LifeSave::Opened::NotASave);

            // Not a save: the defaults, whatever was read before
            writeAll(path, "<html>not this</html>");
            EXPECT_FALSE(read.load(path));
            EXPECT_FALSE(read.beginOnFirstClick);
            EXPECT_EQ(read.theme, "day");

            // Written over a file of another kind, the save reads back: its first line is no header
            writeAll(path, "<html>not this</html>");
            ASSERT_TRUE(back.save(path, &chosen));
            EXPECT_EQ(LifeSave::open(path, opened), LifeSave::Opened::Ok);
            writeAll(path, "<html>not this</html>");

            // A theme the game has not: the day's
            Settings neon;
            neon.theme = "neon";
            ASSERT_TRUE(neon.save(path));
            ASSERT_TRUE(read.load(path));
            EXPECT_EQ(read.theme, "day");

            // No path: nothing read, nothing written
            EXPECT_FALSE(read.load(""));
            EXPECT_FALSE(read.save(""));
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A save carries the version of its format. Load takes a save of this build, and refuses a
        // file of another kind, a save from before the field, or one from a newer build
        TEST(options_test, save_version_is_written_and_checked)
        {
            MockLogger logger;
            const auto folder = scratch();
            const auto path = folder / "life.sz";

            LifeSave life = boyOfSeven();
            life.name = "Wenna of the Mill";
            life.lives = 3;
            ASSERT_TRUE(life.save(path.string()));

            const std::string text = readAll(path);
            const std::regex versionLine("\\n(\\s*)version: __PGSA int \\{" + std::to_string(LifeSave::SaveVersion) + "\\},");
            ASSERT_TRUE(std::regex_search(text, versionLine)) << text.substr(0, 400);

            // As written: this build's
            LifeSave opened;
            ASSERT_EQ(LifeSave::open(path.string(), opened), LifeSave::Opened::Ok);
            EXPECT_EQ(opened.version, LifeSave::SaveVersion);
            EXPECT_EQ(opened.name, "Wenna of the Mill");
            EXPECT_EQ(opened.lives, 3);

            // Read the old way (the game's own save at start), a save without the field reads 0
            const auto old = folder / "old.sz";
            writeAll(old, std::regex_replace(text, versionLine, ""));

            LifeSave legacy;
            ASSERT_TRUE(legacy.load(old.string()));
            EXPECT_EQ(legacy.version, 0);
            EXPECT_EQ(legacy.name, "Wenna of the Mill");

            // ...and Load does not take it from outside the game; nor touches what it was handed
            LifeSave untouched = freshLife();
            EXPECT_EQ(LifeSave::open(old.string(), untouched), LifeSave::Opened::NotASave);
            EXPECT_EQ(untouched.name, freshLife().name);
            EXPECT_EQ(untouched.age, freshLife().age);

            // A newer build's
            const auto newer = folder / "newer.sz";
            writeAll(newer, std::regex_replace(text, versionLine, "\n$1version: __PGSA int {99},"));
            EXPECT_EQ(LifeSave::open(newer.string(), untouched), LifeSave::Opened::Newer);
            EXPECT_EQ(untouched.name, freshLife().name);

            // Another kind of file, an empty one, one that is not there
            const auto foreign = folder / "foreign.sz";
            writeAll(foreign, "PNG\x1a\nthis is no chronicle\n");
            EXPECT_EQ(LifeSave::open(foreign.string(), untouched), LifeSave::Opened::NotASave);

            const auto empty = folder / "empty.sz";
            writeAll(empty, "");
            EXPECT_EQ(LifeSave::open(empty.string(), untouched), LifeSave::Opened::NotASave);

            EXPECT_EQ(LifeSave::open((folder / "nothing.sz").string(), untouched), LifeSave::Opened::NotASave);
            EXPECT_EQ(untouched.name, freshLife().name);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A switch turns over on a click, or on Enter with the keyboard on it, and says so; set by
        // the scene it says nothing. On, its track is verdigris and its knob at the right
        TEST(options_test, switch_turns_over)
        {
            MockLogger logger;
            OptionsFixture f;

            SwitchSpec spec;
            spec.tag = "options.click";
            Switch sw = makeSwitch(&f.ecs, spec);
            f.place(sw.root);

            EXPECT_GE(f.pos(sw.face)->height, 44.0f);
            EXPECT_EQ(f.element(sw.track), "switch.track");
            const float offKnob = f.pos(sw.knob)->x;

            f.click(f.centreX(sw.face), f.centreY(sw.face));

            ASSERT_EQ(f.recorder->switches.size(), 1u);
            EXPECT_TRUE(f.recorder->switches[0].on);
            EXPECT_EQ(f.recorder->switches[0].tag, "options.click");
            EXPECT_TRUE(sw.on(&f.ecs));
            EXPECT_EQ(f.element(sw.track), "switch.track.on");
            EXPECT_EQ(f.theme->elementEntry(f.element(sw.track), "color").get<std::string>(), "verdigris");
            EXPECT_GT(f.pos(sw.knob)->x, offKnob);

            // Set by the scene: no event
            sw.setOn(&f.ecs, false);
            f.pump();
            EXPECT_EQ(f.recorder->switches.size(), 1u);
            EXPECT_EQ(f.element(sw.track), "switch.track");

            // The keyboard: Enter on the switch it is on, its ring showing
            f.order->focus(sw.face.id);
            f.pump();
            EXPECT_TRUE(f.pos(sw.ring)->visible);

            f.key(SDL_SCANCODE_RETURN);
            ASSERT_EQ(f.recorder->switches.size(), 2u);
            EXPECT_TRUE(f.recorder->switches[1].on);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // The theme's cards: side by side where they have room, else one under the other. A click
        // or the arrows choose one; its edge and its radio are lit. Each swatch shows its own
        // theme's page, whichever is in use
        TEST(options_test, radio_cards_choose_and_show_their_theme)
        {
            MockLogger logger;
            OptionsFixture f;

            RadioCardsSpec spec;
            spec.tag = "options.light";
            spec.selected = "day";
            spec.width = 540.0f;
            spec.items = {{"day", "Scriptorium", "Day · ink on vellum", "day"}, {"candle", "Candle", "Night · pale ink on dark leaf", "candle"}};

            RadioCards cards = makeRadioCards(&f.ecs, spec);
            f.place(cards.root);

            ASSERT_EQ(cards.cards.size(), 2u);
            EXPECT_NEAR(f.pos(cards.cards[0].face)->y, f.pos(cards.cards[1].face)->y, 0.5f);
            EXPECT_GT(f.pos(cards.cards[1].face)->x, f.pos(cards.cards[0].face)->x + f.pos(cards.cards[0].face)->width);
            EXPECT_TRUE(f.pos(cards.cards[0].edge)->visible);
            EXPECT_FALSE(f.pos(cards.cards[1].edge)->visible);

            // The candle's swatch is the candle's page, in the day theme too
            ASSERT_FALSE(cards.cards[1].swatch.empty());
            const auto ground = f.ecs.getEntity(cards.cards[1].swatch.id)->get<Simple2DObject>()->colors;
            const auto candle = f.theme->theme().color("vellum", "candle");
            EXPECT_FLOAT_EQ(ground.x, candle.x);
            EXPECT_FLOAT_EQ(ground.y, candle.y);
            EXPECT_FLOAT_EQ(ground.z, candle.z);

            f.click(f.centreX(cards.cards[1].face), f.centreY(cards.cards[1].face));

            ASSERT_EQ(f.recorder->radios.size(), 1u);
            EXPECT_EQ(f.recorder->radios[0].id, "candle");
            EXPECT_TRUE(f.pos(cards.cards[1].edge)->visible);
            EXPECT_TRUE(f.pos(cards.cards[1].dot)->visible);
            EXPECT_FALSE(f.pos(cards.cards[0].dot)->visible);

            // The arrows, from the card the keyboard is on: the other one, chosen
            f.order->focus(cards.cards[1].face.id);
            f.pump();
            f.key(SDL_SCANCODE_LEFT);
            ASSERT_EQ(f.recorder->radios.size(), 2u);
            EXPECT_EQ(f.recorder->radios[1].id, "day");

            // Chosen by the scene: no event
            cards.select(&f.ecs, "candle");
            f.pump();
            EXPECT_EQ(f.recorder->radios.size(), 2u);
            EXPECT_TRUE(f.pos(cards.cards[1].dot)->visible);

            // Narrow: one under the other, each the whole width
            cards.setWidth(&f.ecs, 372.0f);
            f.pump();
            EXPECT_NEAR(f.pos(cards.cards[0].face)->x, f.pos(cards.cards[1].face)->x, 0.5f);
            EXPECT_GT(f.pos(cards.cards[1].face)->y, f.pos(cards.cards[0].face)->y);
            EXPECT_NEAR(f.pos(cards.cards[0].face)->width, 372.0f, 0.5f);
            EXPECT_NEAR(f.pos(cards.root)->height, cards.height(), 0.5f);
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A word field takes what is typed while it has the focus, Backspace takes a letter back,
        // and a click elsewhere leaves it
        TEST(options_test, word_field_takes_what_is_typed)
        {
            MockLogger logger;
            OptionsFixture f;

            WordFieldSpec spec;
            spec.tag = "options.burnWord";
            spec.placeholder = "BURN";
            spec.most = 6;
            WordField field = makeWordField(&f.ecs, spec);
            f.place(field.root);

            EXPECT_TRUE(f.pos(field.placeholder.entity)->visible);

            // Not focused: nothing is taken
            f.type("b");
            EXPECT_TRUE(f.recorder->words.empty());

            f.click(f.centreX(field.face), f.centreY(field.face));
            EXPECT_TRUE(f.ecs.getSystem<WordFieldSystem>()->anyTyping());
            EXPECT_TRUE(f.pos(field.caret)->visible);

            for (const char* letter : {"b", "U", "r", "n"})
                f.type(letter);

            ASSERT_EQ(f.recorder->words.size(), 4u);
            EXPECT_EQ(f.recorder->words.back().text, "bUrn");
            EXPECT_EQ(field.text(&f.ecs), "bUrn");
            EXPECT_FALSE(f.pos(field.placeholder.entity)->visible);

            f.key(SDL_SCANCODE_BACKSPACE);
            EXPECT_EQ(field.text(&f.ecs), "bUr");

            // No more than it takes: what is typed past it is cut, and once full nothing goes in
            f.type("ababab");
            EXPECT_EQ(field.text(&f.ecs), "bUraba");
            f.type("x");
            EXPECT_EQ(field.text(&f.ecs), "bUraba");

            // A click elsewhere: it lets go
            f.click(900.0f, 900.0f);
            EXPECT_FALSE(f.ecs.getSystem<WordFieldSystem>()->anyTyping());
            EXPECT_FALSE(f.pos(field.caret)->visible);

            f.key(SDL_SCANCODE_BACKSPACE);
            f.type("x");
            EXPECT_EQ(field.text(&f.ecs), "bUraba");
        }

        // ----------------------------------------------------------------------------------------
        // ---------------------------        Test separator        -------------------------------
        // ----------------------------------------------------------------------------------------
        // A space typed at the end moves the caret by a space at once, not when the next letter
        // comes; two spaces move it twice as far, and a letter after a space goes past both
        TEST(options_test, word_field_caret_goes_past_a_space)
        {
            MockLogger logger;
            OptionsFixture f;

            WordFieldSpec spec;
            spec.tag = "report.text";
            spec.most = 24;
            WordField field = makeWordField(&f.ecs, spec);
            f.place(field.root);
            field.focus(&f.ecs);
            f.pump();

            const TextStyle& style = f.theme->style("control");
            const float space = f.ecs.getSystem<TTFTextSystem>()->fonts.at(style.fontAlias).glyphOrNotdef(0x20).advance + style.letterSpacingPx;
            ASSERT_GT(space, 1.0f);

            auto caretX = [&]() { return f.pos(field.caret)->x; };

            f.type("a");
            const float afterA = caretX();

            f.type(" ");
            EXPECT_EQ(field.text(&f.ecs), "a ");
            EXPECT_NEAR(caretX(), afterA + space, 0.5f);

            f.type(" ");
            EXPECT_EQ(field.text(&f.ecs), "a  ");
            EXPECT_NEAR(caretX(), afterA + 2.0f * space, 0.5f);

            // A letter after the spaces: the caret goes on past it, the spaces kept in between
            f.key(SDL_SCANCODE_BACKSPACE);
            const float afterSpace = caretX();
            f.type("b");
            EXPECT_EQ(field.text(&f.ecs), "a b");
            EXPECT_GT(caretX(), afterSpace + 1.0f);

            // Backspace takes the letter back: the caret is past the space again
            f.key(SDL_SCANCODE_BACKSPACE);
            EXPECT_NEAR(caretX(), afterA + space, 0.5f);
        }
    }
}
