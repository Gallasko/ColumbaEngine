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
    CompList<Prefab, UiAnchor, VerticalLayout> makeFoldableCard(Type *ecsRef)
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

        auto title = makeTTFText(ecsRef, 0, 0, 2, "light", "Card Name", 0.4f);
        auto titleAnchor = title.template get<UiAnchor>();

        titleAnchor->setVerticalCenter(titleBgAnchor->verticalCenter);
        titleAnchor->setLeftAnchor(titleBgAnchor->left);
        titleAnchor->setLeftMargin(5);

        auto layoutEnt = makeVerticalLayout(ecsRef, 0, 0, 250, 100);
        auto layout = layoutEnt.template get<VerticalLayout>();
        // layout->fitToAxis = true;
        layout->setScrollable(false);
        auto layoutAnchor = layoutEnt.template get<UiAnchor>();

        auto test1 = makeTTFText(ecsRef, 0, 0, 2, "light", "Test 1", 0.5);
        auto test2 = makeTTFText(ecsRef, 0, 0, 2, "light", "Test 2", 0.5);

        layout->addEntity(test1);
        layout->addEntity(test2);

        mainLayout->addEntity(titleBg);
        mainLayout->addEntity(layoutEnt);

        prefab->addHelper("toggleCard", [](Prefab *prefab) -> void {
            LOG_INFO("Foldable card", "Fold");
            auto vLayoutEnt = prefab->getEntity("MainEntity")->template get<VerticalLayout>()->entities[1];

            auto vLayoutPos = vLayoutEnt->template get<PositionComponent>();

            auto visible = vLayoutPos->isVisible();

            vLayoutPos->setVisibility(not visible);
        });

        return {prefabEnt, prefab, prefabAnchor, layout};
    }
}