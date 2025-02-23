#include "contextmenu.h"

#include <string>

#include "ECS/entitysystem.h"
#include "UI/uisystem.h"
#include "UI/sentencesystem.h"
#include "UI/textinput.h"

// #include "Input/inputcomponent.h"
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
    
    // template <>
    // void renderer(MasterRenderer *masterRenderer, pg::editor::ContextMenu *contextMenu)
    // {
    //     // contextMenu->backgroundTextureC->render(masterRenderer);

    //     // contextMenu->addButtonButtonC->render(masterRenderer);
    //     // contextMenu->addTextureButtonC->render(masterRenderer);
    //     // contextMenu->addTextButtonC->render(masterRenderer);
    //     // contextMenu->addTextInputButtonC->render(masterRenderer);
    //     // contextMenu->addListButtonC->render(masterRenderer);
    // }

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
 
        auto file = makeSentence(ecsRef, 0, 0, {"Open"});
        file.get<UiComponent>()->setZ(12);
        ecsRef->attach<MouseLeftClickComponent>(file.entity, makeCallable<OpenFile>());

        auto save = makeSentence(ecsRef, 50, 0, {"Save"});
        save.get<UiComponent>()->setZ(12);
        ecsRef->attach<MouseLeftClickComponent>(save.entity, makeCallable<SaveFile>());

        parent = ecsRef->createEntity();

        parentUi = ecsRef->attach<UiComponent>(parent);

        parentUi->setZ(10);

        ecsRef->attach<MouseLeaveClickComponent>(parent, makeCallable<HideContextMenu>());

        auto backTexture = makeUiTexture(ecsRef, 1, 1, "TabTexture");

        auto background = backTexture.entity;

        backgroundC = backTexture.get<UiComponent>();

        backgroundC->fill(parentUi);
        
        // Todo move this in the ctor of the Context menu cause it is the only thing preventing this class to be generic
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
        auto addItem = makeSentence(ecsRef, 0, 0, {text});

        auto addItemEntity = addItem.entity;

        ecsRef->attach<MouseLeftClickComponent>(addItemEntity, callable);

        auto addItemC = addItem.get<UiComponent>();

        addItemC->setZ(11);

        if (components.size() > 0)
            addItemC->setTopAnchor(components.back()->bottom);
        else
            addItemC->setTopAnchor(parentUi->top);
        
        addItemC->setLeftAnchor(parentUi->left);

        if (addItemC->width > parentUi->width)
        {
            parentUi->setWidth(addItemC->width);

            for (auto& comp : components)
            {
                comp->setWidth(addItemC->width);
            }
        }

        parentUi->height += addItemC->height;

        components.push_back(addItemC);
    }

    void ContextMenu::hide()
    {
        LOG_THIS_MEMBER(DOM);

        parentUi->hide();

        backgroundC->hide();

        for (auto& comp : components)
        {
            comp->hide();
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
                    auto newElement = makeSentence(ecsRef, currentX, currentY, {"New Text"});

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

                    auto uiComp = newElement.get<UiComponent>();

                    uiComp->setX(currentX);
                    uiComp->setY(currentY);

                    ecsRef->attach<SceneElement>(newElement.entity);

                    break;
                }

                case UiComponentType::TEXTINPUT:
                {
                    auto newElement = makeTextInput(ecsRef, 50, 50, StandardEvent("nocallback"), {"TabTexture"});

                    auto uiComp = newElement.get<UiComponent>();

                    uiComp->setX(currentX);
                    uiComp->setY(currentY);

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

            parentUi->show();

            currentX = pos.x;
            currentY = pos.y;

            // Todo check for width / height overflow 

            parentUi->setX(pos.x);
            parentUi->setY(pos.y);

            LOG_INFO("Context Menu", "Parent: " << parentUi->pos.x << ", " << parentUi->pos.y << ", " << parentUi->width << ", " << parentUi->height);

            backgroundC->show();
            
            for (auto& comp : components)
            {
                comp->show();
            }

            showEventQueue.pop();
        }
    }

}
}