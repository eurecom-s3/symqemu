# prepare machine
FROM ubuntu:22.04 as builder

RUN apt update
RUN apt install -y \
    ninja-build \
    libglib2.0-dev \
    llvm \
    git \
    python3 \
    python3-pip

#
FROM builder as symqemu

COPY . /symqemu_source
WORKDIR /symqemu_source

# Meson gives an error if symcc is in a subdirectory of symqemu
RUN mv /symqemu_source/symcc /symcc

# The only symcc artifact needed by symqemu is libSymRuntime.so
# Instead of compiling symcc in this image, we rely on the existing symcc docker image and
# we just copy libSymRuntime.so at the location where symqemu expects it
COPY --from=symcc /symcc_build/SymRuntime-prefix/src/SymRuntime-build/libSymRuntime.so /symcc/build/SymRuntime-prefix/src/SymRuntime-build/libSymRuntime.so

RUN ./configure                                                       \
          --cc=gcc-13                                               \
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
          --symcc-build=/symcc/build                                  \
          --enable-debug-tcg                                          \
          --enable-werror

RUN make -j

RUN make check

WORKDIR /symqemu_source/tests/symqemu
RUN python3 -m unittest test.py
