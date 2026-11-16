#include "application.h"

#include "window.h"
#include "UI/ttftext.h"
#include "UI/texteditor/texteditor.h"
#include "ECS/entitysystem_fwd.h"

#include <chrono>
#include <cstdlib>
#include <iostream>

using namespace pg;

namespace
{
    const char* SampleText =
        "# The text editor\n"
        "\n"
        "A paragraph with **bold**, *italic*, `inline code` and a [link](https://columbaengine.org/). "
        "It is long on purpose, so it wraps at the window's edge and you can see the cursor walk across the wrap point.\n"
        "\n"
        "## A list\n"
        "\n"
        "- the first item\n"
        "- the second, with an_underscore_word that is not italic\n"
        "12. a numbered one\n"
        "\n"
        "```cpp\n"
        "bool canFail(const Expr* e) { return e->kind == Kind::Call; } // **not bold here**\n"
        "```\n"
        "\n"
        "Backslashes drawn as written: a\\nb, \\c{1,2,3,4}, C:\\path\\to\\file\\\n"
        "Accents and symbols: é à ç — “quotes” …\n"
        "<!-- visual: a chart of the 1594 ms to 117 ms change -->\n"
        "<!-- number: how many statements a run removes -->\n"
        "\n"
        "https://example.com/a/very/long/path/that/has/no/space/in/it/at/all/so/it/must/break/anyway\n";

    // Prints the text on Ctrl+S
    struct DemoSaveSystem : public System<Listener<TextEditorSaveRequested>, StoragePolicy>
    {
        void onEvent(const TextEditorSaveRequested& event) override
        {
            auto ent = ecsRef->getEntity(event.id);
            if (not ent or not ent->has<TextEditorComponent>())
                return;
            auto editor = ent->get<TextEditorComponent>();
            std::cout << "---- saved ----\n" << editor->text() << "\n---------------" << std::endl;
            editor->markSaved();
        }
    };

    // Makes the editor fill the window
    struct DemoSceneSystem : public System<InitSys, Listener<ResizeEvent>>
    {
        DemoSceneSystem(float width, float height) : width(width), height(height) {}

        void init() override
        {
            auto editor = makeTextEditor(ecsRef, 0.0f, 0.0f, width, height, SampleText);
            editorId = editor.entity.id;
        }

        // The window takes the focus as it starts: give it to the editor on the first frame.
        // With PG_EDITOR_DEMO_SCRIPT set, it also plays keys as a keyboard would, a stage every
        // 1.5 s (type, select, scroll, find), so the widget can be checked by screenshots
        void execute() override
        {
            if (not focused)
            {
                ecsRef->sendEvent(OnFocus{editorId});
                focused = true;
                start = std::chrono::steady_clock::now();
            }
            if (not std::getenv("PG_EDITOR_DEMO_SCRIPT"))
                return;
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            const int due = static_cast<int>(seconds / 1.5);
            while (stage < due and stage < 4)
            {
                ++stage;
                play(stage);
            }
        }

        void key(SDL_Scancode code, Uint16 mod = 0) { ecsRef->sendEvent(OnSDLScanCode{code, mod}); }

        void play(int which)
        {
            switch (which)
            {
                case 1: // type at the start
                    ecsRef->sendEvent(OnSDLTextInput{"Typed here. "});
                    break;
                case 2: // select the word "Typed" with Shift+Ctrl+Left, after going to line start
                    key(SDL_SCANCODE_HOME);
                    key(SDL_SCANCODE_RIGHT, KMOD_SHIFT | KMOD_CTRL);
                    break;
                case 3: // scroll down three lines a notch
                    key(SDL_SCANCODE_PAGEDOWN);
                    break;
                case 4: // find "the"
                    key(SDL_SCANCODE_F, KMOD_CTRL);
                    ecsRef->sendEvent(OnSDLTextInput{"the"});
                    break;
            }
            std::cout << "[demo] stage " << which << std::endl;
        }

        void onEvent(const ResizeEvent& event) override
        {
            auto ent = ecsRef->getEntity(editorId);
            if (not ent)
                return;
            auto pos = ent->get<PositionComponent>();
            pos->setWidth(event.width);
            pos->setHeight(event.height);
        }

        float width, height;
        _unique_id editorId = 0;
        bool focused = false;
        int stage = 0;
        std::chrono::steady_clock::time_point start;
    };

    EngineConfig makeConfig()
    {
        EngineConfig config;
        config.width = 1000;
        config.height = 700;
        config.resizable = true;
        return config;
    }
}

GameApp::GameApp(const std::string &appName) : engine(appName, makeConfig())
{
    engine.setSetupFunction([this](EntitySystem& ecs, Window& window)
    {
        auto ttfSys = ecs.createSystem<TTFTextSystem>(window.masterRenderer);
        ttfSys->registerFont("res/font/Inter/static/Inter_24pt-Regular.ttf", "editor");

        ecs.createSystem<TextEditorSystem>();
        ecs.createSystem<DemoSaveSystem>();
        ecs.createSystem<DemoSceneSystem>(static_cast<float>(engine.getConfig().width), static_cast<float>(engine.getConfig().height));
    });
}

GameApp::~GameApp()
{
}

int GameApp::exec()
{
    return engine.exec();
}
