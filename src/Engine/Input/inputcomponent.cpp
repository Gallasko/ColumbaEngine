#include "stdafx.h"

#include "inputcomponent.h"

#include "../UI/uisystem.h"

#include <iostream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <SDL2/SDL.h>
#include <SDL_opengles2.h>
// #include <SDL_opengl_glext.h>
// #include <GLES2/gl2.h>
// #include <GLFW/glfw3.h>
#else
    #ifdef __linux__
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_opengl.h>
    #elif _WIN32
    #include <SDL.h>
    #include <SDL_opengl.h>
    #endif
#endif

#include "serialization.h"

namespace pg
{
    // Helper function to convert SDL_Scancode to friendly string
    std::string scancodeToString(SDL_Scancode key)
    {
        const char* name = SDL_GetScancodeName(key);
        if (name && name[0] != '\0')
        {
            return std::string(name);
        }
        return "Unknown";
    }

    // Helper function to convert SDL modifier flags to friendly string
    std::string modifierToString(Uint16 mod)
    {
        if (mod == KMOD_NONE)
            return "None";

        std::string result;

        // Check for left/right specific modifiers first
        if (mod & KMOD_LCTRL)  result += "LCtrl+";
        if (mod & KMOD_RCTRL)  result += "RCtrl+";
        if (mod & KMOD_LSHIFT) result += "LShift+";
        if (mod & KMOD_RSHIFT) result += "RShift+";
        if (mod & KMOD_LALT)   result += "LAlt+";
        if (mod & KMOD_RALT)   result += "RAlt+";
        if (mod & KMOD_LGUI)   result += "LGui+";
        if (mod & KMOD_RGUI)   result += "RGui+";

        // Check for generic modifiers if specific ones weren't set
        // (KMOD_CTRL = LCTRL | RCTRL, etc.)
        if ((mod & KMOD_CTRL) && !(mod & (KMOD_LCTRL | KMOD_RCTRL)))
            result += "Ctrl+";
        if ((mod & KMOD_SHIFT) && !(mod & (KMOD_LSHIFT | KMOD_RSHIFT)))
            result += "Shift+";
        if ((mod & KMOD_ALT) && !(mod & (KMOD_LALT | KMOD_RALT)))
            result += "Alt+";
        if ((mod & KMOD_GUI) && !(mod & (KMOD_LGUI | KMOD_RGUI)))
            result += "Gui+";

        if (mod & KMOD_NUM)    result += "Num+";
        if (mod & KMOD_CAPS)   result += "Caps+";
        if (mod & KMOD_MODE)   result += "AltGr+";

        // Remove trailing '+'
        if (!result.empty() && result.back() == '+')
            result.pop_back();

        return result;
    }

    namespace
    {
        size_t getViewport(const CompRef<ViewportComponent>& vp)
        {
            return vp.empty() ? 0 : vp->viewport;
        }
    }

    template<>
    void serialize(Archive& archive, const MouseLeftClickComponent& component)
    {
        archive.startSerialization("Mouse Left Click Component");

        component.callback->serialize(archive);

        archive.endSerialization();
    }

    void MouseClickSystem::init()
    {
        LOG_THIS_MEMBER("MouseClickSystem");

        pressedList.emplace(SDL_BUTTON_LEFT, false);
        pressedList.emplace(SDL_BUTTON_RIGHT, false);

        auto leftGroup = registerGroup<PositionComponent, MouseLeftClickComponent>();

        // Todo change this (useless and error prone, just loop over the group view)
        // To do this change we need to make the view orderable (maybe more complicated than expected)
        leftGroup->addOnGroup([this](EntityRef entity) {
            LOG_MILE("MouseLeftClickSystem", "Add entity " << entity->id << " to ui - mouse left click group !");

            auto pos = entity->get<PositionComponent>();
            auto mouse = entity->get<MouseLeftClickComponent>();

            CompRef<ViewportComponent> vp;
            if (entity->has<ViewportComponent>())
                vp = entity->get<ViewportComponent>();

            if (mouse->trigger == MouseStateTrigger::OnPress)
            {
                mouseLeftAreaPressHolder.emplace(entity->id, entity, pos, vp);
            }
            else if (mouse->trigger == MouseStateTrigger::OnRelease)
            {
                mouseLeftAreaReleaseHolder.emplace(entity->id, entity, pos, vp);
            }
            else
            {
                mouseLeftAreaPressHolder.emplace(entity->id, entity, pos, vp);
                mouseLeftAreaReleaseHolder.emplace(entity->id, entity, pos, vp);
            }
        });

        leftGroup->removeOfGroup([this](EntitySystem*, _unique_id id) {
            LOG_MILE("MouseLeftClickSystem", "Remove entity " << id << " to ui - mouse left click group !");

            const auto& it = std::find_if(mouseLeftAreaPressHolder.begin(), mouseLeftAreaPressHolder.end(), [id](const MouseAreaZ& area) { return area.id == id; });

            if (it != mouseLeftAreaPressHolder.end())
            {
                mouseLeftAreaPressHolder.erase(it);
            }

            const auto& it2 = std::find_if(mouseLeftAreaReleaseHolder.begin(), mouseLeftAreaReleaseHolder.end(), [id](const MouseAreaZ& area) { return area.id == id; });

            if (it2 != mouseLeftAreaReleaseHolder.end())
            {
                mouseLeftAreaReleaseHolder.erase(it2);
            }
        });

        auto rightGroup = registerGroup<PositionComponent, MouseRightClickComponent>();

        rightGroup->addOnGroup([this](EntityRef entity) {
            LOG_MILE("MouseRightClickSystem", "Add entity " << entity->id << " to ui - mouse right click group !");

            auto pos = entity->get<PositionComponent>();
            auto mouse = entity->get<MouseRightClickComponent>();

            CompRef<ViewportComponent> vp;
            if (entity->has<ViewportComponent>())
                vp = entity->get<ViewportComponent>();

            if (mouse->trigger == MouseStateTrigger::OnPress)
            {
                mouseRightAreaPressHolder.emplace(entity->id, entity, pos, vp);
            }
            else if (mouse->trigger == MouseStateTrigger::OnRelease)
            {
                mouseRightAreaReleaseHolder.emplace(entity->id, entity, pos, vp);
            }
            else
            {
                mouseRightAreaPressHolder.emplace(entity->id, entity, pos, vp);
                mouseRightAreaReleaseHolder.emplace(entity->id, entity, pos, vp);
            }
        });

        rightGroup->removeOfGroup([this](EntitySystem*, _unique_id id) {
            LOG_MILE("MouseRightClickSystem", "Remove entity " << id << " to ui - mouse right click group !");

            const auto& it = std::find_if(mouseRightAreaPressHolder.begin(), mouseRightAreaPressHolder.end(), [id](const MouseAreaZ& area) { return area.id == id; });

            if (it != mouseRightAreaPressHolder.end())
            {
                mouseRightAreaPressHolder.erase(it);
            }

            const auto& it2 = std::find_if(mouseRightAreaReleaseHolder.begin(), mouseRightAreaReleaseHolder.end(), [id](const MouseAreaZ& area) { return area.id == id; });

            if (it2 != mouseRightAreaReleaseHolder.end())
            {
                mouseRightAreaReleaseHolder.erase(it2);
            }
        });
    }

    void MouseClickSystem::execute()
    {
        handleClick(SDL_BUTTON_LEFT, mouseLeftAreaPressHolder, mouseLeftAreaReleaseHolder);
        handleClick(SDL_BUTTON_RIGHT, mouseRightAreaPressHolder, mouseRightAreaReleaseHolder);
    }

    void MouseClickSystem::handleClick(const MouseButton& button, const std::set<MouseAreaZ, std::greater<>>& pressAreas, const std::set<MouseAreaZ, std::greater<>>& releaseAreas)
    {
        size_t highestViewport = 0;
        int highestZ = INT_MIN;
        const auto& mousePos = inputHandler->getMousePos();

        if (areNotAlmostEqual(oldMousePos.x, mousePos.x) or areNotAlmostEqual(oldMousePos.y, mousePos.y))
        {
            ecsRef->sendEvent(OnMouseMove{mousePos, inputHandler});
        }

        oldMousePos = mousePos;

        if (inputHandler->isButtonPressed(button))
        {
            if (not pressedList[button])
            {
                ecsRef->sendEvent(OnMouseClick{mousePos, button});
            }

            // Todo check if this should not be in a if (not pressedList[button]) statment
            for (const auto& mouseArea : pressAreas)
            {
                auto pos = mouseArea.pos;
                auto areaVp = getViewport(mouseArea.vp);

                if (areaVp < highestViewport)
                    break;
                if (areaVp == highestViewport and pos->z < highestZ)
                    break;

                if (inClipBound(mouseArea.ui, mousePos.x, mousePos.y))
                {
                    highestViewport = areaVp;
                    highestZ = pos->z;

                    callCallback(button, mouseArea.id);
                }
            }

            pressedList[button] = true;
        }

        highestViewport = 0;
        highestZ = INT_MIN;

        if (not inputHandler->isButtonPressed(button))
        {
            if (pressedList[button])
            {
                ecsRef->sendEvent(OnMouseRelease{mousePos, button});

                for (const auto& mouseArea : releaseAreas)
                {
                    auto pos = mouseArea.pos;
                    auto areaVp = getViewport(mouseArea.vp);

                    if (areaVp < highestViewport)
                        break;
                    if (areaVp == highestViewport and pos->z < highestZ)
                        break;

                    if (inClipBound(mouseArea.ui, mousePos.x, mousePos.y))
                    {
                        highestViewport = areaVp;
                        highestZ = pos->z;

                        callCallback(button, mouseArea.id);
                    }
                }
            }

            pressedList[button] = false;
        }
    }

    void MouseClickSystem::callCallback(const MouseButton& button, _unique_id id)
    {
        if (button == SDL_BUTTON_LEFT)
        {
            auto comp = static_cast<Own<MouseLeftClickComponent>*>(this)->getComponent(id);

            comp->callback->call(world());
        }
        else if (button == SDL_BUTTON_RIGHT)
        {
            auto comp = static_cast<Own<MouseRightClickComponent>*>(this)->getComponent(id);

            comp->callback->call(world());
        }
        else
        {
            LOG_ERROR("MouseClickSystem", "Unknown mouse button pressed: " << button);
        }
    }

    void MouseWheelSystem::onEvent(const OnSDLMouseWheel& event)
    {
        size_t highestViewport = 0;
        int highestZ = INT_MIN;
        const auto& mousePos = inputHandler->getMousePos();

        for (const auto& mouseArea : mouseAreaHolder)
        {
            auto pos = mouseArea.pos;
            auto areaVp = getViewport(mouseArea.vp);

            if (areaVp < highestViewport)
                break;
            if (areaVp == highestViewport and pos->z < highestZ)
                break;

            if (inClipBound(mouseArea.ui, mousePos.x, mousePos.y))
            {
                highestViewport = areaVp;
                highestZ = pos->z;

                auto comp = static_cast<Own<MouseWheelComponent>*>(this)->getComponent(mouseArea.id);

                auto retEvent = comp->event;

                retEvent.values["x"] = event.x;
                retEvent.values["y"] = event.y;

                ecsRef->sendEvent(retEvent);
            }
        }
    }

    bool operator<(MouseAreaZ lhs, MouseAreaZ rhs)
    {
        const auto lhsVp = getViewport(lhs.vp);
        const auto rhsVp = getViewport(rhs.vp);

        if (lhsVp != rhsVp)
            return lhsVp < rhsVp;

        const auto& z = lhs.pos->z;
        const auto& rhsZ = rhs.pos->z;

        if (areAlmostEqual(z, rhsZ))
            return lhs.id < rhs.id;
        else
            return z < rhsZ;
    }

    bool operator>(MouseAreaZ lhs, MouseAreaZ rhs)
    {
        const auto lhsVp = getViewport(lhs.vp);
        const auto rhsVp = getViewport(rhs.vp);

        if (lhsVp != rhsVp)
            return lhsVp > rhsVp;

        const auto& z = lhs.pos->z;
        const auto& rhsZ = rhs.pos->z;

        if (areAlmostEqual(z, rhsZ))
            return lhs.id > rhs.id;
        else
            return z > rhsZ;
    }

    // ============================================================================
    // StandardEvent Conversion Implementations
    // ============================================================================
    // These implementations are only used when PG_AUTO_CONVERT_EVENTS_TO_STANDARD is defined

    STANDARD_EVENT_CONVERSION_IMPL(OnMouseClick)
    {
        StandardEvent event("OnMouseClick");

        event.values["x"] = ElementType{pos.x};
        event.values["y"] = ElementType{pos.y};

        if (button == SDL_BUTTON_LEFT)
            event.values["button"] = ElementType{"left"};
        else if (button == SDL_BUTTON_MIDDLE)
            event.values["button"] = ElementType{"middle"};
        else if (button == SDL_BUTTON_RIGHT)
            event.values["button"] = ElementType{"right"};

        event.values["buttonValue"] = ElementType{static_cast<int>(button)};

        return event;
    }

    STANDARD_EVENT_CONVERSION_IMPL(OnMouseRelease)
    {
        StandardEvent event("OnMouseRelease");

        event.values["x"] = ElementType{pos.x};
        event.values["y"] = ElementType{pos.y};

        if (button == SDL_BUTTON_LEFT)
            event.values["button"] = ElementType{"left"};
        else if (button == SDL_BUTTON_MIDDLE)
            event.values["button"] = ElementType{"middle"};
        else if (button == SDL_BUTTON_RIGHT)
            event.values["button"] = ElementType{"right"};

        event.values["buttonValue"] = ElementType{static_cast<int>(button)};

        return event;
    }

    STANDARD_EVENT_CONVERSION_IMPL(OnMouseMove)
    {
        StandardEvent event("OnMouseMove");

        event.values["x"] = ElementType{pos.x};
        event.values["y"] = ElementType{pos.y};

        return event;
    }

    STANDARD_EVENT_CONVERSION_IMPL(OnSDLTextInput)
    {
        StandardEvent event("OnSDLTextInput");

        event.values["text"] = ElementType{text};

        return event;
    }

    STANDARD_EVENT_CONVERSION_IMPL(OnSDLScanCode)
    {
        StandardEvent event("OnSDLScanCode");

        event.values["keyValue"] = ElementType{static_cast<int>(key)};
        event.values["modValue"] = ElementType{static_cast<int>(mod)};

        event.values["key"] = ElementType{scancodeToString(key)};
        event.values["mod"] = ElementType{modifierToString(mod)};

        return event;
    }

    STANDARD_EVENT_CONVERSION_IMPL(OnSDLScanCodeReleased)
    {
        StandardEvent event("OnSDLScanCodeReleased");

        event.values["keyValue"] = ElementType{static_cast<int>(key)};
        event.values["modValue"] = ElementType{static_cast<int>(mod)};

        event.values["key"] = ElementType{scancodeToString(key)};
        event.values["mod"] = ElementType{modifierToString(mod)};

        return event;
    }

    STANDARD_EVENT_CONVERSION_IMPL(OnSDLMouseWheel)
    {
        StandardEvent event("OnSDLMouseWheel");

        event.values["x"] = ElementType{static_cast<int>(x)};
        event.values["y"] = ElementType{static_cast<int>(y)};

        return event;
    }
}