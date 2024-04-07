# prepare machine
FROM ubuntu:22.04 as builder

# This is passed along to symcc and qsym backend
arg LLVM_VERSION=15

RUN apt update && apt install -y \
    ninja-build \
    libglib2.0-dev \
    llvm \
    git \
    python3 \
    python3-pip \
    cmake \
    wget \
    lsb-release \
    software-properties-common \
    gnupg \
    z3 \
    libz3-dev \
    libz3-dev \
    libzstd-dev \
    llvm-${LLVM_VERSION} \
    clang-${LLVM_VERSION}

RUN pip install --user meson

COPY . /symqemu_source
WORKDIR /symqemu_source

RUN mkdir build && cd build && ../configure                           \
          --audio-drv-list=                                           \
          --disable-sdl                                               \
          --disable-gtk                                               \
          --disable-vte                                               \
          --disable-opengl                                            \
          --disable-virglrenderer                                     \
          --target-list=x86_64-linux-user,riscv64-linux-user          \
          --enable-debug                                              \
          --enable-debug-tcg

RUN cd build && make -j

RUN cd build && make check

WORKDIR /symqemu_source/tests/symqemu
RUN python3 -m unittest test.py
