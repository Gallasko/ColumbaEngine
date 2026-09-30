# Chronicle prefab files

A `.yaml` file in this folder describes one UI tree. The engine parses it with
`tools/yaml_parser.pg` (run on the embedded PgScript VM through
`res/scripts/ui_loader.pg`), maps it onto a `pg::NodeSpec`, and `pg::buildTree`
realises it through the Chronicle prefab factories
(`examples/Chronicle/UI/factories.cpp`).

```cpp
auto spec = pg::loadNodeSpec(ecs, "res/chronicle/ui/skills.yaml");
pg::EntityRef page = pg::buildTree(ecs, *spec);
auto fed = page->get<pg::Prefab>()->findEntity("fed")->get<pg::Prefab>();
fed->callHelper("setItem", size_t{0}, 18, 18);   // the runtime setters are helpers on the piece's prefab
```

## Shape

One file holds one root node. A node is a map:

| key | meaning |
|---|---|
| `kind` | the factory: `Label`, `Mark`, `MarkedLabel`, `Ornament`, `Panel`, `Button`, `Tabs`, `Gloss`, `ProgressRule`, `StatLine`, `RequirementList`, `LifeClock`, `ActivityRow`, `ActivityList`, `ActivityGroup`, `WindowMeter`, or the engine's `Shape2D`, `Texture`, `TTFText`, `Layout:Vertical`, `Layout:Horizontal`. Omitted: a bare container (its children keep their own `x` / `y`). |
| `name` | registers the entity on the enclosing prefab (`getEntity(name)`), which also reaches the piece's helpers. |
| `x`, `y` | places the node (useful on roots and on children of a bare container). |
| `anchors` | list of `{side, target, targetSide?, margin?}`; `target` is `main`, `parent`, a sibling's name or a globally named entity; sides are the `AnchorType` names, any case. |
| `children` | the subtree, in order. |
| `flow`, `padding`, `spacing` | the engine's build-time flow sugar (`flow: vertical` / `horizontal`). |
| anything else | a prop of the kind. A scalar goes to `props`; a list of maps (or one nested map) goes to `records`. |

Scalars: `12` is an int, `12.5` a double, `true` / `false` a bool, everything else
a string (quote it when it holds `: ` or starts with `[`, `{`, `"`). A
double-quoted string may span lines until a line that ends with `"`. `#` starts a
comment.

## Conventions of the Chronicle kinds

- Enum props are lowercase strings: `frame: ruled`, `overflow: wrap`, `align: right`,
  `variant: seal`, `weight: hair`, `tone: gold`, `corner: tl`, `gloss: tooltip`.
- Where the spec field is itself called `kind`, the prop is renamed: `ornament: divider`
  and `gloss: margin`. `size: 16` picks a `MarkSize`. `name` is the node's handle, so an
  `ActivityRow`'s display name is `label: Train at the yard`, and a `WindowMeter`'s is
  `label: Squire`. `WindowMeter.state` is `upcoming`, `open` or `closed`.
- Colours are token names (`ink-muted`, `status-gain`); an unknown token is logged and
  the default kept. Numeric props also take a spacing token: `gap: space-2`.
- **Panel** hands `width: innerWidth` and `z: contentZ` (default `z + 10`) down to its
  children and puts them in its body layout, so the rows never spell those out, and
  a nested panel lands in the next z band on its own. An explicit child value wins.
- **ActivityList** does the same with `width` and `z`. Its children are `ActivityGroup`s
  and `ActivityRow`s **side by side**: a group is a heading (`label`), and the rows that
  follow it, up to the next heading, are its rows. A group takes no children. The list
  stripes and owns its rows from the next frame. `height: 560` makes the list keep that
  height and scroll its rows; without it the list is as tall as its rows.
  `ActivityRow.state` is `idle`, `running` or `locked`; the events carry the list's `id`
  and the row's `id`. A row is reached by its `name`, or with `list->row(ecs, id)`.
- Lists: `Tabs.items` (`label`, `glyph`, `badge`), `RequirementList.items` (`label`,
  `current`, `needed`, `met` as a bool), `Gloss.rows` (`label`, `value`),
  `LifeClock.milestones` (`age`, `label`), `LifeClock.windows` (`from`, `to`, `label`,
  `closed`), `ActivityRow.gains` (`stat`, `amount`), `ActivityRow.requirements` (as
  `RequirementList.items`), and `MarkedLabel.label` as a nested map (or flat `text` / `style` / `color` on the node).
- Any node takes `theme: <element key>` next to `kind` and `name`: the builder attaches a
  `ThemeComponent` to the node's leaf, so an engine `Shape2D` or `TTFText` is painted by the
  theme like the kit pieces are (`theme: scene.background`, `theme: label.caption.ink-muted`).
- Events stay tag-based: `Button.tag` rides `ButtonActivatedEvent`, `Tabs.tag` rides
  `TabSelectedEvent`, `StatLine.glossKey` attaches a registered gloss.
- No data bindings in files: name the node and drive its helpers from C++
  (see `Scenes/prefabfilegallery.cpp`).

## Example

```yaml
kind: Panel
name: squire
x: 372
y: 90
frame: illuminated
width: 360
heading: Squire
glyph: training
aside: PATH
children:
  - kind: RequirementList
    name: fed
    items:
      - {label: Strength, current: 12, needed: 18}
      - {label: "Has the Guild's letter", met: false}
  - kind: Button
    variant: seal
    label: Train a season
    glyph: time
    months: 3
    tag: gallery.train
```

## Files

| file | scene | what |
|---|---|---|
| `prefabgallery.yaml` | `Chronicle --dev PrefabFileGallery` | the reference page: one of every kind |
| `clockgallery.yaml` | `Chronicle --dev ClockGallery` | four `LifeClock`s: the Life screen's, a first frame, an old age, a narrow one |
| `activitygallery.yaml` | `Chronicle --dev ActivityGallery` | the Life screen's activity list in five groups, scrolling, and the side panel showing the activity at work or the one chosen |
| `windowgallery.yaml` | `Chronicle --dev WindowGallery` | the doors that close: three `WindowMeter`s (open, upcoming, closed) in a ruled panel, the same three at 480 on vellum, and one with a two-line note |

The file format and the builder are documented in `docs/PREFAB_TREES.md`.
