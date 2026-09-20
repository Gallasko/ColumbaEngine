# Chronicle

The Chronicle UI kit, built on ColumbaEngine. Everything here lives in the
`chronicle` namespace; engine types are used as `pg::…`.

## Running

Run from the repo root (assets are read relative to the working directory):

```
./build/Chronicle --dev TypeSpecimen          # the type/colour specimen dev scene
./build/Chronicle --dev TypeSpecimen --theme candle
```

`--dev <Scene>` picks a dev scene; an unknown name exits with code 2 before a
window opens. With no `--dev`, `TypeSpecimen` is the current default (the Life
scene replaces it in phase 2).

Tests:

```
cmake --build . --target test_chronicle
ctest -R "tokens|textstyle" --output-on-failure
```

## Layout

- `Core/` — data with no rendering: `tokens` (colours/spacing/borders/opacities,
  two themes, alias resolution), `textstyle` (one font atlas per style at exact
  pixel size), `fontfiles` (the family × weight × italic → ttf table).
- `Scenes/` — dev scenes. `TypeSpecimen` shows every style on vellum.
- `UI/` — components (added from phase 1.2 onward).

## Rules

- **`chronicle` namespace.** Everything under `examples/Chronicle/` is in
  `chronicle`; engine types are `pg::…`.
- **Promote only generic constructs.** Anything that is not Chronicle-specific
  (a shader, an engine component, a system) belongs in the engine, not here.
  Chronicle mentions no engine internals it did not put there.

## z-layer bands (all `int`)

| Band          | z range |
| ------------- | ------- |
| Page          | 0–9     |
| Panels        | 10–19   |
| Panel content | 20–59   |
| Banners       | 60–69   |
| Overlay       | 100–129 |
| Tooltip       | 200–209 |
| Debug         | 900–909 |

## Known gap G8: lining/tabular figures

`figure`, `figure-xl` and `tick` want tabular lining figures. EB Garamond's
default digits are old-style proportional, and FreeType alone cannot apply the
`lnum`/`tnum` OpenType features. Restoring them needs either a glyph-name remap
in `FontAtlas` (`one.lf`… if the face names them) or HarfBuzz; decide after the
Label brief. Old-style figures are readable in the meantime; the only thing
that depends on digit width is the `figure` no-reflow promise, which G8 restores.
