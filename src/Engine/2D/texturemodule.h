
#pragma once

#include "Compiler/native_module.h"

#include "ECS/entitysystem.h"

#include "Compiler/vm.h"

#include "texture.h"

#include "Compiler/ecsserialization.h"

namespace pg
{
    /**
     * Algorithm Module
     * Provides utility functions for common algorithms and data structure operations
     */
    class TextureModule : public NativeModule
    {
    public:
        TextureModule(EntitySystem* ecsRef)
        {
            auto ecsRefCopy = ecsRef;

            addNativeFunction("createTexture", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 1)
                {
                    throw std::runtime_error("createTexture expects at least 1 argument");
                }

                if (argCount > 3)
                {
                    throw std::runtime_error("createTexture expects at most 3 arguments");
                }

                if (!IS_STRING(args[0]))
                {
                    throw std::runtime_error("createTexture expects the first argument to be a string (file path)");
                }

                std::string filePath = vm->asString(args[0]);

                int width = 0;
                int height = 0;

                if (argCount >= 2)
                {
                    if (!IS_INT(args[1]))
                    {
                        throw std::runtime_error("createTexture expects the second argument to be an integer (width)");
                    }

                    width = static_cast<int>(AS_INT(args[1]));
                }

                if (argCount == 3)
                {
                    if (!IS_INT(args[2]))
                    {
                        throw std::runtime_error("createTexture expects the third argument to be an integer (height)");
                    }

                    height = static_cast<int>(AS_INT(args[2]));
                }

                auto tex = make2DTexture(ecsRefCopy, width, height, filePath);

                return serializeEntityToTable(vm, ecsRefCopy, tex);
            });
        }
    };
}