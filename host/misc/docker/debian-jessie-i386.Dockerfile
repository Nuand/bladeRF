# This file is part of the bladeRF project:
#   http://www.github.com/nuand/bladeRF
#
# Copyright (c) 2018 Nuand LLC.
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

FROM i386/debian:jessie

LABEL maintainer="Nuand LLC <bladeRF@nuand.com>"
LABEL version="0.0.2"
LABEL description="CI build environment for the bladeRF project"
LABEL com.nuand.ci.distribution.name="Debian"
LABEL com.nuand.ci.distribution.codename="jessie-i386"
LABEL com.nuand.ci.distribution.version="8"

# Install things
RUN apt-get update \
 && apt-get install -y \
        build-essential \
        clang \
        cmake \
        doxygen \
        git \
        help2man \
        libtecla-dev \
        libusb-1.0-0-dev \
        pandoc \
        pkg-config \
        usbutils \
 && apt-get clean

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
        -DTAGGED_RELEASE=${taggedrelease} \
    ../ \
 && make -j${parallel} \
 && make install \
 && ldconfig
