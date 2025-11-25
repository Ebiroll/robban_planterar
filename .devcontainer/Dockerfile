# Development container with Emscripten
FROM debian:bookworm-slim

# Avoid warnings by switching to noninteractive
ENV DEBIAN_FRONTEND=noninteractive

# Install system dependencies
RUN apt-get update && apt-get install -y \
    git \
    cmake \
    build-essential \
    python3 \
    python3-pip \
    wget \
    xz-utils \
    # X11 libraries for Linux native builds
    libx11-dev \
    libxrandr-dev \
    libxcursor-dev \
    libxi-dev \
    libxinerama-dev \
    # OpenGL libraries
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    # Audio libraries
    libasound2-dev \
    # Other useful tools
    curl \
    vim \
    nano \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /workspace

# Install Emscripten SDK
ENV EMSDK_VERSION=3.1.50
RUN git clone https://github.com/emscripten-core/emsdk.git /emsdk && \
    cd /emsdk && \
    ./emsdk install ${EMSDK_VERSION} && \
    ./emsdk activate ${EMSDK_VERSION}

# Set up Emscripten environment variables
ENV EMSDK=/emsdk
ENV EM_CONFIG=/emsdk/.emscripten
ENV PATH="/emsdk:/emsdk/upstream/emscripten:${PATH}"

# Create a script to source emsdk environment
RUN echo '#!/bin/bash\nsource /emsdk/emsdk_env.sh\nexec "$@"' > /usr/local/bin/emsdk-env && \
    chmod +x /usr/local/bin/emsdk-env

# The source code will be mounted as a volume
# so we don't need to COPY it here

CMD ["/bin/bash"]
