#pragma once

#include "UI/thememanager.h"
#include "Interpreter/pginterpreter.h"
#include "logger.h"

namespace pg
{
    // Theme creation function
    class ThemeCreate : public Function
    {
        using Function::Function;
    public:
        void setUp(ThemeManager* manager)
        {
            setArity(2, 2); // name, basedOn
            this->themeManager = manager;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto name = args.front()->getElement().get<std::string>();
            args.pop();
            auto basedOn = args.front()->getElement().get<std::string>();
            args.pop();

            bool result = themeManager->createTheme(name, basedOn);
            return makeVar(result);
        }

    private:
        ThemeManager* themeManager = nullptr;
    };

    // Theme deletion function
    class ThemeDelete : public Function
    {
        using Function::Function;
    public:
        void setUp(ThemeManager* manager)
        {
            setArity(1, 1); // name
            this->themeManager = manager;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto name = args.front()->getElement().get<std::string>();
            args.pop();

            bool result = themeManager->deleteTheme(name);
            return makeVar(result);
        }

    private:
        ThemeManager* themeManager = nullptr;
    };

    // Theme duplication function
    class ThemeDuplicate : public Function
    {
        using Function::Function;
    public:
        void setUp(ThemeManager* manager)
        {
            setArity(2, 2); // source, newName
            this->themeManager = manager;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto source = args.front()->getElement().get<std::string>();
            args.pop();
            auto newName = args.front()->getElement().get<std::string>();
            args.pop();

            bool result = themeManager->duplicateTheme(source, newName);
            return makeVar(result);
        }

    private:
        ThemeManager* themeManager = nullptr;
    };

    // Set current theme function
    class ThemeSetCurrent : public Function
    {
        using Function::Function;
    public:
        void setUp(ThemeManager* manager)
        {
            setArity(1, 1); // name
            this->themeManager = manager;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto name = args.front()->getElement().get<std::string>();
            args.pop();

            themeManager->setCurrentTheme(name);
            return makeVar(true);
        }

    private:
        ThemeManager* themeManager = nullptr;
    };

    // Get current theme function
    class ThemeGetCurrent : public Function
    {
        using Function::Function;
    public:
        void setUp(ThemeManager* manager)
        {
            setArity(0, 0);
            this->themeManager = manager;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            return makeVar(themeManager->getCurrentTheme().name);
        }

    private:
        ThemeManager* themeManager = nullptr;
    };

    // Get property value from current theme
    class ThemeGetProperty : public Function
    {
        using Function::Function;
    public:
        void setUp(ThemeManager* manager)
        {
            setArity(1, 1); // property key
            this->themeManager = manager;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto property = args.front()->getElement().get<std::string>();
            args.pop();

            auto value = themeManager->getCurrentTheme().getValue(property);
            return makeVar(value);
        }

    private:
        ThemeManager* themeManager = nullptr;
    };

    // Set property in current theme
    class ThemeSetProperty : public Function
    {
        using Function::Function;
    public:
        void setUp(ThemeManager* manager)
        {
            setArity(2, 2); // property, value
            this->themeManager = manager;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto property = args.front()->getElement().get<std::string>();
            args.pop();
            auto value = args.front()->getElement();
            args.pop();

            auto& theme = themeManager->getCurrentThemeRef();
            theme.properties[property] = value;
            return makeVar(true);
        }

    private:
        ThemeManager* themeManager = nullptr;
    };

    // Module structure following existing patterns
    struct ThemeModule : public SysModule
    {
        ThemeModule(ThemeManager* manager) : themeManager(manager)
        {
            addSystemFunction<ThemeCreate>("createTheme", themeManager);
            addSystemFunction<ThemeDelete>("deleteTheme", themeManager);
            addSystemFunction<ThemeDuplicate>("duplicateTheme", themeManager);
            addSystemFunction<ThemeSetCurrent>("setCurrentTheme", themeManager);
            addSystemFunction<ThemeGetCurrent>("getCurrentTheme", themeManager);
            addSystemFunction<ThemeGetProperty>("getThemeProperty", themeManager);
            addSystemFunction<ThemeSetProperty>("setThemeProperty", themeManager);
        }

        ThemeManager* themeManager;
    };
}