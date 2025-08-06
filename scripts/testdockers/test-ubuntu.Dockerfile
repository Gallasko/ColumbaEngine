# Ubuntu test environment for PgEngine installation
FROM ubuntu:22.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install minimal dependencies for testing (not PgEngine deps - let script install them)
RUN apt-get update && apt-get install -y \
    curl \
    wget \
    sudo \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create a test user with sudo privileges
RUN useradd -m -s /bin/bash tester && \
    usermod -aG sudo tester && \
    echo 'tester ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# Switch to test user
USER tester
WORKDIR /home/tester

# Set environment variables for testing
ENV PGENGINE_REPO=https://github.com/Gallasko/PgEngine.git
ENV INSTALL_PREFIX=/usr/local
ENV BUILD_JOBS=2

# Copy test scripts (will be mounted from host)
# COPY --chown=tester:tester install-pgengine.sh .
# COPY --chown=tester:tester install-emscripten.sh .
# COPY --chown=tester:tester validate-installation.sh .

# Default command
CMD ["/bin/bash"]