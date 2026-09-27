#pragma once

#include <string>
#include <vector>

#include "UI/prefabfactory.h"

namespace chronicle
{
    // Registers every Chronicle kit piece as a prefab factory, so a NodeSpec tree (hand-built
    // or loaded from a .yaml file through pg::loadNodeSpec) can be realised with pg::buildTree.
    //
    // Kinds: Label, Mark, MarkedLabel, Ornament, Panel, Button, Tabs, Gloss, ProgressRule,
    // StatLine, RequirementList, LifeClock. Each maps its props onto the matching XSpec and calls the
    // existing makeX, then attaches the result struct to the piece's entity as a component:
    // `getEntity("fed")->get<RequirementList>()->setItem(ecs, 0, 18, 18)` keeps every runtime
    // setter. The composite kinds also register those setters as helpers on their root prefab
    // (`getEntity("fed")->get<Prefab>()->callHelper("setItem", size_t{0}, 18, 18)`), with the
    // same arguments as the struct's method minus the ecs.
    //
    // Conventions shared by every kind:
    //   - enum props are lowercase strings (`frame: ruled`, `overflow: wrap`, `variant: seal`);
    //   - colours are token names, checked against the theme (unknown -> logged, default kept);
    //   - numeric props also accept a spacing token (`gap: space-2`);
    //   - `z` is an int; Panel hands `z: contentZ` and `width: innerWidth` down to its children
    //     and exposes its body layout as the slot the children go into;
    //   - list props come through NodeSpec::records: Tabs `items`, RequirementList `items`,
    //     Gloss `rows`, LifeClock `milestones` and `windows`, and MarkedLabel's nested `label` map.
    //
    // Renamed keys (the node's `kind` is taken): Ornament's kind is `ornament`, Gloss's kind
    // is `gloss`.
    void registerChronicleFactories(pg::PrefabFactoryRegistry* registry);

    // The kind names registered above, in registration order.
    const std::vector<std::string>& chronicleKinds();
}
