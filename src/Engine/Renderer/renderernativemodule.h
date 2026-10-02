#pragma once

#include <string>

#include "Compiler/vm.h"
#include "Compiler/native_module.h"

#include "renderer.h"

#include "logger.h"

namespace pg
{
    /**
     * Renderer Module
     * Lets a script register the shaders and the textures of the master renderer
     */
    class RendererNativeModule : public NativeModule
    {
    public:
        RendererNativeModule(MasterRenderer *masterRenderer)
        {
            addNativeFunction("loadShader", [masterRenderer](VM* vm, int argCount, Value* args) -> Value {
                if (argCount != 3)
                {
                    throw std::runtime_error("loadShader expects exactly 3 arguments (name, vsPath, fsPath)");
                }

                if (not IS_STRING(args[0]) or not IS_STRING(args[1]) or not IS_STRING(args[2]))
                {
                    throw std::runtime_error("loadShader expects all its arguments to be strings");
                }

                const auto name = vm->asString(args[0]);

                LOG_INFO("Renderer Module", "Register Shader: " << name);

                masterRenderer->registerShader(name, vm->asString(args[1]), vm->asString(args[2]));

                return makeBoolValue(true);
            });

            // Todo add an argument to specify the type of texture loaded, e.g.: RGBA, RGB, ...
            addNativeFunction("loadTexture", [masterRenderer](VM* vm, int argCount, Value* args) -> Value {
                if (argCount != 2)
                {
                    throw std::runtime_error("loadTexture expects exactly 2 arguments (name, path)");
                }

                if (not IS_STRING(args[0]) or not IS_STRING(args[1]))
                {
                    throw std::runtime_error("loadTexture expects all its arguments to be strings");
                }

                masterRenderer->registerTexture(vm->asString(args[0]), vm->asString(args[1]).c_str());

                return makeBoolValue(true);
            });

            addNativeFunction("loadAtlasTexture", [masterRenderer](VM* vm, int argCount, Value* args) -> Value {
                if (argCount != 3)
                {
                    throw std::runtime_error("loadAtlasTexture expects exactly 3 arguments (name, texturePath, atlasPath)");
                }

                if (not IS_STRING(args[0]) or not IS_STRING(args[1]) or not IS_STRING(args[2]))
                {
                    throw std::runtime_error("loadAtlasTexture expects all its arguments to be strings");
                }

                masterRenderer->registerAtlasTexture(vm->asString(args[0]), vm->asString(args[1]).c_str(), vm->asString(args[2]).c_str());

                return makeBoolValue(true);
            });
        }
    };
}
