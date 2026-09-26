#pragma once

#include <string>
#include <vector>

#include "UI/prefabfactory.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"

namespace chronicle
{
    // Registers every Chronicle kit piece as a prefab factory, so a NodeSpec tree (hand-built
    // or loaded from a .yaml file through pg::loadNodeSpec) can be realised with pg::buildTree.
    //
    // Kinds: Label, Mark, MarkedLabel, Ornament, Panel, Button, Tabs, Gloss, ProgressRule,
    // StatLine, RequirementList. Each maps its props onto the matching XSpec and calls the
    // existing makeX; the result struct is returned as the node's handle, so
    // `built.get<Panel>("skills")` keeps every runtime setter.
    //
    // Conventions shared by every kind:
    //   - enum props are lowercase strings (`frame: ruled`, `overflow: wrap`, `variant: seal`);
    //   - colours are token names, checked against the Tokens (unknown -> logged, default kept);
    //   - numeric props also accept a spacing token (`gap: space-2`);
    //   - `z` is an int; Panel hands `z: contentZ` and `width: innerWidth` down to its children
    //     and exposes its body layout as the slot the children go into;
    //   - list props come through NodeSpec::records: Tabs `items`, RequirementList `items`,
    //     Gloss `rows`, and MarkedLabel's nested `label` map.
    //
    // Renamed keys (the node's `kind` is taken): Ornament's kind is `ornament`, Gloss's kind
    // is `gloss`.
    void registerChronicleFactories(pg::PrefabFactoryRegistry* registry, const Tokens* tokens, const TextStyles* styles);

    // The kind names registered above, in registration order.
    const std::vector<std::string>& chronicleKinds();
}
