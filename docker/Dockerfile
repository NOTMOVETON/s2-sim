FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libeigen3-dev \
    libyaml-cpp-dev \
    nlohmann-json3-dev \
    libgtest-dev \
    libgmock-dev \
    zlib1g-dev \
    libssl-dev \
    pkg-config \
    libgeographic-dev \
    geographiclib-tools \
    libtinyxml2-dev \
    && rm -rf /var/lib/apt/lists/*

# uWebSockets (для визуализатора, ставим сразу)
RUN git clone --recurse-submodules https://github.com/uNetworking/uWebSockets.git /tmp/uws \
    && cp -r /tmp/uws/src/* /usr/local/include/ \
    && cp -r /tmp/uws/uSockets/src/* /usr/local/include/ \
    && cd /tmp/uws/uSockets && make && cp uSockets.a /usr/local/lib/ \
    && rm -rf /tmp/uws

WORKDIR /workspace