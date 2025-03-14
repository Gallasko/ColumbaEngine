```rst
Getting Started
===============

Welcome to the **PgEngine** project! This guide will help you get started with setting up and using the engine. Whether you're a contributor or just interested in testing out the engine, this section will cover everything you need to know to get up and running.

Prerequisites
-------------

Before you begin, ensure you have the following installed on your machine:

1. **C++ Compiler**  
   Make sure you have a C++17-compatible compiler installed (e.g., GCC, Clang, or MSVC).
   
2. **CMake**  
   You'll need CMake to configure and build the engine. Download and install CMake from `https://cmake.org/download/`.

3. **SDL2**  
   PgEngine relies on the SDL2 library for window management and rendering. You can install it through your system package manager or follow the installation instructions from the `SDL2 website <https://www.libsdl.org/>`_.

4. **GLFW**  
   PgEngine uses GLFW for OpenGL context management. You can install it via your package manager or visit `GLFW's website <https://www.glfw.org/>`_ for installation instructions.

5. **OpenGL**  
   PgEngine uses OpenGL for rendering. Ensure your system has a compatible GPU and OpenGL drivers installed.

6. **Other Dependencies**  
   - **GLEW** for OpenGL extension loading
   - **Assimp** for model loading
   - **tinygltf** for loading `.glb` files

You can install most of these dependencies using a package manager (like `apt`, `brew`, or `vcpkg`) or by following individual library instructions.

Clone the Repository
--------------------

Start by cloning the repository to your local machine:

```bash
git clone https://github.com/Gallasko/PgEngine.git
cd PgEngine
```

Build Instructions
------------------

### 1. Using CMake

Once the repository is cloned, you can build the project with CMake:

1. Create a build directory:

   ```bash
   mkdir build
   cd build
   ```

2. Configure the project with CMake:

   ```bash
   cmake ..
   ```

   This will check for all dependencies and prepare the build configuration.

3. Build the project:

   ```bash
   cmake --build .
   ```

4. Once the build process is complete, you'll have an executable located in the `build` directory.

### 2. Running the Engine

To run the engine, simply execute the compiled binary:

```bash
./PgEngine
```

This will launch the engine, and you'll be able to start testing your game projects or use the editor.

Building the Documentation (Optional)
--------------------------------------

If you're interested in contributing to the documentation or building it locally, you can do so by following these steps:

1. Install **Sphinx**:

   ```bash
   pip install sphinx
   ```

2. Navigate to the `docs` directory:

   ```bash
   cd docs
   ```

3. Build the documentation:

   ```bash
   make html
   ```

This will generate the HTML version of the documentation in the `_build/html/` directory, which you can view locally.

Contributing
------------

If you'd like to contribute to **PgEngine**, you're welcome to open issues, submit pull requests, or provide feedback!

To get started with development, clone the repository, and make sure to follow the **contributing guidelines** outlined in the `CONTRIBUTING.md <https://github.com/Gallasko/PgEngine/blob/main/CONTRIBUTING.md>`_.

Common Issues
-------------

1. **Missing Dependencies**  
   If you encounter issues during the build, make sure that all required dependencies (like SDL2, GLEW, Assimp, etc.) are installed. Use the package manager for your system to install them, or follow the installation instructions for each library.

2. **Build Failures**  
   If you run into build failures, try cleaning the build directory and running CMake again:
   
   ```bash
   rm -rf build
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```

3. **Runtime Errors**  
   If the engine fails to run, ensure that your graphics drivers and OpenGL version are up to date.

Contact and Support
-------------------

If you have questions or run into issues, feel free to open an issue on the `GitHub Issues page <https://github.com/Gallasko/PgEngine/issues>`_ or reach out to the maintainers for help.
```
