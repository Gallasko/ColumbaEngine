#include "gl_debug.hpp"

#include "stdafx.h"

#include "window.h"

#include <string>
#include <chrono>

#include "ECS/loggersystem.h"
#include "ECS/ecsmodule.h"

#include "Renderer/renderer.h"
#include "Renderer/renderermodule.h"

#include "Input/input.h"
#include "Input/inputmodule.h"

#include "UI/uisystem.h"
#include "UI/focusable.h"
#include "UI/progressbar.h"
#include "UI/textinput.h"
#include "UI/sentencesystem.h"
#include "UI/listview.h"
#include "UI/prefab.h"

#include "2D/position.h"
#include "2D/simple2dobject.h"
#include "2D/texture.h"

#include "Scene/scenemanager.h"

#include "Interpreter/pginterpreter.h"
#include "Interpreter/systemfunction.h"

#include "Systems/coremodule.h"
#include "Systems/logmodule.h"
#include "Systems/oneventcomponent.h"
#include "Systems/shape2Dmodule.h"
#include "Systems/timemodule.h"
#include "Systems/sentencemodule.h"
#include "Systems/texture2Dmodule.h"
#include "Systems/scenemodule.h"

#include "Audio/audiosystem.h"
#include "Audio/audiomodule.h"

// #include "GameElements/Systems/basicsystems.h"

#include "logger.h"
#include "serialization.h"

namespace pg
{
    namespace
    {
        static const char * const DOM = "Window";

        class OpenTextFileFunction : public Function
        {
            using Function::Function;
        public:
            void setUp()
            {
                setArity(1, 1);
            }

            virtual ValuablePtr call(ValuableQueue& args) override
            {
                auto name = args.front()->getElement();
                args.pop();

                auto file = UniversalFileAccessor::openTextFile(name.toString());

                return makeVar(file.data);
            }
        };

        class OpenTextFolderFunction : public Function
        {
            using Function::Function;
        public:
            void setUp()
            {
                setArity(1, 1);
            }

            virtual ValuablePtr call(ValuableQueue& args) override
            {
                auto name = args.front()->getElement();
                args.pop();

                auto folder = UniversalFileAccessor::openTextFolder(name.toString());

                auto list = makeList(this, {});

                for (auto file : folder)
                {
                    addToList(list, token, {file.filepath, file.data});
                }

                return list; 
            }
        };

        struct FileModule : public SysModule
        {
            FileModule()
            {
                addSystemFunction<OpenTextFileFunction>("openTextFile");
                addSystemFunction<OpenTextFolderFunction>("openTextFolder");
            }
        };

        void getAllControllers(Input *inputHandler)
        {
            for (int i = 0; i < SDL_NumJoysticks(); i++)
            {
                if (SDL_IsGameController(i))
                {
                    inputHandler->addGamepad(SDL_GameControllerOpen(i));
                }
            }
        }

    }

    // Todo better define save path
    Window::Window(const std::string &title, const std::string& savePath) : ecs(savePath), title(title)
    {
        LOG_THIS_MEMBER(DOM);

        LOG_INFO(DOM, "Window creation...");

        inputHandler = new Input();

        LOG_INFO(DOM, "Initializing interpreter");

        // [Start] Interpreter definition
        interpreter = ecs.createSystem<PgInterpreter>();

        interpreter->addSystemFunction<TestPrint>("print");
        interpreter->addSystemFunction<DebugPrint>("debugPrint");
        interpreter->addSystemFunction<ToString>("toString");

        interpreter->addSystemModule("log", LogModule{});
        interpreter->addSystemModule("ui", UiModule{&ecs});
        interpreter->addSystemModule("2Dshapes", Shape2DModule{&ecs});
        interpreter->addSystemModule("2Dtexture", Texture2DModule{&ecs});
        interpreter->addSystemModule("time", TimeModule{&ecs});
        interpreter->addSystemModule("ecs", EcsModule{&ecs});
        interpreter->addSystemModule("core", CoreModule{&ecs});
        interpreter->addSystemModule("input", InputModule{&ecs});
        interpreter->addSystemModule("uitext", SentenceModule{&ecs});
        interpreter->addSystemModule("scene", SceneModule{&ecs});
        interpreter->addSystemModule("audio", AudioModule{&ecs});
        
        // Script to configure the logger
        interpreter->interpretFromFile("logManager.pg");
        // [End] Interpreter definition

        LOG_INFO(DOM, "Window creation done");
    }

    Window::~Window()
    {
        LOG_THIS_MEMBER(DOM);

        LOG_INFO(DOM, "Window destruction...");

        ecs.stop();

        LOG_INFO(DOM, "ECS stopped");

        if (inputHandler != nullptr)
        {
            inputHandler->clearGamepads();

            delete inputHandler;
        }

        if (audioSystem != nullptr)
            audioSystem->closeSDLMixer();

        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();

        LOG_INFO(DOM, "Window destruction done");
    }

    bool Window::init(int width, int height, bool isFullscreen, SDL_Window* sdlWindow)
    {
        LOG_THIS_MEMBER(DOM);

        this->width = width;
        this->height = height;

        // Todo put back SDL_WINDOW_RESIZABLE flag
        int flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
        // int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;

        if (isFullscreen)
        {
            flags = SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
        }

        LOG_INFO(DOM, "Initializing SDL...");

        // if (SDL_Init(SDL_INIT_EVERYTHING) == 0)
        if (not (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) == 0))
        {
            LOG_ERROR(DOM, "SDL initialisation failed");
            LOG_ERROR(DOM, SDL_GetError());
            
            return false;
        }

        LOG_INFO(DOM, "SDL initialized successfully");

        // Enable file dropping
        SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

#ifdef __EMSCRIPTEN__
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

        LOG_INFO(DOM, "Creating WindowSDL...");

        if (sdlWindow)
        {
            window = sdlWindow;
        }
        else
        {
            window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
        }

        if (window != nullptr)
        {
            LOG_INFO(DOM, "WindowSDL initialised");
        }
        else
        {
            LOG_ERROR(DOM, "WindowSDL init failed");
            return false;
        }

        LOG_INFO(DOM, "Creating OpenGL context...");
                
        // OpenGL context
        context = SDL_GL_CreateContext(window);

        if (context)
        {
            LOG_INFO(DOM, "OpenGL Context initialised");
        }
        else
        {
            LOG_ERROR(DOM, "OpenGL Context init failed: " << SDL_GetError());
            return false;
        }

        // OpenGL setup
        glewExperimental = GL_TRUE;
        GLenum initGLEW(glewInit());

        if (initGLEW == GLEW_OK)
        {
            LOG_INFO(DOM, "GLEW initialised");
        }
        else
        {
            LOG_ERROR(DOM, "GLEW init failed");
            return false;
        }

        LOG_INFO(DOM, "Getting all connected controllers...");

        getAllControllers(inputHandler);

        LOG_INFO(DOM, "Got all connected controllers");

#ifndef __EMSCRIPTEN__
        // Get graphics info
        const GLubyte *renderer = glGetString(GL_RENDERER);
        const GLubyte *version = glGetString(GL_VERSION);

        LOG_INFO(DOM, "Renderer: " << renderer);
        LOG_INFO(DOM, "OpenGL version supported " << version);
#endif
            
        LOG_INFO(DOM, "Setting gl functions...");
        // glViewport(0, 0, width, height);
        // Todo set this or not
        // glEnable(GL_CULL_FACE);
        printf("Enable cull face");
        // glDisable(GL_CULL_FACE);
        LOG_INFO(DOM, "Disable cull face");
            // printf("Enable cull face");
#ifndef __EMSCRIPTEN__
        // glEnable(GL_DEPTH_TEST);
#endif
        LOG_INFO(DOM, "Enable depth testing");
        glEnable(GL_ALPHA_TEST);
        LOG_INFO(DOM, "Enable alpha testing");
        glEnable(GL_BLEND);
        LOG_INFO(DOM, "Enable blending");
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        LOG_INFO(DOM, "GL functions set");

#ifndef __EMSCRIPTEN__
        if (glDebugMessageControlARB != NULL)
        {
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

            glDebugMessageCallback((GLDEBUGPROCARB)debugGlErrorCallback, NULL);
            GLuint unusedIds = 0;
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, &unusedIds, GL_TRUE);
        }
#endif
        return true;
    }

    bool Window::initEngine()
    {
        LOG_THIS_MEMBER(DOM);

        // glViewport(0, 0, width, height);

        ecs.createSystem<EntityNameSystem>();

        ecs.createSystem<TickingSystem>();

        ecs.createSystem<TimerSystem>();

        ecs.createSystem<TerminalLogSystem>();

        ecs.createSystem<FocusableSystem>();

        // [Start] Master render definition

        masterRenderer = ecs.createSystem<MasterRenderer>("res/None.png");
        interpreter->addSystemModule("renderer", RendererModule{masterRenderer});

        // Configure the master renderer system
        interpreter->interpretFromFile("setupRenderer.pg");

        masterRenderer->setWindowSize(width, height);

        // [End] Master render definition

        ecs.createSystem<OnEventComponentSystem>();

        ecs.createSystem<UiComponentSystem>();

        ecs.createSystem<PositionComponentSystem>();

        ecs.createSystem<Simple2DObjectSystem>(masterRenderer);

        ecs.createSystem<Texture2DComponentSystem>(masterRenderer);

        ecs.createSystem<ProgressBarComponentSystem>(masterRenderer);

        ecs.createSystem<SentenceSystem>(masterRenderer, "res/font/fontmap.ft");

        // Todo fix for emscripten
        audioSystem = ecs.createSystem<AudioSystem>();

        // ecs.createSystem<FpsSystem>();

        ecs.createSystem<MouseClickSystem>(inputHandler);

        ecs.createSystem<MouseLeaveClickSystem>(inputHandler);
        
        ecs.createSystem<TextInputSystem>(inputHandler);

        ecs.createSystem<RunScriptFromTextInputSystem>();

        ecs.createSystem<SceneElementSystem>();

        ecs.createSystem<PrefabSystem>();

        ecs.createSystem<ListViewSystem>();

        // Ecs task scheduling

        ecs.succeed<TickingSystem, PgInterpreter>();

        ecs.succeed<TimerSystem, TickingSystem>();

        ecs.succeed<MouseClickSystem, TickingSystem>();

        ecs.succeed<UiComponentSystem, PrefabSystem>();
        ecs.succeed<UiComponentSystem, MouseClickSystem>();

        ecs.succeed<PositionComponentSystem, ProgressBarComponentSystem>();

        // Todo make all derived class from AbstractRenderer automaticly run before MasterRenderer
        ecs.succeed<MasterRenderer, Simple2DObjectSystem>();
        ecs.succeed<MasterRenderer, Texture2DComponentSystem>();
        ecs.succeed<MasterRenderer, SentenceSystem>();
        ecs.succeed<MasterRenderer, ProgressBarComponentSystem>();
        ecs.succeed<MasterRenderer, PrefabSystem>();

        ecs.succeed<MasterRenderer, UiComponentSystem>();
        ecs.succeed<MasterRenderer, PositionComponentSystem>();

        ecs.succeed<SceneElementSystem, MasterRenderer>();

        // Script to configure all the users systems
        interpreter->interpretFromFile("sysRegister.pg");

        // Log taskflow for this window
        ecs.dumbTaskflow();

        screenEntity = ecs.createEntity();
        screenUi = ecs.attach<UiComponent>(screenEntity);
        screenUi->width = width;
        screenUi->height = height;
        screenUi->setZ(-1);

        ecs.attach<FocusableComponent>(screenEntity);

        ecs.attach<MouseLeftClickComponent>(screenEntity, makeCallable<OnFocus>(screenEntity.id));

        screenUi->update();

        ecs.attach<EntityName>(screenEntity, "__MainWindow");

        return true;
    }

    void Window::processEvents(const SDL_Event& event)
    {
        switch(event.type)
        {
            case SDL_WINDOWEVENT:
                switch (event.window.event)
                {
                    // User clicked on 'x' to close the window
                    case SDL_WINDOWEVENT_CLOSE:
                        LOG_INFO(DOM, "User wants to close the window, quiting ...");
                        needToQuit = true;
                        break;
                    // User resized the window
                    case SDL_WINDOWEVENT_RESIZED:
                        LOG_INFO(DOM, "MESSAGE: Resizing window... New width: " << event.window.data1 << ", new height: " << event.window.data2);

                        this->resize(event.window.data1, event.window.data2);
                        break;
                    default:
                        break;
                }
                break;

            case SDL_DROPFILE:
            {
                char* dropFile = event.drop.file;
                LOG_INFO(DOM, "User dropped a file: "<< dropFile);

                ecs.sendEvent(DropFileEvent{dropFile});

                SDL_free(dropFile);
                break;
            }

            case SDL_KEYUP:
                inputHandler->registerKeyInput(event.key.keysym.scancode, Input::InputState::KEYRELEASED);
                ecs.sendEvent(OnSDLScanCodeReleased{event.key.keysym.scancode});
                break;

            case SDL_KEYDOWN:
                LOG_MILE(DOM, "Key pressed : " << event.key.keysym.scancode);
                inputHandler->registerKeyInput(event.key.keysym.scancode, Input::InputState::KEYPRESSED);
                ecs.sendEvent(OnSDLScanCode{event.key.keysym.scancode});
                break;
            
            case SDL_MOUSEBUTTONDOWN:
                LOG_MILE(DOM, "Button pressed : " << event.button.button);
                inputHandler->registerMouseInput(event.button.button, Input::InputState::MOUSEPRESS);
                break;

            case SDL_MOUSEBUTTONUP:
                inputHandler->registerMouseInput(event.button.button, Input::InputState::MOUSERELEASE);
                break;

            case SDL_MOUSEMOTION:
                {
                    MousePos currentPos {static_cast<float>(event.motion.x), static_cast<float>(event.motion.y)};
                    MousePos mouseDelta {(mousePos.x - currentPos.x) * xSensitivity, (currentPos.y - mousePos.y) * ySensitivity};

                    inputHandler->registerMouseMove(currentPos, mouseDelta);

                    mousePos = currentPos;
                }
                break;
            
            case SDL_CONTROLLERDEVICEADDED:
                // Todo
                LOG_INFO(DOM, "Gamepad added !");
                break;

            case SDL_CONTROLLERDEVICEREMOVED:
                // Todo
                LOG_INFO(DOM, "Gamepad removed !");
                break;

            case SDL_CONTROLLERBUTTONDOWN:
                LOG_INFO(DOM, "Gamepad Button pressed : " << event.cbutton.button);
                ecs.sendEvent(OnSDLGamepadPressed{event.cbutton.which, event.cbutton.button});
                break;

            case SDL_CONTROLLERBUTTONUP:
                ecs.sendEvent(OnSDLGamepadReleased{event.cbutton.which, event.cbutton.button});
                break;

            case SDL_CONTROLLERAXISMOTION:
                ecs.sendEvent(OnSDLGamepadAxisChanged{event.caxis.which, event.caxis.axis, event.caxis.value});
                break;

            case SDL_TEXTINPUT:
                LOG_MILE(DOM, "MESSAGE: Text input: " << std::string(event.text.text));
                ecs.sendEvent(OnSDLTextInput{std::string(event.text.text)});
                break;
        }
    }

    void Window::resize(int width, int height)
    {
        LOG_THIS_MEMBER(DOM);

        this->width = width;
        this->height = height;

        std::lock_guard<std::mutex> lock(renderMutex);

        glViewport(0, 0, width, height);

        if (screenUi->width != width)
        {
            screenUi->setWidth(width);
        }
        
        if (screenUi->height != height)
        {
            screenUi->setHeight(height);
        }

        ecs.sendEvent(ResizeEvent{static_cast<float>(width), static_cast<float>(height)});
    }

    void Window::render()
    {
        currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        static auto lastTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        std::lock_guard<std::mutex> lock(renderMutex);

        // SDL_GL_MakeCurrent(window, context);

        glClearColor(0.0513f, 0.0501f, 0.123f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        masterRenderer->setCurrentTime(currentTime);

        masterRenderer->setWindowSize(this->width, this->height);

        masterRenderer->renderAll();

        inputHandler->updateInput(float(currentTime - lastTime) / 1000);

        lastTime = currentTime;

        nbFrame++;

        swapBuffer();
    }

    void Window::swapBuffer()
    {
        // Check OpenGL error
        GLenum err;
        while ((err = glGetError()) != GL_NO_ERROR)
        {
            LOG_ERROR(DOM, "OpenGL error: " << err);
        }

        // VSync 0 to disable 1 to activate
#ifndef __EMSCRIPTEN__
        SDL_GL_SetSwapInterval(0);
#endif

        SDL_GL_SwapWindow(window);
    }
}