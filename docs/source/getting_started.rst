Getting Started
===============

Welcome to **ColumbaEngine**! This guide gets you from a clean machine to a running example, then points you at the template for starting your own game.

There are two ways to set up the engine:

1. **The install script (recommended)** — installs the engine system-wide (or to a prefix) and generates a ready-to-build starter app.
2. **Manual build from source** — builds the engine and all bundled examples in-tree.

Prerequisites
-------------

- A C++17 compiler (GCC or Clang; on Windows, MinGW-64)
- CMake 3.18+
- Git
- OpenGL drivers

All third-party libraries (SDL2, GLEW, GLM, FreeType, Taskflow, ...) are **vendored in the repository** — you do not need to install them yourself. On Linux you only need the system development headers the vendored SDL2 builds against:

.. code-block:: bash

    # Ubuntu / Debian
    sudo apt update
    sudo apt install build-essential cmake git \
        libgl1-mesa-dev libglu1-mesa-dev libx11-dev libxext-dev \
        libasound2-dev libpulse-dev libudev-dev pkg-config

Option 1 — Install script (recommended)
---------------------------------------

.. code-block:: bash

    curl -sSL https://raw.githubusercontent.com/Gallasko/ColumbaEngine/main/scripts/install/install-engine.sh | bash

The script installs dependencies for your distro (Ubuntu/Debian, Fedora, Arch), clones and builds the engine, installs it (default prefix ``/usr/local``; use ``--prefix ~/.local`` to avoid sudo), and creates a starter application in ``~/ColumbaEngine-install/test-app`` that you can build and run immediately:

.. code-block:: bash

    cd ~/ColumbaEngine-install/test-app
    ./build.sh
    cd build && ./ColumbaEngineTestApp

You should see a window with a bouncing box that changes color when it hits an edge. This starter app is the recommended base for a new project — it links the installed engine with ``find_package(ColumbaEngine)``.

Option 2 — Manual build from source
-----------------------------------

.. important:: Clone with ``--recursive`` — the dependencies are git submodules/vendored trees and the build will fail without them.

.. code-block:: bash

    git clone --recursive https://github.com/Gallasko/ColumbaEngine.git
    cd ColumbaEngine
    mkdir build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release ..
    cmake --build . -j$(nproc)

This builds the engine library plus the bundled examples (``BUILD_EXAMPLES`` is ON by default). There is no single ``ColumbaEngine`` binary — run one of the example executables from the build directory instead:

.. code-block:: bash

    ./BoxBouncer        # minimal bouncing-box demo
    ./RenderingTest     # 2D rendering feature showcase
    ./StandardSys       # standard-system / ECS patterns demo
    ./Asteroid          # small complete game

Useful CMake options:

- ``-DBUILD_EXAMPLES=OFF`` — build only the engine library
- ``-DBUILD_STATIC_LIB=ON`` — build a static library (used by the installer)
- ``-DBUILD_EDITOR=OFF`` — skip the (early-preview) scene editor, built natively as ``ColumbaEngineEditor``
- ``-DPG_PROFILE=ON`` — enable system profiling

Building for the Web (Emscripten)
---------------------------------

The engine treats WebAssembly as a first-class target:

.. code-block:: bash

    # Install the Emscripten SDK first: https://emscripten.org/docs/getting_started/downloads.html
    cd ColumbaEngine
    mkdir build-web && cd build-web
    emcmake cmake ..
    cmake --build . -j
    emrun ./BoxBouncer.html

Each example is emitted as an ``.html`` + ``.wasm`` bundle you can serve directly.

Where to go next
----------------

- Follow the :doc:`tutorials` to build your first game step by step.
- Read the `PgScript quick start <https://github.com/Gallasko/ColumbaEngine/blob/main/docs/compiler/QUICK_START.md>`_ to script game logic with hot reload.
- Browse the ``examples/`` directory — each subfolder is a self-contained demo or game.

Building the Documentation (Optional)
-------------------------------------

.. code-block:: bash

    pip install sphinx
    cd docs
    make html

The HTML output lands in ``_build/html/``.

Common Issues
-------------

1. **Build fails with missing headers or submodule errors**
    You almost certainly cloned without ``--recursive``. Run ``git submodule update --init --recursive`` and re-run CMake.

2. **CMake version too old**
    The build requires CMake 3.18+ (``cmake --version``).

3. **Runtime errors / black window**
    Update your graphics drivers and check your OpenGL version.

Contact and Support
-------------------

Open an issue on the `GitHub Issues page <https://github.com/Gallasko/ColumbaEngine/issues>`_ or join the `Discord community <https://discord.gg/un4VtehX3W>`_.
