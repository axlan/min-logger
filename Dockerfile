FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /app

# Install build tools, CMake, Python, GDB, and dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    python3 \
    python3-pip \
    gdb \
    curl \
    pkg-config \
    ca-certificates \
  && rm -rf /var/lib/apt/lists/*

# Install uv (fast Python package manager)
RUN curl -LsSf https://astral.sh/uv/install.sh | sh
ENV PATH="/root/.local/bin:${PATH}"

# Copy project into container
COPY . /app

# Build and run integration tests
RUN ./scripts/integration_tests.sh

CMD ["/bin/bash"]
