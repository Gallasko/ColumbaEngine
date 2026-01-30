#pragma once

#include "Compiler/native_module.h"
#include "ECS/entitysystem.h"
#include "Compiler/vm.h"
#include "Compiler/ecsserialization.h"
#include "UI/ttftext.h"

namespace pg
{
    /**
     * UI Module
     * Provides functions for creating UI elements like text, buttons, and panels
     */
    class UIModule : public NativeModule
    {
    public:
        UIModule(EntitySystem* ecsRef)
        {
            auto ecsRefCopy = ecsRef;

            /**
             * createText(fontPath, text, x, y, [scale], [r, g, b, a])
             * Creates a TTF text element
             *
             * @param fontPath - Path to TTF font file
             * @param text - Text content to display
             * @param x - X position
             * @param y - Y position
             * @param scale - (optional) Text scale, default 1.0
             * @param r,g,b,a - (optional) Color components 0-255, default white (255,255,255,255)
             * @returns Entity table with text component
             */
            addNativeFunction("createText", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 4)
                {
                    throw std::runtime_error("createText expects at least 4 arguments: fontPath, text, x, y");
                }

                if (!IS_STRING(args[0]))
                {
                    throw std::runtime_error("createText: fontPath must be a string");
                }

                if (!IS_STRING(args[1]))
                {
                    throw std::runtime_error("createText: text must be a string");
                }

                std::string fontPath = vm->asString(args[0]);
                std::string text = vm->asString(args[1]);

                // Get position
                float x = 0.0f, y = 0.0f;

                if (IS_INT(args[2]))
                    x = static_cast<float>(AS_INT(args[2]));
                else if (IS_DOUBLE(args[2]))
                    x = static_cast<float>(AS_DOUBLE(args[2]));
                else
                    throw std::runtime_error("createText: x must be a number");

                if (IS_INT(args[3]))
                    y = static_cast<float>(AS_INT(args[3]));
                else if (IS_DOUBLE(args[3]))
                    y = static_cast<float>(AS_DOUBLE(args[3]));
                else
                    throw std::runtime_error("createText: y must be a number");

                // Optional: scale (default 1.0)
                float scale = 1.0f;
                if (argCount >= 5)
                {
                    if (IS_INT(args[4]))
                        scale = static_cast<float>(AS_INT(args[4]));
                    else if (IS_DOUBLE(args[4]))
                        scale = static_cast<float>(AS_DOUBLE(args[4]));
                    else
                        throw std::runtime_error("createText: scale must be a number");
                }

                // Optional: colors (default white)
                constant::Vector4D colors = {255.0f, 255.0f, 255.0f, 255.0f};

                if (argCount >= 9)
                {
                    // r, g, b, a provided
                    for (int i = 0; i < 4; i++)
                    {
                        float component = 255.0f;
                        if (IS_INT(args[5 + i]))
                            component = static_cast<float>(AS_INT(args[5 + i]));
                        else if (IS_DOUBLE(args[5 + i]))
                            component = static_cast<float>(AS_DOUBLE(args[5 + i]));
                        else
                            throw std::runtime_error("createText: color components must be numbers");

                        colors[i] = component;
                    }
                }

                // Create TTF text entity
                float z = 0.0f; // Default z-order
                auto textEntity = makeTTFText(ecsRefCopy, x, y, z, fontPath, text, scale, colors);

                return serializeEntityToTable(vm, ecsRefCopy, textEntity);
            });

            /**
             * updateText(textEntity, newText)
             * Updates the text content of a text entity
             *
             * @param textEntity - Entity table returned from createText
             * @param newText - New text content
             */
            // addNativeFunction("updateText", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
            //     if (argCount != 2)
            //     {
            //         throw std::runtime_error("updateText expects 2 arguments: textEntity, newText");
            //     }

            //     if (!IS_INSTANCE(args[0]))
            //     {
            //         throw std::runtime_error("updateText: first argument must be an entity table");
            //     }

            //     if (!IS_STRING(args[1]))
            //     {
            //         throw std::runtime_error("updateText: newText must be a string");
            //     }

            //     auto instance = vm->asInstance(args[0]);
            //     std::string newText = vm->asString(args[1])->toString();

            //     // Get entity ID from the table
            //     auto idField = instance->fields.find(vm->newString("__entityId"));
            //     if (idField == instance->fields.end() || !IS_INT(idField->second))
            //     {
            //         throw std::runtime_error("updateText: entity table missing __entityId field");
            //     }

            //     _unique_id entityId = static_cast<_unique_id>(AS_INT(idField->second));

            //     // Get the entity
            //     auto entity = ecsRefCopy->getEntity(entityId);
            //     if (!entity)
            //     {
            //         throw std::runtime_error("updateText: entity not found");
            //     }

            //     // Update the text
            //     auto textComp = entity->get<TTFText>();
            //     if (textComp)
            //     {
            //         textComp->setText(newText);
            //     }
            //     else
            //     {
            //         throw std::runtime_error("updateText: entity does not have a TTFText component");
            //     }

            //     return makeIntValue(0);
            // });

            // /**
            //  * setTextColor(textEntity, r, g, b, [a])
            //  * Sets the color of a text entity
            //  *
            //  * @param textEntity - Entity table returned from createText
            //  * @param r,g,b,a - Color components 0-255, alpha optional (default 255)
            //  */
            // addNativeFunction("setTextColor", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
            //     if (argCount < 4)
            //     {
            //         throw std::runtime_error("setTextColor expects at least 4 arguments: textEntity, r, g, b, [a]");
            //     }

            //     if (!IS_INSTANCE(args[0]))
            //     {
            //         throw std::runtime_error("setTextColor: first argument must be an entity table");
            //     }

            //     auto instance = vm->asInstance(args[0]);

            //     // Get color components
            //     constant::Vector4D colors;
            //     for (int i = 0; i < 3; i++)
            //     {
            //         if (IS_INT(args[1 + i]))
            //             colors[i] = static_cast<float>(AS_INT(args[1 + i]));
            //         else if (IS_DOUBLE(args[1 + i]))
            //             colors[i] = static_cast<float>(AS_DOUBLE(args[1 + i]));
            //         else
            //             throw std::runtime_error("setTextColor: color components must be numbers");
            //     }

            //     // Alpha (optional, default 255)
            //     colors[3] = 255.0f;
            //     if (argCount >= 5)
            //     {
            //         if (IS_INT(args[4]))
            //             colors[3] = static_cast<float>(AS_INT(args[4]));
            //         else if (IS_DOUBLE(args[4]))
            //             colors[3] = static_cast<float>(AS_DOUBLE(args[4]));
            //     }

            //     // Get entity ID
            //     auto idField = instance->fields.find(vm->newString("__entityId"));
            //     if (idField == instance->fields.end() || !IS_INT(idField->second))
            //     {
            //         throw std::runtime_error("setTextColor: entity table missing __entityId field");
            //     }

            //     _unique_id entityId = static_cast<_unique_id>(AS_INT(idField->second));

            //     // Get the entity
            //     auto entity = ecsRefCopy->getEntity(entityId);
            //     if (!entity)
            //     {
            //         throw std::runtime_error("setTextColor: entity not found");
            //     }

            //     // Update the color
            //     auto textComp = entity->get<TTFText>();
            //     if (textComp)
            //     {
            //         textComp->setColor(colors);
            //     }
            //     else
            //     {
            //         throw std::runtime_error("setTextColor: entity does not have a TTFText component");
            //     }

            //     return makeIntValue(0);
            // });

            /**
             * setTextPosition(textEntity, x, y)
             * Updates the position of a text entity
             *
             * @param textEntity - Entity table returned from createText
             * @param x - New X position
             * @param y - New Y position
             */
            // addNativeFunction("setTextPosition", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
            //     if (argCount != 3)
            //     {
            //         throw std::runtime_error("setTextPosition expects 3 arguments: textEntity, x, y");
            //     }

            //     if (!IS_INSTANCE(args[0]))
            //     {
            //         throw std::runtime_error("setTextPosition: first argument must be an entity table");
            //     }

            //     auto instance = vm->asInstance(args[0]);

            //     // Get position
            //     float x = 0.0f, y = 0.0f;

            //     if (IS_INT(args[1]))
            //         x = static_cast<float>(AS_INT(args[1]));
            //     else if (IS_DOUBLE(args[1]))
            //         x = static_cast<float>(AS_DOUBLE(args[1]));
            //     else
            //         throw std::runtime_error("setTextPosition: x must be a number");

            //     if (IS_INT(args[2]))
            //         y = static_cast<float>(AS_INT(args[2]));
            //     else if (IS_DOUBLE(args[2]))
            //         y = static_cast<float>(AS_DOUBLE(args[2]));
            //     else
            //         throw std::runtime_error("setTextPosition: y must be a number");

            //     // Get entity ID
            //     auto idField = instance->fields.find(vm->newString("__entityId"));
            //     if (idField == instance->fields.end() || !IS_INT(idField->second))
            //     {
            //         throw std::runtime_error("setTextPosition: entity table missing __entityId field");
            //     }

            //     _unique_id entityId = static_cast<_unique_id>(AS_INT(idField->second));

            //     // Get the entity
            //     auto entity = ecsRefCopy->getEntity(entityId);
            //     if (!entity)
            //     {
            //         throw std::runtime_error("setTextPosition: entity not found");
            //     }

            //     // Update position
            //     auto posComp = entity->get<PositionComponent>();
            //     if (posComp)
            //     {
            //         posComp->setX(x);
            //         posComp->setY(y);
            //     }
            //     else
            //     {
            //         throw std::runtime_error("setTextPosition: entity does not have a PositionComponent");
            //     }

            //     return makeIntValue(0);
            // });
        }
    };
}
