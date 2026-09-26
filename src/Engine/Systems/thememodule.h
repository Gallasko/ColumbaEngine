#pragma once

#include <cstdio>

#include "UI/themesystem.h"
#include "Interpreter/pginterpreter.h"
#include "logger.h"

namespace pg
{
    // Switches the current theme
    class ThemeSetCurrent : public Function
    {
        using Function::Function;
    public:
        void setUp(ThemeSystem* theme)
        {
            setArity(1, 1); // name
            this->theme = theme;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto name = args.front()->getElement().get<std::string>();
            args.pop();

            const bool known = theme->theme().hasThemeId(name);

            if (known)
                theme->setTheme(name);

            return makeVar(known);
        }

    private:
        ThemeSystem* theme = nullptr;
    };

    // Name of the current theme
    class ThemeGetCurrent : public Function
    {
        using Function::Function;
    public:
        void setUp(ThemeSystem* theme)
        {
            setArity(0, 0);
            this->theme = theme;
        }

        virtual ValuablePtr call(ValuableQueue&) override
        {
            return makeVar(theme->currentTheme());
        }

    private:
        ThemeSystem* theme = nullptr;
    };

    // Whether the loaded theme file defines a theme id
    class ThemeHas : public Function
    {
        using Function::Function;
    public:
        void setUp(ThemeSystem* theme)
        {
            setArity(1, 1); // name
            this->theme = theme;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto name = args.front()->getElement().get<std::string>();
            args.pop();

            return makeVar(theme->theme().hasThemeId(name));
        }

    private:
        ThemeSystem* theme = nullptr;
    };

    // Loads a theme file; returns whether it parsed without errors
    class ThemeLoad : public Function
    {
        using Function::Function;
    public:
        void setUp(ThemeSystem* theme)
        {
            setArity(1, 1); // path
            this->theme = theme;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto path = args.front()->getElement().get<std::string>();
            args.pop();

            return makeVar(theme->loadTheme(path));
        }

    private:
        ThemeSystem* theme = nullptr;
    };

    // A color token of the current theme as "#rrggbbaa"
    class ThemeGetColor : public Function
    {
        using Function::Function;
    public:
        void setUp(ThemeSystem* theme)
        {
            setArity(1, 1); // token
            this->theme = theme;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto token = args.front()->getElement().get<std::string>();
            args.pop();

            const auto color = theme->color(token);

            char text[10];
            std::snprintf(text, sizeof(text), "#%02x%02x%02x%02x", static_cast<int>(color.x), static_cast<int>(color.y), static_cast<int>(color.z), static_cast<int>(color.w));

            return makeVar(std::string(text));
        }

    private:
        ThemeSystem* theme = nullptr;
    };

    // A scale token of the current theme: spacing, border, radius or opacity
    template <int Kind>
    class ThemeGetScale : public Function
    {
        using Function::Function;
    public:
        void setUp(ThemeSystem* theme)
        {
            setArity(1, 1); // token
            this->theme = theme;
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto token = args.front()->getElement().get<std::string>();
            args.pop();

            float value = 0.0f;

            switch (Kind)
            {
            case 0:
                value = theme->spacing(token);
                break;

            case 1:
                value = theme->border(token);
                break;

            case 2:
                value = theme->radius(token);
                break;

            default:
                value = theme->opacity(token);
                break;
            }

            return makeVar(value);
        }

    private:
        ThemeSystem* theme = nullptr;
    };

    struct ThemeModule : public SysModule
    {
        ThemeModule(ThemeSystem* theme) : theme(theme)
        {
            addSystemFunction<ThemeSetCurrent>("setCurrentTheme", theme);
            addSystemFunction<ThemeGetCurrent>("getCurrentTheme", theme);
            addSystemFunction<ThemeHas>("hasTheme", theme);
            addSystemFunction<ThemeLoad>("loadTheme", theme);
            addSystemFunction<ThemeGetColor>("getThemeColor", theme);
            addSystemFunction<ThemeGetScale<0>>("getThemeSpacing", theme);
            addSystemFunction<ThemeGetScale<1>>("getThemeBorder", theme);
            addSystemFunction<ThemeGetScale<2>>("getThemeRadius", theme);
            addSystemFunction<ThemeGetScale<3>>("getThemeOpacity", theme);
        }

        ThemeSystem* theme;
    };
}
