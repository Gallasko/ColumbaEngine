# PgEngine Scripts Directory

This directory contains installation and testing scripts for PgEngine.

## Directory Structure

```
scripts/
├── install/                    # Installation scripts
│   ├── install-pgengine.sh     # Main installation script
│   ├── install-emscripten.sh   # Emscripten/WebAssembly support
│   ├── validate-installation.sh # Installation validator
│   └── INSTALLATION.md         # Detailed installation guide
├── testdockers/                # Docker testing framework
│   ├── run-docker-tests.sh     # Multi-distribution test runner
│   ├── quick-test.sh           # Fast single-container tests
│   ├── test-all.sh             # Master test orchestrator
│   ├── test-ubuntu.Dockerfile  # Ubuntu test environment
│   ├── test-fedora.Dockerfile  # Fedora test environment
│   ├── test-arch.Dockerfile    # Arch Linux test environment
│   └── docker-compose.test.yml # Multi-container testing
└── README.md                   # This file
```

## Quick Start

### Install PgEngine

```bash
# Run the installation script
./install/install-pgengine.sh

# Or with custom options
./install/install-pgengine.sh -v v1.0.0 -p ~/.local -j 4
```

### Test Installation Scripts

```bash
# Quick test in Docker
./testdockers/quick-test.sh

# Full multi-distribution testing
./testdockers/run-docker-tests.sh

# Comprehensive test suite
./testdockers/test-all.sh full
```

### Add Web Support

```bash
# Install Emscripten and build for web
./install/install-emscripten.sh
```

## For Users

If you just want to install PgEngine, you only need the files in `install/`:

- **`install-pgengine.sh`** - Main installation script
- **`validate-installation.sh`** - Verify your installation works
- **`install-emscripten.sh`** - Add WebAssembly support (optional)

## For Developers

If you're developing or testing the installation scripts, use `testdockers/`:

- **`quick-test.sh`** - Fast development testing
- **`run-docker-tests.sh`** - Multi-distribution validation
- **`test-all.sh`** - Complete test suite with reporting

## Installation Options

The installation scripts support:

- ✅ **Ubuntu, Fedora, Arch Linux** - Automatic dependency installation
- ✅ **Custom installation paths** - No sudo required with `--prefix ~/.local`
- ✅ **Version selection** - Install specific releases with `--version`
- ✅ **Parallel builds** - Speed up compilation with `--jobs N`
- ✅ **WebAssembly support** - Full Emscripten integration
- ✅ **Validation testing** - Ensure everything works correctly

## Testing Capabilities

The Docker testing framework provides:

- 🐳 **Fresh environments** - Clean container testing
- 🚀 **Multi-distribution** - Ubuntu, Fedora, Arch Linux
- ⚡ **Fast iteration** - Quick tests for development
- 📊 **Comprehensive reports** - Detailed test results
- 🔧 **CI/CD ready** - GitHub Actions integration

## Examples

### Basic Installation
```bash
cd scripts/install
./install-pgengine.sh
```

### Development Testing
```bash
cd scripts/testdockers
./quick-test.sh build
```

### Release Validation
```bash
cd scripts/testdockers
./test-all.sh full
```

### Custom Installation
```bash
cd scripts/install
./install-pgengine.sh --prefix ~/my-engine --version v2.0.0 --jobs 8
```

## Environment Variables

- `PGENGINE_REPO` - Repository URL to install from
- `INSTALL_PREFIX` - Installation directory (default: /usr/local)
- `BUILD_JOBS` - Number of parallel build jobs

## Getting Help

- Check `install/INSTALLATION.md` for detailed installation guide
- Run any script with `--help` for usage information
- Test your installation with `install/validate-installation.sh`

## Contributing

When modifying installation scripts:

1. Test with `testdockers/quick-test.sh` during development
2. Run `testdockers/test-all.sh` before submitting changes
3. Update documentation if adding new features
4. Ensure all test distributions pass