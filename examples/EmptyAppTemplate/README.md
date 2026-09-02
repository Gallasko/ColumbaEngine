# EmptyAppTemplate

**Build target:** `EmptyApp`

The bare in-tree skeleton: a `pg::Window`, an init thread, and a hand-written main loop with Emscripten support — and no game content at all. This is the *manual-boilerplate* path (~175 lines of `application.cpp`).

**Starting a new game? Use `/templates/quickstart` instead** — it links against an installed engine with `find_package(ColumbaEngine)` and uses `pg::Engine`, which encapsulates everything this template writes by hand (SDL init, the desktop and Emscripten main loops, save-on-close hooks). Keep EmptyAppTemplate as a reference for when you need full control over the window and loop, or when hacking on the engine itself in-tree.
