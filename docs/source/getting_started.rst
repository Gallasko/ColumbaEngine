Getting Started
===============

.. autosummary::
    :toctree: generated

Welcome to the **ColumbaEngine** project! This guide will help you get started with setting up and using the engine. Whether you're a contributor or just interested in testing out the engine, this section will cover everything you need to know to get up and running.

Prerequisites
-------------

Before you begin, ensure you have the following installed on your machine:

1. **C++ Compiler**
    Make sure you have a C++17-compatible compiler installed (e.g., GCC, Clang, or MSVC).

2. **CMake**
    You'll need CMake to configure and build the engine. Download and install CMake from `https://cmake.org/download/`.

3. **OpenGL**
    ColumbaEngine uses OpenGL for rendering. Ensure your system has a compatible GPU and OpenGL drivers installed.

You can install most of these dependencies using a package manager (like `apt`, `brew`, or `vcpkg`) or by following individual library instructions.

Clone the Repository
--------------------

Start by cloning the repository to your local machine:

.. code-block:: bash

    git clone https://github.com/Gallasko/ColumbaEngine.git
    cd ColumbaEngine


Build Instructions
------------------

Using CMake
^^^^^^^^^^^

Once the repository is cloned, you can build the project with CMake:

1. Create a build directory:

.. code-block:: bash

    mkdir build
    cd build


2. Configure the project with CMake:

.. code-block:: bash

    cmake ..

This will check for all dependencies and prepare the build configuration.

3. Build the project:

.. code-block:: bash

    cmake --build .

4. Once the build process is complete, you'll have an executable located in the `build` directory.

Running the Engine
^^^^^^^^^^^^^^^^^^

To run the engine, simply execute the compiled binary:

.. code-block:: bash

    ./ColumbaEngine

This will launch the engine, and you'll be able to start testing your game projects or use the editor.

Building the Documentation (Optional)
--------------------------------------

If you're interested in contributing to the documentation or building it locally, you can do so by following these steps:

1. Install **Sphinx**:

.. code-block:: bash

    pip install sphinx

2. Navigate to the `docs` directory:

.. code-block:: bash

    cd docs

3. Build the documentation:

.. code-block:: bash

    make html

This will generate the HTML version of the documentation in the `_build/html/` directory, which you can view locally.

Contributing
------------

If you'd like to contribute to **ColumbaEngine**, you're welcome to open issues, submit pull requests, or provide feedback!

To get started with development, clone the repository, and make sure to follow the **contributing guidelines** outlined in the `CONTRIBUTING.md <https://github.com/Gallasko/ColumbaEngine/blob/main/CONTRIBUTING.md>`_.

Common Issues
-------------

1. **Missing Dependencies**
    If you encounter issues during the build, make sure that all required dependencies (like SDL2, GLEW, Assimp, etc.) are installed. Use the package manager for your system to install them, or follow the installation instructions for each library.

2. **Build Failures**
    If you run into build failures, try cleaning the build directory and running CMake again:

.. code-block:: bash

    rm -rf build
    mkdir build
    cd build
    cmake ..
    cmake --build .

3. **Runtime Errors**
    If the engine fails to run, ensure that your graphics drivers and OpenGL version are up to date.

Contact and Support
-------------------

If you have questions or run into issues, feel free to open an issue on the `GitHub Issues page <https://github.com/Gallasko/ColumbaEngine/issues>`_ or reach out to the maintainers for help.
