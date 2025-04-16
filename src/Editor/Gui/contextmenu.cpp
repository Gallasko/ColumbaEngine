#include "contextmenu.h"

#include <string>

#include "ECS/entitysystem.h"
#include "UI/sentencesystem.h"
#include "UI/textinput.h"
#include "2D/texture.h"
#include "Renderer/renderer.h"
#include "Scene/scenemanager.h"

#include "Helpers/tinyfiledialogs.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Context Menu";
    }

namespace editor
{
    struct Noop {};

    ContextMenu::ContextMenu()
    {
        LOG_THIS_MEMBER(DOM);
    }

    ContextMenu::~ContextMenu()
    {
        LOG_THIS_MEMBER(DOM);
    }

    void ContextMenu::init()
    {
        LOG_THIS_MEMBER(DOM);

        auto file = makeTTFText(ecsRef, 0.0f, 0.0f, 12.0f, "res/font/Inter/static/Inter_28pt-Light.ttf", "Open", 1);
        ecsRef->attach<MouseLeftClickComponent>(file.entity, makeCallable<OpenFile>());

        auto save = makeTTFText(ecsRef, 50.0f, 0.0f, 12.0f, "res/font/Inter/static/Inter_28pt-Light.ttf", "Save", 1);
        ecsRef->attach<MouseLeftClickComponent>(save.entity, makeCallable<SaveFile>());

        parent = ecsRef->createEntity();
        parentPos = ecsRef->attach<PositionComponent>(parent);

        parentPos->setX(0);
        parentPos->setY(0);
        parentPos->setZ(12);
        parentPos->setWidth(200);
        parentPos->setHeight(200);

        parentUi = ecsRef->attach<UiAnchor>(parent);
        ecsRef->attach<MouseLeaveClickComponent>(parent, makeCallable<HideContextMenu>());

        auto backTexture = makeUiTexture(ecsRef, 1, 1, "TabTexture");
        auto background = backTexture.entity;
        backgroundPos = backTexture.get<PositionComponent>();
        backgroundC = backTexture.get<UiAnchor>();
        backgroundC->fillIn(parentUi);

        setContextList("Add Sentence",  makeCallable<CreateElement>(UiComponentType::TEXT),
                       "Add TTF Text",  makeCallable<CreateElement>(UiComponentType::TTFTEXT),
                       "Add Texture",   makeCallable<CreateElement>(UiComponentType::TEXTURE),
                       "Add Button",    makeCallable<CreateElement>(UiComponentType::BUTTON),
                       "Add TextInput", makeCallable<CreateElement>(UiComponentType::TEXTINPUT),
                       "Add List",      makeCallable<CreateElement>(UiComponentType::LIST),
                       "Add Prefab",    makeCallable<CreateElement>(UiComponentType::PREFAB));

        hide();
    }

    void ContextMenu::setContextList(const std::string& text, CallablePtr callable)
    {
        addItemInContextMenu(text, callable);
    }

    void ContextMenu::addItemInContextMenu(const std::string& text, CallablePtr callable)
    {
        auto addItem = makeTTFText(ecsRef, 0, 0, 11.0f, "res/font/Inter/static/Inter_28pt-Light.ttf", text, 1);
        auto addItemEntity = addItem.entity;

        ecsRef->attach<MouseLeftClickComponent>(addItemEntity, callable);
        auto addItemAnchor = ecsRef->attach<UiAnchor>(addItemEntity);

        if (not components.empty())
            addItemAnchor->setTopAnchor(components.back()->get<UiAnchor>()->bottom);
        else
            addItemAnchor->setTopAnchor(parentUi->top);

        addItemAnchor->setLeftAnchor(parentUi->left);

        auto addItemPos = addItem.get<PositionComponent>();
        if (addItemPos->width > parentPos->width)
        {
            parentPos->setWidth(addItemPos->width);

            for (auto comp : components)
            {
                comp->get<PositionComponent>()->setWidth(addItemPos->width);
            }
        }

        parentPos->setHeight(parentPos->height + addItemPos->height);

        components.push_back(addItemEntity);
    }

    void ContextMenu::hide()
    {
        LOG_THIS_MEMBER(DOM);

        parentPos->setVisibility(false);

        backgroundPos->setVisibility(false);

        for (auto& comp : components)
        {
            comp->get<PositionComponent>()->setVisibility(false);
        }
    }

    void ContextMenu::onEvent(const ShowContextMenu& event)
    {
        LOG_THIS_MEMBER(DOM);

        showEventQueue.push(event);
    }

    void ContextMenu::onEvent(const HideContextMenu& event)
    {
        LOG_THIS_MEMBER(DOM);

        hideEventQueue.push(event);
    }

    void ContextMenu::onEvent(const CreateElement& event)
    {
        elementQueue.push(event);
    }

    void ContextMenu::onEvent(const OpenFile&)
    {
        LOG_INFO("Context Menu", "Open file");

        char * lTheOpenFileName;
    	char const * lFilterPatterns[1] = { "*.sc" };

        lTheOpenFileName = tinyfd_openFileDialog(
            "Open a scene file",
            "", // Starting path
            1, // Number of patterns
            lFilterPatterns, // List of patterns
            "Scene files (.sc)",
            1);

        if (lTheOpenFileName)
        {
            LOG_INFO("Context Menu", lTheOpenFileName);
            ecsRef->sendEvent(LoadScene{lTheOpenFileName});
        }
    }

    void ContextMenu::onEvent(const SaveFile&)
    {
        LOG_INFO("Context Menu", "Save file");

        char * lTheSaveFileName;
    	char const * lFilterPatterns[1] = { "*.sc" };

        lTheSaveFileName = tinyfd_saveFileDialog(
            "Save a scene file",
            "./scene.sc",
            1,
            lFilterPatterns,
            NULL);

        if (lTheSaveFileName)
        {
            LOG_INFO("Context Menu", lTheSaveFileName);
            ecsRef->sendEvent(SaveScene{lTheSaveFileName});
        }
    }

    void ContextMenu::execute()
    {
        while (not elementQueue.empty())
        {
            LOG_INFO("Context Menu", "Create scene element");

            auto& event = elementQueue.front();

            switch(event.type)
            {
                case UiComponentType::TEXT:
                {
                    auto newElement = makeTTFText(ecsRef, currentX, currentY, 0.0f, "res/font/Inter/static/Inter_28pt-Light.ttf", "New Text", 1);
                    ecsRef->attach<SceneElement>(newElement.entity);
                    break;
                }

                case UiComponentType::TTFTEXT:
                {
                    auto newElement = makeTTFText(ecsRef, currentX, currentY, 0.0f, "res/font/Inter/static/Inter_28pt-Light.ttf", "New Text", 1);
                    ecsRef->attach<SceneElement>(newElement.entity);
                    break;
                }

                case UiComponentType::TEXTURE:
                {
                    auto newElement = makeUiTexture(ecsRef, 50, 50, "TabTexture");
                    ecsRef->attach<SceneElement>(newElement.entity);
                    break;
                }

                case UiComponentType::TEXTINPUT:
                {
                    auto newElement = makeTextInput(ecsRef, 50, 50, StandardEvent("nocallback"), {"TabTexture"});
                    ecsRef->attach<SceneElement>(newElement.entity);
                    break;
                }

                default:
                    break;
            }

            hide();
            elementQueue.pop();
        }

        while (not hideEventQueue.empty())
        {
            LOG_INFO("Context Menu", "Hide context");

            hide();
            hideEventQueue.pop();
        }

        while (not showEventQueue.empty())
        {
            auto& event = showEventQueue.front();

            LOG_INFO("Context Menu", "Show context");

            auto pos = event.inputHandler->getMousePos();

            parentPos->setVisibility(true);

            currentX = pos.x;
            currentY = pos.y;

            parentPos->setX(pos.x);
            parentPos->setY(pos.y);

            backgroundPos->setVisibility(true);

            for (auto& comp : components)
            {
                comp->get<PositionComponent>()->setVisibility(true);
            }

            showEventQueue.pop();
        }
    }

}
}