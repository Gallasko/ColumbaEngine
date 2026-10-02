#pragma once

#include <cstdio>
#include <string>

#include "Compiler/vm.h"
#include "Compiler/native_module.h"

#include "UI/themesystem.h"

namespace pg
{
    /**
     * Theme Module
     * Lets a script load a theme file, switch the current theme and read its tokens
     */
    class ThemeNativeModule : public NativeModule
    {
    public:
        ThemeNativeModule(ThemeSystem* theme)
        {
            // Switches the current theme, returns whether the theme id is known
            addNativeFunction("setCurrentTheme", [theme](VM* vm, int argCount, Value* args) -> Value {
                const auto name = getStringArg(vm, "setCurrentTheme", argCount, args);

                const bool known = theme->theme().hasThemeId(name);

                if (known)
                    theme->setTheme(name);

                return makeBoolValue(known);
            });

            addNativeFunction("getCurrentTheme", [theme](VM* vm, int argCount, Value*) -> Value {
                if (argCount != 0)
                {
                    throw std::runtime_error("getCurrentTheme expects no argument");
                }

                return vm->createString(theme->currentTheme());
            });

            addNativeFunction("hasTheme", [theme](VM* vm, int argCount, Value* args) -> Value {
                return makeBoolValue(theme->theme().hasThemeId(getStringArg(vm, "hasTheme", argCount, args)));
            });

            // Loads a theme file, returns whether it parsed without errors
            addNativeFunction("loadTheme", [theme](VM* vm, int argCount, Value* args) -> Value {
                return makeBoolValue(theme->loadTheme(getStringArg(vm, "loadTheme", argCount, args)));
            });

            // A color token of the current theme as "#rrggbbaa"
            addNativeFunction("getThemeColor", [theme](VM* vm, int argCount, Value* args) -> Value {
                const auto color = theme->color(getStringArg(vm, "getThemeColor", argCount, args));

                char text[10];
                std::snprintf(text, sizeof(text), "#%02x%02x%02x%02x", static_cast<int>(color.x), static_cast<int>(color.y), static_cast<int>(color.z), static_cast<int>(color.w));

                return vm->createString(std::string(text));
            });

            addNativeFunction("getThemeSpacing", [theme](VM* vm, int argCount, Value* args) -> Value {
                return makeDoubleValue(theme->spacing(getStringArg(vm, "getThemeSpacing", argCount, args)));
            });

            addNativeFunction("getThemeBorder", [theme](VM* vm, int argCount, Value* args) -> Value {
                return makeDoubleValue(theme->border(getStringArg(vm, "getThemeBorder", argCount, args)));
            });

            addNativeFunction("getThemeRadius", [theme](VM* vm, int argCount, Value* args) -> Value {
                return makeDoubleValue(theme->radius(getStringArg(vm, "getThemeRadius", argCount, args)));
            });

            addNativeFunction("getThemeOpacity", [theme](VM* vm, int argCount, Value* args) -> Value {
                return makeDoubleValue(theme->opacity(getStringArg(vm, "getThemeOpacity", argCount, args)));
            });
        }

    private:
        static std::string getStringArg(VM* vm, const std::string& function, int argCount, Value* args)
        {
            if (argCount != 1 or not IS_STRING(args[0]))
            {
                throw std::runtime_error(function + " expects exactly 1 string argument");
            }

            return vm->asString(args[0]);
        }
    };
}
