#pragma once

#include "Compiler/native_module.h"
#include "Input/input.h"
#include <SDL_scancode.h>

namespace pg
{
    /**
     * Input Module for VM Compiler
     * Provides functions to check keyboard input state
     */
    class InputModuleVM : public NativeModule
    {
    public:
        InputModuleVM(Input* inputRef)
        {
            // Capture input pointer by value (not this pointer)
            Input* inputPtr = inputRef;

            // Add input checking functions
            addNativeFunction("isKeyPressed", [inputPtr](VM* vm, int argCount, Value* args) -> Value {
                return nativeIsKeyPressed(vm, argCount, args, inputPtr);
            });

            addNativeFunction("isKeyReleased", [inputPtr](VM* vm, int argCount, Value* args) -> Value {
                return nativeIsKeyReleased(vm, argCount, args, inputPtr);
            });

            addNativeFunction("isKeyGrabbed", [inputPtr](VM* vm, int argCount, Value* args) -> Value {
                return nativeIsKeyGrabbed(vm, argCount, args, inputPtr);
            });

            // Debug function to test if input module is working
            addNativeFunction("testInput", [inputPtr](VM* vm, int, Value*) -> Value {
                if (not inputPtr)
                {
                    return vm->createString("Input system NOT initialized!");
                }

                return vm->createString("Input system is working!");
            });

            // Add scancode constants for easy key checking
            addNativeVariable("KEY_A", static_cast<int>(SDL_SCANCODE_A));
            addNativeVariable("KEY_B", static_cast<int>(SDL_SCANCODE_B));
            addNativeVariable("KEY_C", static_cast<int>(SDL_SCANCODE_C));
            addNativeVariable("KEY_D", static_cast<int>(SDL_SCANCODE_D));
            addNativeVariable("KEY_E", static_cast<int>(SDL_SCANCODE_E));
            addNativeVariable("KEY_F", static_cast<int>(SDL_SCANCODE_F));
            addNativeVariable("KEY_G", static_cast<int>(SDL_SCANCODE_G));
            addNativeVariable("KEY_H", static_cast<int>(SDL_SCANCODE_H));
            addNativeVariable("KEY_I", static_cast<int>(SDL_SCANCODE_I));
            addNativeVariable("KEY_J", static_cast<int>(SDL_SCANCODE_J));
            addNativeVariable("KEY_K", static_cast<int>(SDL_SCANCODE_K));
            addNativeVariable("KEY_L", static_cast<int>(SDL_SCANCODE_L));
            addNativeVariable("KEY_M", static_cast<int>(SDL_SCANCODE_M));
            addNativeVariable("KEY_N", static_cast<int>(SDL_SCANCODE_N));
            addNativeVariable("KEY_O", static_cast<int>(SDL_SCANCODE_O));
            addNativeVariable("KEY_P", static_cast<int>(SDL_SCANCODE_P));
            addNativeVariable("KEY_Q", static_cast<int>(SDL_SCANCODE_Q));
            addNativeVariable("KEY_R", static_cast<int>(SDL_SCANCODE_R));
            addNativeVariable("KEY_S", static_cast<int>(SDL_SCANCODE_S));
            addNativeVariable("KEY_T", static_cast<int>(SDL_SCANCODE_T));
            addNativeVariable("KEY_U", static_cast<int>(SDL_SCANCODE_U));
            addNativeVariable("KEY_V", static_cast<int>(SDL_SCANCODE_V));
            addNativeVariable("KEY_W", static_cast<int>(SDL_SCANCODE_W));
            addNativeVariable("KEY_X", static_cast<int>(SDL_SCANCODE_X));
            addNativeVariable("KEY_Y", static_cast<int>(SDL_SCANCODE_Y));
            addNativeVariable("KEY_Z", static_cast<int>(SDL_SCANCODE_Z));

            addNativeVariable("KEY_0", static_cast<int>(SDL_SCANCODE_0));
            addNativeVariable("KEY_1", static_cast<int>(SDL_SCANCODE_1));
            addNativeVariable("KEY_2", static_cast<int>(SDL_SCANCODE_2));
            addNativeVariable("KEY_3", static_cast<int>(SDL_SCANCODE_3));
            addNativeVariable("KEY_4", static_cast<int>(SDL_SCANCODE_4));
            addNativeVariable("KEY_5", static_cast<int>(SDL_SCANCODE_5));
            addNativeVariable("KEY_6", static_cast<int>(SDL_SCANCODE_6));
            addNativeVariable("KEY_7", static_cast<int>(SDL_SCANCODE_7));
            addNativeVariable("KEY_8", static_cast<int>(SDL_SCANCODE_8));
            addNativeVariable("KEY_9", static_cast<int>(SDL_SCANCODE_9));

            addNativeVariable("KEY_SPACE", static_cast<int>(SDL_SCANCODE_SPACE));
            addNativeVariable("KEY_RETURN", static_cast<int>(SDL_SCANCODE_RETURN));
            addNativeVariable("KEY_ESCAPE", static_cast<int>(SDL_SCANCODE_ESCAPE));
            addNativeVariable("KEY_BACKSPACE", static_cast<int>(SDL_SCANCODE_BACKSPACE));
            addNativeVariable("KEY_TAB", static_cast<int>(SDL_SCANCODE_TAB));

            addNativeVariable("KEY_UP", static_cast<int>(SDL_SCANCODE_UP));
            addNativeVariable("KEY_DOWN", static_cast<int>(SDL_SCANCODE_DOWN));
            addNativeVariable("KEY_LEFT", static_cast<int>(SDL_SCANCODE_LEFT));
            addNativeVariable("KEY_RIGHT", static_cast<int>(SDL_SCANCODE_RIGHT));

            addNativeVariable("KEY_LSHIFT", static_cast<int>(SDL_SCANCODE_LSHIFT));
            addNativeVariable("KEY_RSHIFT", static_cast<int>(SDL_SCANCODE_RSHIFT));
            addNativeVariable("KEY_LCTRL", static_cast<int>(SDL_SCANCODE_LCTRL));
            addNativeVariable("KEY_RCTRL", static_cast<int>(SDL_SCANCODE_RCTRL));
            addNativeVariable("KEY_LALT", static_cast<int>(SDL_SCANCODE_LALT));
            addNativeVariable("KEY_RALT", static_cast<int>(SDL_SCANCODE_RALT));
        }

    private:
        /**
         * Check if a key is currently pressed
         * Usage: isKeyPressed(KEY_K)
         * Returns: true if the key is pressed, false otherwise
         */
        static Value nativeIsKeyPressed(VM*, int argCount, Value* args, Input* inputHandler)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("isKeyPressed expects exactly 1 argument (scancode)");
            }

            if (!inputHandler)
            {
                throw std::runtime_error("Input system not initialized");
            }

            // Get the scancode (should be an integer)
            if (!IS_INT(args[0]))
            {
                throw std::runtime_error("isKeyPressed expects an integer scancode (e.g., KEY_K)");
            }

            int64_t scancodeValue = AS_INT(args[0]);

            // Validate scancode range
            if (scancodeValue < 0 || scancodeValue > 512)
            {
                throw std::runtime_error("Invalid scancode value: " + std::to_string(scancodeValue));
            }

            SDL_Scancode scancode = static_cast<SDL_Scancode>(scancodeValue);

            // Check both pressed AND grabbed states for better usability
            // pressed = just pressed this frame
            // grabbed = held down
            bool pressed = inputHandler->isKeyPressed(scancode);
            bool grabbed = inputHandler->isKeyGrabbed(scancode);

            return makeBoolValue(pressed || grabbed);
        }

        /**
         * Check if a key was just released
         * Usage: isKeyReleased(KEY_K)
         * Returns: true if the key was just released, false otherwise
         */
        static Value nativeIsKeyReleased(VM*, int argCount, Value* args, Input* inputHandler)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("isKeyReleased expects exactly 1 argument (scancode)");
            }

            if (!inputHandler)
            {
                throw std::runtime_error("Input system not initialized");
            }

            if (!IS_INT(args[0]))
            {
                throw std::runtime_error("isKeyReleased expects an integer scancode (e.g., KEY_K)");
            }

            SDL_Scancode scancode = static_cast<SDL_Scancode>(AS_INT(args[0]));
            bool released = inputHandler->isKeyReleased(scancode);

            return makeBoolValue(released);
        }

        /**
         * Check if a key is grabbed (held down)
         * Usage: isKeyGrabbed(KEY_K)
         * Returns: true if the key is grabbed, false otherwise
         */
        static Value nativeIsKeyGrabbed(VM*, int argCount, Value* args, Input* inputHandler)
        {
            if (argCount != 1)
            {
                throw std::runtime_error("isKeyGrabbed expects exactly 1 argument (scancode)");
            }

            if (!inputHandler)
            {
                throw std::runtime_error("Input system not initialized");
            }

            if (!IS_INT(args[0]))
            {
                throw std::runtime_error("isKeyGrabbed expects an integer scancode (e.g., KEY_K)");
            }

            SDL_Scancode scancode = static_cast<SDL_Scancode>(AS_INT(args[0]));
            bool grabbed = inputHandler->isKeyGrabbed(scancode);

            return makeBoolValue(grabbed);
        }
    };
}
