# Arch Linux test environment for PgEngine installation
FROM archlinux:latest

# Update system and install minimal dependencies
RUN pacman -Syu --noconfirm && pacman -S --noconfirm \
    curl \
    wget \
    sudo \
    ca-certificates \
    base-devel \
    && pacman -Scc --noconfirm

# Create a test user with sudo privileges
RUN useradd -m -s /bin/bash tester && \
    usermod -aG wheel tester && \
    echo 'tester ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# Switch to test user
USER tester
WORKDIR /home/tester

# Set environment variables for testing
ENV PGENGINE_REPO=https://github.com/Gallasko/PgEngine.git
ENV INSTALL_PREFIX=/usr/local
ENV BUILD_JOBS=2

# Default command
CMD ["/bin/bash"]