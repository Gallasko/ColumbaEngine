#include "contextmenu.h"

#include <string>

#include "ECS/entitysystem.h"
#include "UI/sentencesystem.h"
#include "UI/textinput.h"
#include "2D/texture.h"
#include "Renderer/renderer.h"
#include "Scene/scenemanager.h"

#include "Helpers/tinyfiledialogs.h"

#include "2D/simple2dobject.h"

#include "inspector.h"

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

        auto file = makeTTFText(ecsRef, 10.0f, 5.0f, 12.0f, "light", "Open", 0.5);
        ecsRef->attach<MouseLeftClickComponent>(file.entity, makeCallable<OpenFile>());

        auto save = makeTTFText(ecsRef, 70.0f, 5.0f, 12.0f, "light", "Save", 0.5);
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
        backgroundPos->setZ(10);
        backgroundC = backTexture.get<UiAnchor>();
        backgroundC->fillIn(parentUi);

        auto vLayout = makeVerticalLayout(ecsRef, 1, 1, 200, 200);
        auto layoutPos = vLayout.get<PositionComponent>();
        layoutPos->setZ(11);
        auto layoutAnchor = vLayout.get<UiAnchor>();
        // layoutAnchor->fillIn(parentUi);

        layoutAnchor->setTopAnchor(parentUi->top);
        layoutAnchor->setLeftAnchor(parentUi->left);
        layoutAnchor->setWidthConstrain(PosConstrain{parent.id, AnchorType::Width});
        layoutAnchor->setHeightConstrain(PosConstrain{parent.id, AnchorType::Height});

        auto layoutComp = vLayout.get<VerticalLayout>();

        layoutComp->spacing = 8.0f;

        layout = vLayout.entity;

        setContextList("Add Sentence",  makeCallable<CreateElement>(UiComponentType::TEXT),
                       "Add TTF Text",  makeCallable<CreateElement>(UiComponentType::TTFTEXT),
                       "Add Texture",   makeCallable<CreateElement>(UiComponentType::TEXTURE),
                       "Add Shape 2D",  makeCallable<CreateElement>(UiComponentType::SHAPE2D),
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
        auto addItem = makeTTFText(ecsRef, 0, 0, 11.0f, "light", text, 0.5);
        auto addItemEntity = addItem.entity;

        ecsRef->attach<MouseLeftClickComponent>(addItemEntity, callable);

        LOG_ERROR("Context Menu", layout->has<VerticalLayout>());

        auto vLayout = layout->get<VerticalLayout>();

        vLayout->addEntity(addItemEntity);
    }

    void ContextMenu::hide()
    {
        LOG_THIS_MEMBER(DOM);

        parentPos->setVisibility(false);

        backgroundPos->setVisibility(false);

        layout->get<PositionComponent>()->setVisibility(false);
    }

    void ContextMenu::onProcessEvent(const ShowContextMenu& event)
    {
        LOG_THIS_MEMBER(DOM);

        LOG_INFO(DOM, "Show context menu");

        auto pos = event.inputHandler->getMousePos();

        parentPos->setVisibility(true);

        currentX = pos.x;
        currentY = pos.y;

        parentPos->setX(pos.x);
        parentPos->setY(pos.y);

        backgroundPos->setVisibility(true);

        layout->get<PositionComponent>()->setVisibility(true);
    }

    void ContextMenu::onProcessEvent(const HideContextMenu&)
    {
        LOG_THIS_MEMBER(DOM);

        LOG_INFO(DOM, "Hide context menu");

        hide();
    }

    void ContextMenu::onProcessEvent(const CreateElement& event)
    {
        auto cX = currentX, cY = currentY;
        switch(event.type)
        {
            case UiComponentType::TEXT:
            {
                ecsRef->sendEvent(CreateInspectorEntityEvent{[cX, cY](EntitySystem* ecsRef) -> EntityRef {
                    auto newElement = makeTTFText(ecsRef, cX, cY, 0.0f, "light", "New Text", 1);

                    return newElement;
                }});
                break;
            }

            case UiComponentType::TTFTEXT:
            {
                ecsRef->sendEvent(CreateInspectorEntityEvent{[cX, cY](EntitySystem* ecsRef) -> EntityRef {
                    auto newElement = makeTTFText(ecsRef, cX, cY, 0.0f, "light", "New Text", 1);

                    return newElement;
                }});
                break;
            }

            case UiComponentType::TEXTURE:
            {
                ecsRef->sendEvent(CreateInspectorEntityEvent{[cX, cY](EntitySystem* ecsRef) -> EntityRef {
                    auto newElement = makeUiTexture(ecsRef, 50, 50, "TabTexture");
                    newElement.get<PositionComponent>()->setX(cX);
                    newElement.get<PositionComponent>()->setY(cY);

                    return newElement;
                }});

                break;
            }

            case UiComponentType::SHAPE2D:
            {
                ecsRef->sendEvent(CreateInspectorEntityEvent{[cX, cY](EntitySystem* ecsRef) -> EntityRef {
                    auto newElement = makeUiSimple2DShape(ecsRef, Shape2D::Square, 50, 50, {0.f, 192.f, 0.f, 255.f});
                    newElement.get<PositionComponent>()->setX(cX);
                    newElement.get<PositionComponent>()->setY(cY);

                    return newElement;
                }});

                break;
            }

            case UiComponentType::TEXTINPUT:
            {
                ecsRef->sendEvent(CreateInspectorEntityEvent{[cX, cY](EntitySystem* ecsRef) -> EntityRef {
                    auto newElement = makeTextInput(ecsRef, 50, 50, StandardEvent("nocallback"), {"TabTexture"});

                    return newElement;
                }});

                break;
            }

            default:
                break;
        }

        hide();
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
    }

}
}