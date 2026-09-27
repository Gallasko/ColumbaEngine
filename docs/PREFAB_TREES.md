# Prefab Trees

A prefab tree describes a piece of UI as data: a tree of nodes, each naming a kind, its parameters, its
children and how they are placed. The builder turns the tree into live entities through factories registered
by name, so the same file builds engine primitives and a game's own components. The source is
`src/Engine/UI/prefabspec.h` (the tree), `src/Engine/UI/prefabfactory.h` (the factories),
`src/Engine/UI/prefabbuilder.h` (the builder) and `src/Engine/UI/prefabloader.h` (the YAML loader).
A complete example is `res/chronicle/ui/prefabgallery.yaml`, shown by `Chronicle --dev PrefabFileGallery`.

## Table of Contents

- [Quick start](#quick-start)
- [The node](#the-node)
- [Kinds and what they produce](#kinds-and-what-they-produce)
- [Props and records](#props-and-records)
- [Placement and anchors](#placement-and-anchors)
- [Flow](#flow)
- [Layout nodes](#layout-nodes)
- [Names and helpers](#names-and-helpers)
- [Theme](#theme)
- [Inheritance](#inheritance)
- [The YAML file](#the-yaml-file)
- [Writing a factory](#writing-a-factory)
- [Engine kinds](#engine-kinds)

## Quick start

A file:

```
kind: Panel
name: skills
x: 48
y: 90
frame: ruled
width: 300
heading: Skills
theme: panel.ground
children:
  - {kind: Label, text: Strength}
  - {kind: Ornament, ornament: divider, knot: false}
  - kind: Label
    name: note
    overflow: wrap
    color: ink-muted
    text: A wrapped body line takes the panel's inner width.
```

Loading and building it:

```cpp
PrefabLoadOptions options;
std::vector<std::string> errors;
options.errors = &errors;

auto spec = loadNodeSpec(ecs, "res/chronicle/ui/prefabgallery.yaml", options);

if (spec)
{
    EntityRef page = buildTree(ecs, *spec);

    auto skills = page->get<Prefab>()->getEntity("skills")->get<Prefab>();
    skills->callHelper("setHeading", std::string("Parts"));
}
```

The registry of factories must exist first: `ecs->createSystem<PrefabFactoryRegistry>()`, then
`registerEnginePrefabFactories(registry)` for the engine kinds and the game's own registration
(`registerChronicleFactories(registry)` in Chronicle). The builder also needs `PositionComponentSystem` and
`PrefabSystem`, both created by the engine boot.

## The node

`NodeSpec` is the whole vocabulary:

| Field | Meaning |
|---|---|
| `kind` | which factory builds the leaf: an engine kind (`Shape2D`, `TTFText`, `Texture`, `Panel`, `Text`, `TitleBar`), a layout (`Layout:Vertical`, `Layout:Horizontal`), a game kind, or empty for a bare container |
| `name` | the node's name in the enclosing prefab's scope (`getEntity`, `findEntity`) |
| `theme` | the theme element key the leaf is painted with (see [Theme](#theme)) |
| `props` | the scalar parameters of the leaf, read by the factory |
| `records` | the list-shaped parameters: `records["items"]` is a list of flat maps |
| `anchors` | how this node is attached to a sibling, its parent or a named entity |
| `children` | nodes built inside this one |
| `flow`, `padding`, `spacing` | build-time sugar that anchors children in a row or a column |

In C++ a tree is built the same way the loader does it:

```cpp
NodeSpec label;
label.kind = "Label";
label.name = "title";
label.props = {{"text", std::string("Skills")}, {"style", std::string("heading")}};

NodeSpec root;
root.kind = "Panel";
root.name = "skills";
root.props = {{"width", 300.0f}, {"x", 48.0f}, {"y", 90.0f}};
root.children.push_back(label);
```

## Kinds and what they produce

Every node is wrapped: the builder creates a Prefab container (a `Prefab` + `UiAnchor` + `PositionComponent`
entity), asks the kind's factory for the leaf, and makes that leaf the container's main entity, anchored to
the container's top-left with the container sized to it. Children are built the same way and land beside the
leaf inside the container. This is the one rule to keep in mind: the thing you position is the container, the
thing you paint or read is the leaf. `buildTree` returns the root container, so `root->get<Prefab>()` always
works.

- A **non-empty kind** is looked up in the `PrefabFactoryRegistry`. Unknown kinds log and produce nothing.
- An **empty kind** produces a container with no leaf: a grouping node whose children keep the `x` and `y`
  they declare. The root of `prefabgallery.yaml` is one.
- A **layout kind** (`Layout:Vertical`, `Layout:Horizontal`) produces one entity carrying the layout component;
  its children are added to the layout and reflow at runtime. A nested layout sits inside its parent's
  container as is; a layout at the root gets a container of its own like every other node.

## Props and records

`props` is an `ElementMap`: string keys to `ElementType` scalars (int, float, double, bool, string). A factory
reads them with the helpers of `prefabfactory.h`: `hasParam`, `getParam`, `getParamFloat`, `getParamInt`,
`getParamBool`, `getParamString`. Each kind declares a `ParamSchema`: the props it understands, their type and
default, and whether they are required. Before the factory runs, the schema defaults are merged under the
node's props and a missing required prop fails the build with a log. Props the schema does not list pass
through untouched.

`records` holds list-shaped parameters. `records["items"]` is a `RecordList`, a vector of flat maps, and a
factory reads it as it likes: a tab row's `items`, a requirement list's `items`, a gloss's `rows`.

Two props are read by the builder itself on every wrapped node: `x` and `y` position the container.

## Placement and anchors

`anchors` attaches the node's container to something else. An `AnchorSpec` has:

- `side`: which side of this node is attached. `Top`, `Bottom`, `Left`, `Right` attach that edge;
  `Width`, `Height` constrain the size to the target's; `VerticalCenter`, `HorizontalCenter` centre the node on
  that axis of the target. `centerInAnchors(target)` returns the pair that fully centres a node.
- `target`: `main` (the parent node's leaf), `parent` (the parent's container), a sibling's `name`, or an
  entity registered in the global `EntityNameSystem`. `targetId` bypasses the lookup with an entity id.
- `targetSide`: the target's side, when it differs from `side` (`side: Top, targetSide: Bottom` stacks the node
  under its target).
- `margin`: the distance kept from the target.

Targets resolve in that order: id, then the local scope (siblings, `main`, `parent`), then the global names.
An unknown non-empty target logs and is skipped.

```
- kind: Label
  name: aside
  text: LEDGER
  anchors:
    - {side: right, target: parent, margin: 16}
    - {side: top, target: title, targetSide: top}
```

## Flow

`flow: vertical` or `flow: horizontal` on a node anchors its children one after another without writing the
anchors by hand: the first child sits `padding` away from the parent's leaf on both axes, every next child
follows the previous one at `spacing`. A child with anchors of its own is left out of the chain, like an
absolutely positioned element. Flow is baked at build time; use a layout node when children appear and
disappear at runtime or need to scroll.

## Layout nodes

`kind: Layout:Vertical` or `Layout:Horizontal` creates a real `VerticalLayout` or `HorizontalLayout` entity.
Its props: `x`, `y`, `width`, `height`, `z`, `visibility`, `scrollable`, plus the layout's own `spacing`,
`fitToAxis`, `spaced` and `stickToEnd`. Children are added with `addEntity` and the `LayoutSystem` places
them every frame. A layout declares no defaults for its children; it forwards what it inherited. A layout has
no `Prefab` of its own, so the names of its children are registered on the nearest enclosing prefab.

A factory can also expose a **slot**: a layout entity inside the leaf that receives the node's children
instead of the sibling-anchoring path. The Chronicle panel's body is one, which is why a panel's children in
the YAML need no anchors at all.

## Names and helpers

A named child registers under `name` in its parent's scope. `parent->get<Prefab>()->getEntity("bg")` returns
the child's leaf, not its container, so `->get<Simple2DObject>()` works directly on it. Anchoring to a sibling
by name targets that leaf as well. A composite piece (a leaf with a `Prefab` of its own, like every Chronicle
panel) also takes over the names of the nodes nested under it, so `getEntity("squire")->get<Prefab>()->getEntity("fed")`
walks the file's structure. `Prefab::findEntity(name)` searches the whole subtree when the path does not
matter.

A factory that builds a stateful piece attaches the piece's struct to the piece's entity as a component, so
the state lives on the entity: `page->get<Prefab>()->findEntity("fed")->get<chronicle::RequirementList>()`
is the list with every setter. On top of that, a composite piece registers its setters as **helpers** on its
own `Prefab` (`Prefab::addHelper`), stateless conveniences that reach the component by name:

```cpp
EntityRef page = buildTree(ecs, spec);

auto fed = page->get<Prefab>()->findEntity("fed")->get<Prefab>();

if (fed->hasHelper("setItem"))
    fed->callHelper("setItem", size_t{0}, 18, 18);

const size_t rows = fed->callHelper<size_t>("size");
```

Two rules of the helper registry: a helper's arguments are matched by exact type (an `int` literal does not
match a `size_t` parameter, a string literal does not match a `std::string` one), and an unknown name or a
type mismatch throws, so check with `hasHelper` when a file may or may not carry the piece. The Chronicle kit
registers the same setters its structs offer, minus the ecs argument (`examples/Chronicle/UI/factories.h`).
A single-entity piece such as a label needs no helper: change its `TTFText`, `IconComponent` or
`ThemeComponent` directly.

## Theme

`theme` is a node keyword like `kind` and `name`: the element key the leaf is painted with. The builder
attaches a `ThemeComponent` with it to the leaf the factory produced, and the `ThemeSystem` paints the leaf
from the loaded theme and repaints it on every switch. It works for every kind, which is how an engine
`Shape2D` or `TTFText` joins a themed page:

```
- kind: Shape2D
  name: page
  width: 1320
  height: 860
  theme: scene.background

- kind: TTFText
  text: every panel on this page comes from the file
  theme: label.caption.ink-muted
```

A `TTFText` needs no `font` when its element carries one. The element keys and the file that defines them are
described in `docs/THEMING.md`.

## Inheritance

A factory can hand props down to the node's children through `FactoryResult::childDefaults`. They are merged
under each child's own props, so an explicit child value always wins, and they are forwarded through layout
nodes and through kinds that declare none of their own. The Chronicle panel gives its children
`width: innerWidth` and `z: contentZ`; that is why a panel's rows never spell out a width.

## The YAML file

`loadNodeSpec(ecs, path, options)` parses the file with the PgScript YAML parser (`tools/yaml_parser.pg`,
driven by `res/scripts/ui_loader.pg` on a throwaway VM) and maps the result onto a `NodeSpec`:

- reserved keys become fields: `kind`, `name`, `theme` (strings), `anchors` (a list of `{side, target,
  targetSide, margin}` maps), `children` (a list of nodes), `flow` (`horizontal`, `vertical`, `none`),
  `padding` and `spacing` (numbers, also kept in props for layout kinds);
- every other scalar becomes a prop, keeping its type (`48` is an int, `56.5` a float, `true` a bool);
- a list of maps becomes `records[key]`; a single nested map becomes a one-record list;
- a list of scalars is reported and skipped.

The file holds one root node, not a list. Every problem is logged and, when `options.errors` is set,
collected there too. `nullopt` comes back only when the file cannot be read or is not a map.

## Writing a factory

A factory is a function registered under a kind name with its schema. It receives the whole `NodeSpec`
(props already merged with the schema defaults and the parent's inheritance, plus `records`, `name` and
`theme`; `children` are the builder's business) and returns a `FactoryResult`:

- `entity`: the leaf, empty on failure;
- `slot`: a layout entity that receives the node's children;
- `childDefaults`: props merged under every child's own.

A factory that produces one entity and nothing else wraps a props-in, entity-out function with `leafFactory`:

```cpp
registry->registerFactory("Badge", ParamSchema{{{"text", ""}, {"width", 24.0f}}}, leafFactory([](EntitySystem* ecs, const PrefabParams& p) -> EntityRef {
    auto shape = makeUiSimple2DShape(ecs, Shape2D::Square, getParamFloat(p, "width"), 16.0f);

    return shape.entity;
}));
```

A composite piece returns its slot and defaults, keeps its state on its entity as a component, and registers
its runtime setters as helpers on its own prefab. The helpers are stateless: they look the component up on
the prefab they were called on, and the `Prefab` knows the ECS, so they take no ecs argument:

```cpp
registry->registerFactory("Panel", std::move(schema), PrefabFactoryFn{[](EntitySystem* ecs, const NodeSpec& spec) -> FactoryResult {
    Panel panel = makePanel(ecs, panelSpecFrom(spec.props));

    auto prefab = panel.root->get<Prefab>();
    prefab->addHelper("setHeading", [](Prefab* p, const std::string& text) { p->ecsRef->getEntity(p->id)->get<Panel>()->setHeading(p->ecsRef, text); });

    FactoryResult r;
    r.entity = panel.root;
    r.slot = panel.body;
    r.childDefaults = {{"width", ElementType{panel.innerWidth()}}, {"z", ElementType{panel.spec.contentZ}}};

    ecs->attachGeneric<Panel>(panel.root, std::move(panel));

    return r;
}});
```

A plain struct attaches with `attachGeneric`; register its flag component once at setup
(`ecs->registerFlagComponent<Panel>()`) and give it an archive form (`serialize`/`deserialize` specialisations,
empty when the piece is never saved). The first parameter of a helper is always the `Prefab*` it was called
on; the registry adds it.

Conventions the Chronicle factories follow (`examples/Chronicle/UI/factories.cpp`): enum props are lowercase
strings, colour props are theme token names checked against the theme, numeric props also accept a spacing
token (`gap: space-2`), and a kind whose natural prop name collides with a reserved key renames it
(`ornament:` for the ornament's kind, `gloss:` for the gloss's kind).

## Engine kinds

Registered by `registerEnginePrefabFactories`:

| Kind | Props |
|---|---|
| `Shape2D` | `shape` (`Square`, `Circle`), `width`, `height`, `r` `g` `b` `a`, `x`, `y`, `z`, `viewport`, `visibility` |
| `Texture` | `texture`, `width`, `height`, `x`, `y`, `z`, `viewport`, `visibility` |
| `TTFText`, `Text` | `text`, `font`, `scale`, `r` `g` `b` `a`, `x`, `y`, `z`, `viewport`, `visibility` |
| `Panel` | a `Shape2D` backdrop: `shape`, `width`, `height`, `r` `g` `b` `a`, `z`, `viewport`, `centerInTarget` |
| `TitleBar` | a backdrop with a title text: `width`, `height`, `text`, `font`, `scale`, `padding`, the backdrop and text colours, `z`, `viewport` |

A game may re-register any of these under the same name with its own schema and factory. The Chronicle kit
registers `Label`, `Mark`, `MarkedLabel`, `Ornament`, `Panel`, `Button`, `Tabs`, `Gloss`, `ProgressRule`,
`StatLine` and `RequirementList`; `res/chronicle/ui/README.md` lists their props.
