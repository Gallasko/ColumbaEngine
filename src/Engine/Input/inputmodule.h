#pragma once

#include "Interpreter/pginterpreter.h"

#include "Input/sdlevents.h"

#include "Systems/oneventcomponent.h"

#include "Interpreter/scriptcallable.h"

namespace pg
{
    struct SceneElement;

    class ConnectToKeyInput : public Function
    {
        using Function::Function;
    public:
        virtual ~ConnectToKeyInput() {}

        void setUp(EntitySystem *ecsRef)
        {
            LOG_THIS_MEMBER("Input Module");

            setArity(1, 1);

            this->ecsRef = ecsRef;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            LOG_THIS_MEMBER("Input Module");

            auto arg = args.front();
            args.pop();

            if (arg->getType() == "Function")
            {
                auto fun = std::static_pointer_cast<Function>(arg);

                auto ent = ecsRef->createEntity();

                ecsRef->attach<SceneElement>(ent);

                std::shared_ptr<CallableIntepretedFunction> function = makeCallable(fun);

                std::function<void(const OnSDLTextInput&)> f = [function](const OnSDLTextInput& event) {
                    if (not function)
                        return;

                    ValuableQueue queue;
                    auto arg = makeList(function->getRef(), {{"text", event.text}});

                    queue.push(arg);

                    try
                    {
                        function->call(queue);
                    }
                    catch (const std::exception& e)
                    {
                        LOG_ERROR("Input Module", e.what());
                    }
                };

                ecsRef->attach<OnEventComponent>(ent, f);
            }
            else
            {
                LOG_ERROR("Input Module", "Need to pass a function to be able to listen to an event");
            }

            return nullptr; 
        }

    private:
        EntitySystem *ecsRef;
    };

    template<typename Event>
    void addListenerComponent(EntitySystem *ecsRef, std::shared_ptr<CallableIntepretedFunction> function)
    {
        auto ent = ecsRef->createEntity();

        ecsRef->attach<SceneElement>(ent);

        std::function<void(const Event&)> callback = [function](const Event& event) {
            if (not function)
                return;

            ValuableQueue queue;
            auto arg = makeList(function->getRef(), {{"key", event.key}});

            queue.push(arg);

            try
            {
                function->call(queue);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Input Module", e.what());
            }
        };

        ecsRef->attach<OnEventComponent>(ent, callback);
    }

    class ConnectToScancode : public Function
    {
        using Function::Function;
    public:
        virtual ~ConnectToScancode() {}

        void setUp(EntitySystem *ecsRef)
        {
            LOG_THIS_MEMBER("Input Module");

            setArity(1, 1);

            this->ecsRef = ecsRef;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            LOG_THIS_MEMBER("Input Module");

            auto arg = args.front();
            args.pop();

            if (arg->getType() == "Function")
            {
                auto fun = std::static_pointer_cast<Function>(arg);

                std::shared_ptr<CallableIntepretedFunction> function = makeCallable(fun);

                addListenerComponent<OnSDLScanCode>(ecsRef, function);
            }
            else
            {
                LOG_ERROR("Input Module", "Need to pass a function to be able to listen to an event");
            }

            return nullptr; 
        }

    private:
        EntitySystem *ecsRef;
    };

    class ConnectToScancodeReleased : public Function
    {
        using Function::Function;
    public:
        virtual ~ConnectToScancodeReleased() {}

        void setUp(EntitySystem *ecsRef)
        {
            LOG_THIS_MEMBER("Input Module");

            setArity(1, 1);

            this->ecsRef = ecsRef;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            LOG_THIS_MEMBER("Input Module");

            auto arg = args.front();
            args.pop();

            if (arg->getType() == "Function")
            {
                auto fun = std::static_pointer_cast<Function>(arg);

                std::shared_ptr<CallableIntepretedFunction> function = makeCallable(fun);

                addListenerComponent<OnSDLScanCodeReleased>(ecsRef, function);
            }
            else
            {
                LOG_ERROR("Input Module", "Need to pass a function to be able to listen to an event");
            }

            return nullptr; 
        }

    private:
        EntitySystem *ecsRef;
    };

    class ConnectToGamepadPressed : public Function
    {
        using Function::Function;
    private:
        struct GamepadListener
        {
            GamepadListener(std::shared_ptr<Function> func) : function(makeCallable(func)) {}

            void onEvent(const OnSDLGamepadPressed& event)
            {
                if(not function)
                    return;

                ValuableQueue queue;
                auto arg = makeList(function->getRef(), {{"id", static_cast<int>(event.id)}, {"button", static_cast<int>(event.button)}});

                queue.push(arg);

                try
                {
                    function->call(queue);
                }
                catch(const std::exception& e)
                {
                    LOG_ERROR("Input Module", e.what());
                }
            }

            std::shared_ptr<CallableIntepretedFunction> function;
        };

    public:
        virtual ~ConnectToGamepadPressed() { for(auto listener : keyListeners) delete listener; }

        void setUp(ComponentRegistry *registryRef)
        {
            LOG_THIS_MEMBER("Input Module");

            setArity(1, 1);

            this->registryRef = registryRef;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            LOG_THIS_MEMBER("Input Module");

            auto arg = args.front();
            args.pop();

            if(arg->getType() == "Function")
            {
                auto fun = std::static_pointer_cast<Function>(arg);

                auto listener = new GamepadListener(fun);

                registryRef->addEventListener<OnSDLGamepadPressed>(listener);

                keyListeners.push_back(listener);

                // Todo return the id of the listener to be able to remove it later
            }
            else
            {
                LOG_ERROR("Input Module", "Need to pass a function to be able to listen to an event");
            }

            return nullptr; 
        }

    private:
        ComponentRegistry *registryRef;

        mutable std::vector<GamepadListener*> keyListeners;
    };

    class ConnectToGamepadReleased : public Function
    {
        using Function::Function;
    private:
        struct GamepadListener
        {
            GamepadListener(std::shared_ptr<Function> func) : function(makeCallable(func)) {}

            void onEvent(const OnSDLGamepadReleased& event)
            {
                if(not function)
                    return;

                ValuableQueue queue;
                auto arg = makeList(function->getRef(), {{"id", static_cast<int>(event.id)}, {"button", static_cast<int>(event.button)}});

                queue.push(arg);

                try
                {
                    function->call(queue);
                }
                catch(const std::exception& e)
                {
                    LOG_ERROR("Input Module", e.what());
                }
            }

            std::shared_ptr<CallableIntepretedFunction> function;
        };

    public:
        virtual ~ConnectToGamepadReleased() { for(auto listener : keyListeners) delete listener; }

        void setUp(ComponentRegistry *registryRef)
        {
            LOG_THIS_MEMBER("Input Module");

            setArity(1, 1);

            this->registryRef = registryRef;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            LOG_THIS_MEMBER("Input Module");

            auto arg = args.front();
            args.pop();

            if(arg->getType() == "Function")
            {
                auto fun = std::static_pointer_cast<Function>(arg);

                auto listener = new GamepadListener(fun);

                registryRef->addEventListener<OnSDLGamepadReleased>(listener);

                keyListeners.push_back(listener);

                // Todo return the id of the listener to be able to remove it later
            }
            else
            {
                LOG_ERROR("Input Module", "Need to pass a function to be able to listen to an event");
            }

            return nullptr; 
        }

    private:
        ComponentRegistry *registryRef;

        mutable std::vector<GamepadListener*> keyListeners;
    };

    class ConnectToGamepadAxis : public Function
    {
        using Function::Function;
    private:
        struct GamepadListener
        {
            GamepadListener(std::shared_ptr<Function> func) : function(makeCallable(func)) {}

            void onEvent(const OnSDLGamepadAxisChanged& event)
            {
                if(not function)
                    return;

                ValuableQueue queue;
                auto arg = makeList(function->getRef(), {{"id", static_cast<int>(event.id)},
                                                         {"axis", static_cast<int>(event.axis)},
                                                         {"value", static_cast<float>(event.value / (float)INT16_MAX)}});

                queue.push(arg);

                try
                {
                    function->call(queue);
                }
                catch(const std::exception& e)
                {
                    LOG_ERROR("Input Module", e.what());
                }
            }

            std::shared_ptr<CallableIntepretedFunction> function;
        };

    public:
        virtual ~ConnectToGamepadAxis() { for(auto listener : keyListeners) delete listener; }

        void setUp(ComponentRegistry *registryRef)
        {
            LOG_THIS_MEMBER("Input Module");

            setArity(1, 1);

            this->registryRef = registryRef;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            LOG_THIS_MEMBER("Input Module");

            auto arg = args.front();
            args.pop();

            if(arg->getType() == "Function")
            {
                auto fun = std::static_pointer_cast<Function>(arg);

                auto listener = new GamepadListener(fun);

                registryRef->addEventListener<OnSDLGamepadAxisChanged>(listener);

                keyListeners.push_back(listener);

                // Todo return the id of the listener to be able to remove it later
            }
            else
            {
                LOG_ERROR("Input Module", "Need to pass a function to be able to listen to an event");
            }

            return nullptr; 
        }

    private:
        ComponentRegistry *registryRef;

        mutable std::vector<GamepadListener*> keyListeners;
    };

    struct InputModule : public SysModule
    {
        InputModule(EntitySystem *ecsRef)
        {
            LOG_THIS_MEMBER("Core Module");

            addSystemFunction<ConnectToKeyInput>("connectToKeyInput", ecsRef);
            addSystemFunction<ConnectToScancode>("connectToScancodePressed", ecsRef);
            // Todo change this name as it ambiguous
            addSystemFunction<ConnectToScancodeReleased>("connectToScancode", ecsRef);
            addSystemFunction<ConnectToGamepadPressed>("connectToGamepadPressed", &(ecsRef->registry));
            addSystemFunction<ConnectToGamepadReleased>("connectToGamepadReleased", &(ecsRef->registry));
            addSystemFunction<ConnectToGamepadAxis>("connectToGamepadAxis", &(ecsRef->registry));

            addSystemVar("SCANCODE_A", SDL_SCANCODE_A);
            addSystemVar("SCANCODE_B", SDL_SCANCODE_B);
            addSystemVar("SCANCODE_C", SDL_SCANCODE_C);
            addSystemVar("SCANCODE_D", SDL_SCANCODE_D);
            addSystemVar("SCANCODE_E", SDL_SCANCODE_E);
            addSystemVar("SCANCODE_F", SDL_SCANCODE_F);
            addSystemVar("SCANCODE_G", SDL_SCANCODE_G);
            addSystemVar("SCANCODE_H", SDL_SCANCODE_H);
            addSystemVar("SCANCODE_I", SDL_SCANCODE_I);
            addSystemVar("SCANCODE_J", SDL_SCANCODE_J);
            addSystemVar("SCANCODE_K", SDL_SCANCODE_K);
            addSystemVar("SCANCODE_L", SDL_SCANCODE_L);
            addSystemVar("SCANCODE_M", SDL_SCANCODE_M);
            addSystemVar("SCANCODE_N", SDL_SCANCODE_N);
            addSystemVar("SCANCODE_O", SDL_SCANCODE_O);
            addSystemVar("SCANCODE_P", SDL_SCANCODE_P);
            addSystemVar("SCANCODE_Q", SDL_SCANCODE_Q);
            addSystemVar("SCANCODE_R", SDL_SCANCODE_R);
            addSystemVar("SCANCODE_S", SDL_SCANCODE_S);
            addSystemVar("SCANCODE_T", SDL_SCANCODE_T);
            addSystemVar("SCANCODE_U", SDL_SCANCODE_U);
            addSystemVar("SCANCODE_V", SDL_SCANCODE_V);
            addSystemVar("SCANCODE_W", SDL_SCANCODE_W);
            addSystemVar("SCANCODE_X", SDL_SCANCODE_X);
            addSystemVar("SCANCODE_Y", SDL_SCANCODE_Y);
            addSystemVar("SCANCODE_Z", SDL_SCANCODE_Z);
            addSystemVar("SCANCODE_1", SDL_SCANCODE_1);
            addSystemVar("SCANCODE_2", SDL_SCANCODE_2);
            addSystemVar("SCANCODE_3", SDL_SCANCODE_3);
            addSystemVar("SCANCODE_4", SDL_SCANCODE_4);
            addSystemVar("SCANCODE_5", SDL_SCANCODE_5);
            addSystemVar("SCANCODE_6", SDL_SCANCODE_6);
            addSystemVar("SCANCODE_7", SDL_SCANCODE_7);
            addSystemVar("SCANCODE_8", SDL_SCANCODE_8);
            addSystemVar("SCANCODE_9", SDL_SCANCODE_9);
            addSystemVar("SCANCODE_0", SDL_SCANCODE_0);
            addSystemVar("SCANCODE_RETURN", SDL_SCANCODE_RETURN);
            addSystemVar("SCANCODE_ESCAPE", SDL_SCANCODE_ESCAPE);
            addSystemVar("SCANCODE_BACKSPACE", SDL_SCANCODE_BACKSPACE);
            addSystemVar("SCANCODE_TAB", SDL_SCANCODE_TAB);
            addSystemVar("SCANCODE_SPACE", SDL_SCANCODE_SPACE);
            addSystemVar("SCANCODE_MINUS", SDL_SCANCODE_MINUS);
            addSystemVar("SCANCODE_EQUALS", SDL_SCANCODE_EQUALS);
            addSystemVar("SCANCODE_LEFTBRACKET", SDL_SCANCODE_LEFTBRACKET);
            addSystemVar("SCANCODE_RIGHTBRACKET", SDL_SCANCODE_RIGHTBRACKET);
            addSystemVar("SCANCODE_BACKSLASH", SDL_SCANCODE_BACKSLASH);
            addSystemVar("SCANCODE_NONUSHASH", SDL_SCANCODE_NONUSHASH); 
            addSystemVar("SCANCODE_SEMICOLON", SDL_SCANCODE_SEMICOLON);
            addSystemVar("SCANCODE_APOSTROPHE", SDL_SCANCODE_APOSTROPHE);
            addSystemVar("SCANCODE_GRAVE", SDL_SCANCODE_GRAVE); 
            addSystemVar("SCANCODE_COMMA", SDL_SCANCODE_COMMA);
            addSystemVar("SCANCODE_PERIOD", SDL_SCANCODE_PERIOD);
            addSystemVar("SCANCODE_SLASH", SDL_SCANCODE_SLASH);
            addSystemVar("SCANCODE_CAPSLOCK", SDL_SCANCODE_CAPSLOCK);
            addSystemVar("SCANCODE_F1", SDL_SCANCODE_F1);
            addSystemVar("SCANCODE_F2", SDL_SCANCODE_F2);
            addSystemVar("SCANCODE_F3", SDL_SCANCODE_F3);
            addSystemVar("SCANCODE_F4", SDL_SCANCODE_F4);
            addSystemVar("SCANCODE_F5", SDL_SCANCODE_F5);
            addSystemVar("SCANCODE_F6", SDL_SCANCODE_F6);
            addSystemVar("SCANCODE_F7", SDL_SCANCODE_F7);
            addSystemVar("SCANCODE_F8", SDL_SCANCODE_F8);
            addSystemVar("SCANCODE_F9", SDL_SCANCODE_F9);
            addSystemVar("SCANCODE_F10", SDL_SCANCODE_F10);
            addSystemVar("SCANCODE_F11", SDL_SCANCODE_F11);
            addSystemVar("SCANCODE_F12", SDL_SCANCODE_F12);
            addSystemVar("SCANCODE_PRINTSCREEN", SDL_SCANCODE_PRINTSCREEN);
            addSystemVar("SCANCODE_SCROLLLOCK", SDL_SCANCODE_SCROLLLOCK);
            addSystemVar("SCANCODE_PAUSE", SDL_SCANCODE_PAUSE);
            addSystemVar("SCANCODE_INSERT", SDL_SCANCODE_INSERT); 
            addSystemVar("SCANCODE_HOME", SDL_SCANCODE_HOME);
            addSystemVar("SCANCODE_PAGEUP", SDL_SCANCODE_PAGEUP);
            addSystemVar("SCANCODE_DELETE", SDL_SCANCODE_DELETE);
            addSystemVar("SCANCODE_END", SDL_SCANCODE_END);
            addSystemVar("SCANCODE_PAGEDOWN", SDL_SCANCODE_PAGEDOWN);
            addSystemVar("SCANCODE_RIGHT", SDL_SCANCODE_RIGHT);
            addSystemVar("SCANCODE_LEFT", SDL_SCANCODE_LEFT);
            addSystemVar("SCANCODE_DOWN", SDL_SCANCODE_DOWN);
            addSystemVar("SCANCODE_UP", SDL_SCANCODE_UP);
            addSystemVar("SCANCODE_NUMLOCKCLEAR", SDL_SCANCODE_NUMLOCKCLEAR);
            addSystemVar("SCANCODE_KP_DIVIDE", SDL_SCANCODE_KP_DIVIDE);
            addSystemVar("SCANCODE_KP_MULTIPLY", SDL_SCANCODE_KP_MULTIPLY);
            addSystemVar("SCANCODE_KP_MINUS", SDL_SCANCODE_KP_MINUS);
            addSystemVar("SCANCODE_KP_PLUS", SDL_SCANCODE_KP_PLUS);
            addSystemVar("SCANCODE_KP_ENTER", SDL_SCANCODE_KP_ENTER);
            addSystemVar("SCANCODE_KP_1", SDL_SCANCODE_KP_1);
            addSystemVar("SCANCODE_KP_2", SDL_SCANCODE_KP_2);
            addSystemVar("SCANCODE_KP_3", SDL_SCANCODE_KP_3);
            addSystemVar("SCANCODE_KP_4", SDL_SCANCODE_KP_4);
            addSystemVar("SCANCODE_KP_5", SDL_SCANCODE_KP_5);
            addSystemVar("SCANCODE_KP_6", SDL_SCANCODE_KP_6);
            addSystemVar("SCANCODE_KP_7", SDL_SCANCODE_KP_7);
            addSystemVar("SCANCODE_KP_8", SDL_SCANCODE_KP_8);
            addSystemVar("SCANCODE_KP_9", SDL_SCANCODE_KP_9);
            addSystemVar("SCANCODE_KP_0", SDL_SCANCODE_KP_0);
            addSystemVar("SCANCODE_KP_PERIOD", SDL_SCANCODE_KP_PERIOD);
            addSystemVar("SCANCODE_NONUSBACKSLASH", SDL_SCANCODE_NONUSBACKSLASH);
            addSystemVar("SCANCODE_APPLICATION", SDL_SCANCODE_APPLICATION);
            addSystemVar("SCANCODE_POWER", SDL_SCANCODE_POWER);
            addSystemVar("SCANCODE_KP_EQUALS", SDL_SCANCODE_KP_EQUALS);
            addSystemVar("SCANCODE_F13", SDL_SCANCODE_F13);
            addSystemVar("SCANCODE_F14", SDL_SCANCODE_F14);
            addSystemVar("SCANCODE_F15", SDL_SCANCODE_F15);
            addSystemVar("SCANCODE_F16", SDL_SCANCODE_F16);
            addSystemVar("SCANCODE_F17", SDL_SCANCODE_F17);
            addSystemVar("SCANCODE_F18", SDL_SCANCODE_F18);
            addSystemVar("SCANCODE_F19", SDL_SCANCODE_F19);
            addSystemVar("SCANCODE_F20", SDL_SCANCODE_F20);
            addSystemVar("SCANCODE_F21", SDL_SCANCODE_F21);
            addSystemVar("SCANCODE_F22", SDL_SCANCODE_F22);
            addSystemVar("SCANCODE_F23", SDL_SCANCODE_F23);
            addSystemVar("SCANCODE_F24", SDL_SCANCODE_F24);
            addSystemVar("SCANCODE_EXECUTE", SDL_SCANCODE_EXECUTE);
            addSystemVar("SCANCODE_HELP", SDL_SCANCODE_HELP);
            addSystemVar("SCANCODE_MENU", SDL_SCANCODE_MENU);
            addSystemVar("SCANCODE_SELECT", SDL_SCANCODE_SELECT);
            addSystemVar("SCANCODE_STOP", SDL_SCANCODE_STOP);
            addSystemVar("SCANCODE_AGAIN", SDL_SCANCODE_AGAIN);
            addSystemVar("SCANCODE_UNDO", SDL_SCANCODE_UNDO);
            addSystemVar("SCANCODE_CUT", SDL_SCANCODE_CUT);
            addSystemVar("SCANCODE_COPY", SDL_SCANCODE_COPY);
            addSystemVar("SCANCODE_PASTE", SDL_SCANCODE_PASTE);
            addSystemVar("SCANCODE_FIND", SDL_SCANCODE_FIND);
            addSystemVar("SCANCODE_MUTE", SDL_SCANCODE_MUTE);
            addSystemVar("SCANCODE_VOLUMEUP", SDL_SCANCODE_VOLUMEUP);
            addSystemVar("SCANCODE_VOLUMEDOWN", SDL_SCANCODE_VOLUMEDOWN);
            addSystemVar("SCANCODE_KP_COMMA", SDL_SCANCODE_KP_COMMA);
            addSystemVar("SCANCODE_KP_EQUALSAS400", SDL_SCANCODE_KP_EQUALSAS400);
            addSystemVar("SCANCODE_INTERNATIONAL1", SDL_SCANCODE_INTERNATIONAL1);
            addSystemVar("SCANCODE_INTERNATIONAL2", SDL_SCANCODE_INTERNATIONAL2);
            addSystemVar("SCANCODE_INTERNATIONAL3", SDL_SCANCODE_INTERNATIONAL3);
            addSystemVar("SCANCODE_INTERNATIONAL4", SDL_SCANCODE_INTERNATIONAL4);
            addSystemVar("SCANCODE_INTERNATIONAL5", SDL_SCANCODE_INTERNATIONAL5);
            addSystemVar("SCANCODE_INTERNATIONAL6", SDL_SCANCODE_INTERNATIONAL6);
            addSystemVar("SCANCODE_INTERNATIONAL7", SDL_SCANCODE_INTERNATIONAL7);
            addSystemVar("SCANCODE_INTERNATIONAL8", SDL_SCANCODE_INTERNATIONAL8);
            addSystemVar("SCANCODE_INTERNATIONAL9", SDL_SCANCODE_INTERNATIONAL9);
            addSystemVar("SCANCODE_LANG1", SDL_SCANCODE_LANG1);
            addSystemVar("SCANCODE_LANG2", SDL_SCANCODE_LANG2);
            addSystemVar("SCANCODE_LANG3", SDL_SCANCODE_LANG3);
            addSystemVar("SCANCODE_LANG4", SDL_SCANCODE_LANG4);
            addSystemVar("SCANCODE_LANG5", SDL_SCANCODE_LANG5);
            addSystemVar("SCANCODE_LANG6", SDL_SCANCODE_LANG6);
            addSystemVar("SCANCODE_LANG7", SDL_SCANCODE_LANG7);
            addSystemVar("SCANCODE_LANG8", SDL_SCANCODE_LANG8);
            addSystemVar("SCANCODE_LANG9", SDL_SCANCODE_LANG9);
            addSystemVar("SCANCODE_ALTERASE", SDL_SCANCODE_ALTERASE);
            addSystemVar("SCANCODE_SYSREQ", SDL_SCANCODE_SYSREQ);
            addSystemVar("SCANCODE_CANCEL", SDL_SCANCODE_CANCEL);
            addSystemVar("SCANCODE_CLEAR", SDL_SCANCODE_CLEAR);
            addSystemVar("SCANCODE_PRIOR", SDL_SCANCODE_PRIOR);
            addSystemVar("SCANCODE_RETURN2", SDL_SCANCODE_RETURN2);
            addSystemVar("SCANCODE_SEPARATOR", SDL_SCANCODE_SEPARATOR);
            addSystemVar("SCANCODE_OUT", SDL_SCANCODE_OUT);
            addSystemVar("SCANCODE_OPER", SDL_SCANCODE_OPER);
            addSystemVar("SCANCODE_CLEARAGAIN", SDL_SCANCODE_CLEARAGAIN);
            addSystemVar("SCANCODE_CRSEL", SDL_SCANCODE_CRSEL);
            addSystemVar("SCANCODE_EXSEL", SDL_SCANCODE_EXSEL);
            addSystemVar("SCANCODE_KP_00", SDL_SCANCODE_KP_00);
            addSystemVar("SCANCODE_KP_000", SDL_SCANCODE_KP_000);
            addSystemVar("SCANCODE_THOUSANDSSEPARATOR", SDL_SCANCODE_THOUSANDSSEPARATOR);
            addSystemVar("SCANCODE_DECIMALSEPARATOR", SDL_SCANCODE_DECIMALSEPARATOR);
            addSystemVar("SCANCODE_CURRENCYUNIT", SDL_SCANCODE_CURRENCYUNIT);
            addSystemVar("SCANCODE_CURRENCYSUBUNIT", SDL_SCANCODE_CURRENCYSUBUNIT);
            addSystemVar("SCANCODE_KP_LEFTPAREN", SDL_SCANCODE_KP_LEFTPAREN);
            addSystemVar("SCANCODE_KP_RIGHTPAREN", SDL_SCANCODE_KP_RIGHTPAREN);
            addSystemVar("SCANCODE_KP_LEFTBRACE", SDL_SCANCODE_KP_LEFTBRACE);
            addSystemVar("SCANCODE_KP_RIGHTBRACE", SDL_SCANCODE_KP_RIGHTBRACE);
            addSystemVar("SCANCODE_KP_TAB", SDL_SCANCODE_KP_TAB);
            addSystemVar("SCANCODE_KP_BACKSPACE", SDL_SCANCODE_KP_BACKSPACE);
            addSystemVar("SCANCODE_KP_A", SDL_SCANCODE_KP_A);
            addSystemVar("SCANCODE_KP_B", SDL_SCANCODE_KP_B);
            addSystemVar("SCANCODE_KP_C", SDL_SCANCODE_KP_C);
            addSystemVar("SCANCODE_KP_D", SDL_SCANCODE_KP_D);
            addSystemVar("SCANCODE_KP_E", SDL_SCANCODE_KP_E);
            addSystemVar("SCANCODE_KP_F", SDL_SCANCODE_KP_F);
            addSystemVar("SCANCODE_KP_XOR", SDL_SCANCODE_KP_XOR);
            addSystemVar("SCANCODE_KP_POWER", SDL_SCANCODE_KP_POWER);
            addSystemVar("SCANCODE_KP_PERCENT", SDL_SCANCODE_KP_PERCENT);
            addSystemVar("SCANCODE_KP_LESS", SDL_SCANCODE_KP_LESS);
            addSystemVar("SCANCODE_KP_GREATER", SDL_SCANCODE_KP_GREATER);
            addSystemVar("SCANCODE_KP_AMPERSAND", SDL_SCANCODE_KP_AMPERSAND);
            addSystemVar("SCANCODE_KP_DBLAMPERSAND", SDL_SCANCODE_KP_DBLAMPERSAND);
            addSystemVar("SCANCODE_KP_VERTICALBAR", SDL_SCANCODE_KP_VERTICALBAR);
            addSystemVar("SCANCODE_KP_DBLVERTICALBAR", SDL_SCANCODE_KP_DBLVERTICALBAR);
            addSystemVar("SCANCODE_KP_COLON", SDL_SCANCODE_KP_COLON);
            addSystemVar("SCANCODE_KP_HASH", SDL_SCANCODE_KP_HASH);
            addSystemVar("SCANCODE_KP_SPACE", SDL_SCANCODE_KP_SPACE);
            addSystemVar("SCANCODE_KP_AT", SDL_SCANCODE_KP_AT);
            addSystemVar("SCANCODE_KP_EXCLAM", SDL_SCANCODE_KP_EXCLAM);
            addSystemVar("SCANCODE_KP_MEMSTORE", SDL_SCANCODE_KP_MEMSTORE);
            addSystemVar("SCANCODE_KP_MEMRECALL", SDL_SCANCODE_KP_MEMRECALL);
            addSystemVar("SCANCODE_KP_MEMCLEAR", SDL_SCANCODE_KP_MEMCLEAR);
            addSystemVar("SCANCODE_KP_MEMADD", SDL_SCANCODE_KP_MEMADD);
            addSystemVar("SCANCODE_KP_MEMSUBTRACT", SDL_SCANCODE_KP_MEMSUBTRACT);
            addSystemVar("SCANCODE_KP_MEMMULTIPLY", SDL_SCANCODE_KP_MEMMULTIPLY);
            addSystemVar("SCANCODE_KP_MEMDIVIDE", SDL_SCANCODE_KP_MEMDIVIDE);
            addSystemVar("SCANCODE_KP_PLUSMINUS", SDL_SCANCODE_KP_PLUSMINUS);
            addSystemVar("SCANCODE_KP_CLEAR", SDL_SCANCODE_KP_CLEAR);
            addSystemVar("SCANCODE_KP_CLEARENTRY", SDL_SCANCODE_KP_CLEARENTRY);
            addSystemVar("SCANCODE_KP_BINARY", SDL_SCANCODE_KP_BINARY);
            addSystemVar("SCANCODE_KP_OCTAL", SDL_SCANCODE_KP_OCTAL);
            addSystemVar("SCANCODE_KP_DECIMAL", SDL_SCANCODE_KP_DECIMAL);
            addSystemVar("SCANCODE_KP_HEXADECIMAL", SDL_SCANCODE_KP_HEXADECIMAL);
            addSystemVar("SCANCODE_LCTRL", SDL_SCANCODE_LCTRL);
            addSystemVar("SCANCODE_LSHIFT", SDL_SCANCODE_LSHIFT);
            addSystemVar("SCANCODE_LALT", SDL_SCANCODE_LALT);
            addSystemVar("SCANCODE_LGUI", SDL_SCANCODE_LGUI);
            addSystemVar("SCANCODE_RCTRL", SDL_SCANCODE_RCTRL);
            addSystemVar("SCANCODE_RSHIFT", SDL_SCANCODE_RSHIFT);
            addSystemVar("SCANCODE_RALT", SDL_SCANCODE_RALT);
            addSystemVar("SCANCODE_RGUI", SDL_SCANCODE_RGUI);
            addSystemVar("SCANCODE_MODE", SDL_SCANCODE_MODE);
            addSystemVar("SCANCODE_AUDIONEXT", SDL_SCANCODE_AUDIONEXT);
            addSystemVar("SCANCODE_AUDIOPREV", SDL_SCANCODE_AUDIOPREV);
            addSystemVar("SCANCODE_AUDIOSTOP", SDL_SCANCODE_AUDIOSTOP);
            addSystemVar("SCANCODE_AUDIOPLAY", SDL_SCANCODE_AUDIOPLAY);
            addSystemVar("SCANCODE_AUDIOMUTE", SDL_SCANCODE_AUDIOMUTE);
            addSystemVar("SCANCODE_MEDIASELECT", SDL_SCANCODE_MEDIASELECT);
            addSystemVar("SCANCODE_WWW", SDL_SCANCODE_WWW);
            addSystemVar("SCANCODE_MAIL", SDL_SCANCODE_MAIL);
            addSystemVar("SCANCODE_CALCULATOR", SDL_SCANCODE_CALCULATOR);
            addSystemVar("SCANCODE_COMPUTER", SDL_SCANCODE_COMPUTER);
            addSystemVar("SCANCODE_AC_SEARCH", SDL_SCANCODE_AC_SEARCH);
            addSystemVar("SCANCODE_AC_HOME", SDL_SCANCODE_AC_HOME);
            addSystemVar("SCANCODE_AC_BACK", SDL_SCANCODE_AC_BACK);
            addSystemVar("SCANCODE_AC_FORWARD", SDL_SCANCODE_AC_FORWARD);
            addSystemVar("SCANCODE_AC_STOP", SDL_SCANCODE_AC_STOP);
            addSystemVar("SCANCODE_AC_REFRESH", SDL_SCANCODE_AC_REFRESH);
            addSystemVar("SCANCODE_AC_BOOKMARKS", SDL_SCANCODE_AC_BOOKMARKS);
            addSystemVar("SCANCODE_BRIGHTNESSDOWN", SDL_SCANCODE_BRIGHTNESSDOWN);
            addSystemVar("SCANCODE_BRIGHTNESSUP", SDL_SCANCODE_BRIGHTNESSUP);
            addSystemVar("SCANCODE_DISPLAYSWITCH", SDL_SCANCODE_DISPLAYSWITCH);
            addSystemVar("SCANCODE_KBDILLUMTOGGLE", SDL_SCANCODE_KBDILLUMTOGGLE);
            addSystemVar("SCANCODE_KBDILLUMDOWN", SDL_SCANCODE_KBDILLUMDOWN);
            addSystemVar("SCANCODE_KBDILLUMUP", SDL_SCANCODE_KBDILLUMUP);
            addSystemVar("SCANCODE_EJECT", SDL_SCANCODE_EJECT);
            addSystemVar("SCANCODE_SLEEP", SDL_SCANCODE_SLEEP);
            addSystemVar("SCANCODE_APP1", SDL_SCANCODE_APP1);
            addSystemVar("SCANCODE_APP2", SDL_SCANCODE_APP2);
            addSystemVar("SCANCODE_AUDIOREWIND", SDL_SCANCODE_AUDIOREWIND);
            addSystemVar("SCANCODE_AUDIOFASTFORWARD", SDL_SCANCODE_AUDIOFASTFORWARD);

            addSystemVar("GAMEPAD_A", SDL_CONTROLLER_BUTTON_A);
            addSystemVar("GAMEPAD_B", SDL_CONTROLLER_BUTTON_B);
            addSystemVar("GAMEPAD_X", SDL_CONTROLLER_BUTTON_X);
            addSystemVar("GAMEPAD_Y", SDL_CONTROLLER_BUTTON_Y);
            addSystemVar("GAMEPAD_BACK", SDL_CONTROLLER_BUTTON_BACK);
            addSystemVar("GAMEPAD_GUIDE", SDL_CONTROLLER_BUTTON_GUIDE);
            addSystemVar("GAMEPAD_START", SDL_CONTROLLER_BUTTON_START);
            addSystemVar("GAMEPAD_LEFTSTICK", SDL_CONTROLLER_BUTTON_LEFTSTICK);
            addSystemVar("GAMEPAD_RIGHTSTICK", SDL_CONTROLLER_BUTTON_RIGHTSTICK);
            addSystemVar("GAMEPAD_LEFTSHOULDER", SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            addSystemVar("GAMEPAD_RIGHTSHOULDER", SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            addSystemVar("GAMEPAD_DPAD_UP", SDL_CONTROLLER_BUTTON_DPAD_UP);
            addSystemVar("GAMEPAD_DPAD_DOWN", SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            addSystemVar("GAMEPAD_DPAD_LEFT", SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            addSystemVar("GAMEPAD_DPAD_RIGHT", SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            addSystemVar("GAMEPAD_MAX", SDL_CONTROLLER_BUTTON_MAX);

            addSystemVar("GAMEPAD_AXIS_LEFTX", SDL_CONTROLLER_AXIS_LEFTX);
            addSystemVar("GAMEPAD_AXIS_LEFTY", SDL_CONTROLLER_AXIS_LEFTY);
            addSystemVar("GAMEPAD_AXIS_RIGHTX", SDL_CONTROLLER_AXIS_RIGHTX);
            addSystemVar("GAMEPAD_AXIS_RIGHTY", SDL_CONTROLLER_AXIS_RIGHTY);
            addSystemVar("GAMEPAD_AXIS_TRIGGERLEFT", SDL_CONTROLLER_AXIS_TRIGGERLEFT);
            addSystemVar("GAMEPAD_AXIS_TRIGGERRIGHT", SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
            addSystemVar("GAMEPAD_AXIS_MAX", SDL_CONTROLLER_AXIS_MAX);
        }
    };

}