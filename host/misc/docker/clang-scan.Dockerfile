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

FROM ubuntu:bionic

LABEL maintainer="Nuand LLC <bladeRF@nuand.com>"
LABEL version="0.0.2"
LABEL description="CI build environment for the bladeRF project"
LABEL com.nuand.ci.distribution.name="Ubuntu"
LABEL com.nuand.ci.distribution.codename="bionic"
LABEL com.nuand.ci.distribution.version="18.04"

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
 && apt-get clean

# Copy in our build context
COPY --from=nuand/bladerf-buildenv:base /root/bladeRF /root/bladeRF
COPY --from=nuand/bladerf-buildenv:base /root/.config /root/.config
WORKDIR /root/bladeRF

# Install clang-tools
RUN apt-get update && apt-get install -y clang-tools && apt-get clean

ARG parallel=1

RUN cd /root/bladeRF/host \
 && mkdir -p build \
 && cd build \
 && cmake -DBUILD_DOCUMENTATION=ON -DCMAKE_C_COMPILER=/usr/share/clang/scan-build-6.0/libexec/ccc-analyzer ../ \
 && /usr/share/clang/scan-build-6.0/bin/scan-build -analyze-headers -maxloop 100 -o ./report make -j${parallel}
