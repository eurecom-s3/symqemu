# prepare machine
FROM ubuntu:22.04 as builder

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
    libzstd-dev

RUN pip install --user meson

# This is passed along to symcc and qsym backend
arg LLVM_VERSION=15

# installing/building with the right LLVM version, currently:
# - no plan to support < 11
# - 12 to 15 are in official packages,
# - 16 and 17 provided by llvm.org
# - TODO 18 should be fixed
RUN if  [ $LLVM_VERSION -le 11 ];  then echo "LLVM <= 11 not supported" ; false ;fi
RUN if  [ $LLVM_VERSION -ge 18 ];  then echo "LLVM >= 18 currently not supported" ; false ;fi
RUN if  [ $LLVM_VERSION -eq 12 ] || [ $LLVM_VERSION -eq 13 ] || [ $LLVM_VERSION -eq 14 ] || [ $LLVM_VERSION -eq 15 ]; then 	\
    apt install -y llvm-${LLVM_VERSION} clang-${LLVM_VERSION} ; 								\
    else mkdir /llvm && cd  /llvm &&  wget https://apt.llvm.org/llvm.sh && chmod +x llvm.sh && ./llvm.sh ${LLVM_VERSION};   	\
    fi

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
          --enable-debug-tcg                                          \
          --symcc-rt-llvm-version="$LLVM_VERSION"

RUN cd build && make -j

RUN cd build && make check

WORKDIR /symqemu_source/tests/symqemu
RUN python3 -m unittest test.py
