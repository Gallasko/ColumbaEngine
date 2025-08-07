# Fedora test environment for ColumbaEngine installation
FROM fedora:39

# Install minimal dependencies for testing
RUN dnf update -y && dnf install -y \
    curl \
    wget \
    sudo \
    ca-certificates \
    && dnf clean all

# Create a test user with sudo privileges
RUN useradd -m -s /bin/bash tester && \
    usermod -aG wheel tester && \
    echo 'tester ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# Switch to test user
USER tester
WORKDIR /home/tester

# Set environment variables for testing
ENV ColumbaEngine_REPO=https://github.com/Gallasko/ColumbaEngine.git
ENV INSTALL_PREFIX=/usr/local
ENV BUILD_JOBS=2

# Default command
CMD ["/bin/bash"]