# StandardSys

**Build target:** `StandardSys`

A tour of the data-driven `createStandardSystem(...)` builder API — the way to declare systems without writing a class, and the main bridge between C++ and PgScript.

What it demonstrates:
- Building systems fluently: `onInit`, `onEvent`, `onExecute`, `onDelta`, `ownComponent`, `useStoragePolicy`
- Dynamic `StandardEvent`s and event-throughput measurement
- Script-backed handlers (e.g. `.onEvent("OnSDLScanCode", "test_key.pg")`) — these compile to bytecode and run on the PgScript VM
- The FSM helper (`FSMSystem` + `FiniteStateMachine`) reacting to key input
- Setting the script optimization level (`ecs.setVMOptimizationLevel(VmOptimizationLevel::O3)`)

Run from the build directory: `./StandardSys`

Read this one when you want scriptable game logic with hot reload.
