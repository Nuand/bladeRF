# This file is part of the bladeRF project:
#   http://www.github.com/nuand/bladeRF
#
# Copyright (c) 2025 Nuand LLC.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

FROM mageia:latest

LABEL maintainer="Nuand LLC <bladeRF@nuand.com>"
LABEL version="0.0.2"
LABEL description="CI build environment for the bladeRF project"
LABEL com.nuand.ci.distribution.name="Mageia Linux"

# Install development tools and required packages using urpmi (Mageia's package manager)
RUN urpmi.update -a \
 && urpmi --auto \
        task-c-devel \
        task-c++-devel \
        clang \
        cmake \
        doxygen \
        git \
        help2man \
        lib64curl-devel \
        lib64edit-devel \
        lib64ncurses-devel \
        lib64usb1.0-devel \
        usbutils \
        make \
        gcc \
        gcc-c++ \
 && urpme --auto-orphans

# Copy in our build context
COPY --from=nuand/bladerf-buildenv:base /root/bladeRF /root/bladeRF
COPY --from=nuand/bladerf-buildenv:base /root/.config /root/.config
WORKDIR /root/bladeRF

# Build arguments
ARG compiler=gcc
ARG buildtype=Release
ARG taggedrelease=NO
ARG parallel=1

# Do the build!
RUN cd /root/bladeRF/ \
 && mkdir -p build \
 && cd build \
 && cmake \
        -DBUILD_DOCUMENTATION=ON \
        -DCMAKE_C_COMPILER=${compiler} \
        -DCMAKE_BUILD_TYPE=${buildtype} \
        -DENABLE_FX3_BUILD=OFF \
        -DENABLE_HOST_BUILD=ON \
        -DTEST_REGRESSION=ON \
        -DTAGGED_RELEASE=${taggedrelease} \
        -DCURSES_INCLUDE_PATH=/usr/include \
        -DCURSES_CURSES_LIBRARY=/usr/lib64/libcurses.so \
        -DCURSES_NCURSES_LIBRARY=/usr/lib64/libncurses.so \
    ../ \
 && make -j${parallel} \
 && make install \
 && echo "/usr/local/lib64" > /etc/ld.so.conf.d/bladerf.conf \
 && ldconfig
