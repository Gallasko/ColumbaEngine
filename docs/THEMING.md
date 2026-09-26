# Theming

ColumbaEngine paints UI from a theme file. An entity carries a `ThemeComponent` holding one string, an
element key; the `ThemeSystem` resolves that key in the loaded theme to a set of role values (color, alpha,
font, radius, border) and applies them to the drawables found beside the component. Switching the theme
repaints every themed entity.

Source: `src/Engine/UI/theme.h` (the data), `src/Engine/UI/themesystem.h` (the system),
`src/Engine/Components/ThemeComponent.pgcomp` (the component). A complete theme file is
`res/chronicle/tokens.json`; the editor's is `res/editor/theme.json`.

## Table of Contents

- [Using it](#using-it)
- [The theme file](#the-theme-file)
- [Elements and keys](#elements-and-keys)
- [Role values](#role-values)
- [Themable drawables](#themable-drawables)
- [Prefab files](#prefab-files)
- [Events](#events)
- [Script bindings](#script-bindings)

## Using it

The engine boot creates the `ThemeSystem` last among the UI systems. An app loads its file after creating
its `TTFTextSystem` (the theme registers one font atlas per text style on it), then paints by attaching
components:

```cpp
ecs.createSystem<TTFTextSystem>(window.masterRenderer);

auto theme = ecs.getSystem<ThemeSystem>();
theme->loadTheme("res/chronicle/tokens.json", "res/font");
theme->setTheme("candle");

auto ground = makeUiSimple2DShape(&ecs, Shape2D::Square, 320.0f, 200.0f);
ecs.attach<ThemeComponent>(ground.entity, "panel.ground");

auto title = theme->makeText(&ecs, "label.heading.ink", "North Forest");

ground.entity->get<ThemeComponent>()->setElement("panel.ground.hover");
```

Token values for build-time geometry come from the same system: `theme->space(3)`, `theme->radius("radius-md")`,
`theme->style("body").lineHeightPx`.

## The theme file

One JSON file holds every theme variant. Every token value is either a scalar, the same for every theme, or an
object keyed by theme id with an optional `"default"` entry. Resolution per theme: the theme id, then
`"default"`, then an error listed by `Theme::errors()`.

```
{
  "themes": [ { "id": "day" }, { "id": "candle" } ],
  "color": { "tokens": [
    { "name": "ink", "value": { "day": "#2b2219", "candle": "#eadfc8" } },
    { "name": "status-gain", "value": "{verdigris}" }
  ] },
  "spacing": { "tokens": [ { "name": "space-3", "value": "12px" } ] },
  "border":  { "tokens": [ { "name": "border-rule", "value": "2px" } ] },
  "radius":  { "tokens": [ { "name": "radius-md", "value": "3px" } ] },
  "opacity": { "tokens": [ { "name": "opacity-hatch", "value": "0.4" } ] },
  "type": {
    "fonts": [ { "family": "text", "weight": 400, "italic": false, "file": "EBGaramond/EBGaramond-Regular.ttf" } ],
    "groups": [ { "family": "text", "styles": [
      { "name": "body", "fontSize": "16px", "lineHeight": "24px", "fontWeight": 400 },
      { "name": "caption", "fontSize": { "default": "11px", "candle": "12px" }, "lineHeight": "15px", "letterSpacing": "0.04em" }
    ] } ]
  },
  "elements": { ... }
}
```

- **Colors** are `#rgb`, `#rrggbb`, `#rrggbbaa` or `rgba(r,g,b,a)`, or an alias `{name}` (chains up to 8
  deep, cycles reported). Every color is resolved for every theme at load time; an unknown name paints
  magenta and logs once. The `themes` list may also sit under `color` (older files).
- **Scales** are pixels (`"12px"` or a number) except `opacity`, a number in `[0, 1]`.
- **Text styles**: each field accepts the per-theme rule. A style rasterises as one atlas named after the
  style (`body`) when it is the same in every theme, or one atlas per theme (`caption@candle`) when it
  differs. `type.fonts` maps family, weight and italic to a file under the font root given to `loadTheme`.

## Elements and keys

The `elements` section maps a key to the role values an entity with that key receives:

```
"elements": {
  "label":                    { "font": "body", "color": "ink" },
  "label.caption":            { "font": "caption" },
  "panel.ground":             { "color": "folio" },
  "panel.frame.ruled":        { "color": "rule-ruled", "border": 1 },
  "button.seal.ground":       { "color": "vermilion", "radius": "radius-md" },
  "button.seal.ground.hover": { "color": { "default": "vermilion-hover", "candle": "vermilion" } },
  "button.seal.sheen":        { "color": "on-vermilion", "alpha": 0 },
  "button.seal.sheen.hover":  { "alpha": "opacity-ghost" },
  "disabled":                 { "alpha": "opacity-locked" },
  "special":                  { "extends": "button.seal.ground", "color": "#ff0000" }
}
```

A key resolves along its dotted prefix chain, root first, deeper prefixes overriding: `button.seal.ground.hover`
merges `button`, `button.seal`, `button.seal.ground` and `button.seal.ground.hover`. For each prefix:

- a defined element is merged, after its `extends` chain;
- an undefined prefix whose last segment names a **color token** sets `color` to it: `label.caption.ink-muted`
  paints the caption style in `ink-muted` with no entry in the file;
- an undefined prefix whose last segment names a **root element** mixes it in: `button.seal.ground.disabled`
  takes the `disabled` alpha;
- anything else is skipped, so an undefined state (`button.seal.frame.hover`) falls back to its base.

A key that matches nothing resolves empty and logs once. Systems that pick a state (hover, pressed, disabled)
call `setElement` with the state key; the theme file only lists what differs.

## Role values

A string value is a token name or a color literal; a number is a literal. `alpha` may be a number or an
opacity token, and scales the color's alpha channel.

| Key | Applies to | Effect |
|---|---|---|
| `color` | Simple2DObject, RoundedRect2DObject, HatchRect2DObject, DottedLine2DObject, StrokeRect2DObject, IconComponent, TTFText | `setColors(color * alpha)` |
| `alpha` | the same, plus Texture2DComponent (`setOpacity`) | literal or opacity token |
| `font` | TTFText | text style: font alias for the current theme, letter spacing, line spacing |
| `radius` | RoundedRect2DObject, StrokeRect2DObject | `setCornerRadius` |
| `border` | StrokeRect2DObject | `setStrokeWidth` |
| `<Type>.<key>` | one drawable | overrides `<key>` for that component type (`TTFText.color`) when an entity carries two drawables |

## Themable drawables

`ThemeSystem::init` pairs `ThemeComponent` with each engine drawable whose system exists at that point.
`TTFTextSystem::init` registers its own pairing when it is created later. An app does the same for its own
drawables:

```cpp
theme->registerThemable<Glow>([](EntityRef entity, const ElementMap& map, const ThemeSystem& theme) {
    entity->get<Glow>()->level = theme.resolveAlpha(map, Glow::getType());
});
```

`registerThemable<Comp>()` with no argument is the default pairing: `color` and `alpha` through `setColors`.
The helpers `entry`, `resolveColor`, `resolveAlpha` and `resolveScale` do the type-prefixed lookup and the
token or literal conversion. A pairing is skipped when no system owns the component type yet.

## Prefab files

A node of a prefab tree (`docs/PREFAB_TREES.md`) has a `theme` field next
to `kind` and `name`: the element key the leaf is painted with. The builder attaches a `ThemeComponent` with it
to the leaf the node's factory produced, for every kind.

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

A `TTFText` node needs no `font` when its element carries one: the theme sets the face, the letter spacing and
the line spacing on attach.

## Events

- `SetThemeEvent{id}`: anyone may send it; the system switches.
- `ThemeChangedEvent{id}`: sent by the system after `setTheme`, in the same frame. The repaint of every
  themed entity happens in the system's execute slot; a `setElement` repaints on the next frame.

`setTheme` is a no-op for the current theme and logs an unknown id. The current id is saved with the
system (`SaveSys`).

## Script bindings

The legacy interpreter module `src/Engine/Systems/thememodule.h` (`import "theme"`) exposes
`setCurrentTheme(name)`, `getCurrentTheme()`, `hasTheme(name)`, `loadTheme(path)`, `getThemeColor(token)`
(`"#rrggbbaa"`), `getThemeSpacing`, `getThemeBorder`, `getThemeRadius` and `getThemeOpacity`.
