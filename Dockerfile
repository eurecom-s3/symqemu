FROM ubuntu:22.04

RUN apt update
RUN apt install -y \
    ninja-build \
    libglib2.0-dev \
    llvm \
    python3 \
    python3-pip

COPY . /symqemu_source
WORKDIR /symqemu_source

# Meson gives an error if symcc is in a subdirectory of symqemu
RUN mv /symqemu_source/symcc /symcc

RUN ./configure                                                       \
          --audio-drv-list=                                           \
          --disable-sdl                                               \
          --disable-gtk                                               \
          --disable-vte                                               \
          --disable-opengl                                            \
          --disable-virglrenderer                                     \
          --disable-werror                                            \
          --target-list=x86_64-linux-user                             \
          --enable-debug                                              \
          --symcc-source=/symcc                                       \
          --symcc-build=/symcc/build

RUN make -j

WORKDIR /symqemu_source/tests/symqemu
RUN python3 -m unittest test.py
