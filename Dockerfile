# SpliX Modernized Build Environment
FROM ubuntu:26.04

LABEL maintainer="SpliX Modernization Team"
LABEL version="2026.2"

# Avoid interactive prompts during apt-get
ENV DEBIAN_FRONTEND=noninteractive

# Install core build dependencies
# Ubuntu 26.04 ships with modern GCC natively
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    ccache \
    cups \
    libcups2-dev \
    libcupsimage2-dev \
    libjbig-dev \
    ca-certificates \
    git \
    xxd \
    && rm -rf /var/lib/apt/lists/*

# Set up ccache
ENV PATH="/usr/lib/ccache:${PATH}"
ENV CCACHE_DIR=/root/.ccache

# Create workspace
WORKDIR /workspace

# Default command
CMD ["bash"]
