#pragma once

#include "ECS/system.h"

#include "2D/position.h"
#include "UI/prefab.h"
#include "UI/sizer.h"

namespace pg
{
    struct FoldCardEvent
    {
        _unique_id id;
    };

    struct FoldCardSystem : public System<Listener<FoldCardEvent>>
    {
        void onEvent(const FoldCardEvent& event)
        {
            auto ent = ecsRef->getEntity(event.id);

            if (not ent or not ent->has<Prefab>())
                return;

            ent->get<Prefab>()->callHelper("toggleCard");
        }
    };


    template <typename Type>
    CompList<Prefab, UiAnchor, VerticalLayout> makeFoldableCard(Type *ecsRef, const std::string& cardTitle = "New Card")
    {
        auto prefabEnt = makeAnchoredPrefab(ecsRef, 100, 100, 1);
        auto prefab = prefabEnt.template get<Prefab>();
        auto prefabAnchor = prefabEnt.template get<UiAnchor>();

        auto mainLayoutEnt = makeVerticalLayout(ecsRef, 0, 0, 250, 0);
        auto mainLayout = mainLayoutEnt.template get<VerticalLayout>();
        auto mainLayoutAnchor = mainLayoutEnt.template get<UiAnchor>();
        mainLayout->setScrollable(false);

        prefabAnchor->setWidthConstrain(PosConstrain{mainLayout.entityId, AnchorType::Width});
        prefabAnchor->setHeightConstrain(PosConstrain{mainLayout.entityId, AnchorType::Height});

        mainLayoutAnchor->setTopAnchor(prefabAnchor->top);
        mainLayoutAnchor->setLeftAnchor(prefabAnchor->left);
        mainLayoutAnchor->setZConstrain(PosConstrain{prefabEnt.id, AnchorType::Z});

        prefab->addToPrefab(mainLayoutEnt, "MainEntity");

        auto titleBg = makeUiSimple2DShape(ecsRef, Shape2D::Square, 250, 50, {55, 55, 125, 255});
        titleBg.template attach<MouseLeftClickComponent>(makeCallable<FoldCardEvent>(FoldCardEvent{prefabEnt.id}));
        auto titleBgAnchor = titleBg.template get<UiAnchor>();

        auto title = makeTTFText(ecsRef, 0, 0, 2, "light", cardTitle, 0.4f);
        auto titleAnchor = title.template get<UiAnchor>();

        prefab->addToPrefab(title, "CardTitle");

        titleAnchor->setVerticalCenter(titleBgAnchor->verticalCenter);
        titleAnchor->setLeftAnchor(titleBgAnchor->left);
        titleAnchor->setLeftMargin(5);

        auto leftLineEnt = makeUiSimple2DShape(ecsRef, Shape2D::Square, 2, 2, {180, 180, 180, 255});
        auto leftLineAnchor = leftLineEnt.template get<UiAnchor>();

        auto layoutEnt = makeVerticalLayout(ecsRef, 0, 0, 250, 100);
        auto layout = layoutEnt.template get<VerticalLayout>();
        // layout->fitToAxis = true;
        layout->setScrollable(false);
        auto layoutAnchor = layoutEnt.template get<UiAnchor>();

        layoutAnchor->setZConstrain(PosConstrain{mainLayoutEnt.entity.id, AnchorType::Z, PosOpType::Add, 1});

        leftLineAnchor->setHeightConstrain(PosConstrain{layoutEnt.entity.id, AnchorType::Height});

        layoutAnchor->setTopAnchor(titleBgAnchor->bottom);
        layoutAnchor->setLeftAnchor(leftLineAnchor->right);
        layoutAnchor->setLeftMargin(5);

        prefab->addToPrefab(layoutEnt, "Layout");

        // Todo fix this (without the test here the child of the layout are invisible !)
        // auto test1 = makeTTFText(ecsRef, 0, 0, 2, "light", "Test 1", 0.5);
        // auto test2 = makeTTFText(ecsRef, 0, 0, 2, "light", "Test 2", 0.5);

        // layout->addEntity(test1);
        // layout->addEntity(test2);

        // Todo fix this this is a dirty fix for now
        layout->addEntity(makeSimple2DShape(ecsRef, Shape2D::Square, 1, 1));

        mainLayout->addEntity(titleBg);
        mainLayout->addEntity(leftLineEnt);
        // mainLayout->addEntity(layoutEnt);

        prefab->addHelper("toggleCard", [](Prefab *prefab) -> void {
            LOG_INFO("Foldable card", "Fold");

            auto leftLine = prefab->getEntity("MainEntity")->get<VerticalLayout>()->entities[1];

            auto leftLinePos = leftLine->template get<PositionComponent>();

            auto visi = leftLinePos->isVisible();

            leftLinePos->setVisibility(not visi);

            auto vLayoutEnt = prefab->getEntity("Layout");

            auto vLayoutPos = vLayoutEnt->template get<PositionComponent>();

            vLayoutPos->setVisibility(not visi);
        });

        return {prefabEnt, prefab, prefabAnchor, layout};
    }
}