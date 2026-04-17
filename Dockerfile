# SpliX Modernized Build Environment
FROM debian:bookworm-slim

LABEL maintainer="SpliX Modernization Team"
LABEL version="2026.1"

# Avoid interactive prompts during apt-get
ENV DEBIAN_FRONTEND=noninteractive

# Install core build dependencies
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
    && rm -rf /var/lib/apt/lists/*

# Add ARM64 architecture for cross-compilation
RUN dpkg --add-architecture arm64 && \
    apt-get update && apt-get install -y --no-install-recommends \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    libcups2-dev:arm64 \
    libcupsimage2-dev:arm64 \
    libjbig-dev:arm64 \
    && rm -rf /var/lib/apt/lists/*

# Set up ccache
ENV PATH="/usr/lib/ccache:${PATH}"
ENV CCACHE_DIR=/root/.ccache

# Create workspace
WORKDIR /workspace

# Default command
CMD ["bash"]
