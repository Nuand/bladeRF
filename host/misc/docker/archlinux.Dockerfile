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

FROM archlinux/base

LABEL maintainer="Nuand LLC <bladeRF@nuand.com>"
LABEL version="0.0.1"
LABEL description="CI build environment for the bladeRF project (Arch Linux)"

# Install things
RUN pacman -Syuq --noconfirm \
    base-devel \
    cmake \
    doxygen \
    git \
    help2man \
    libtecla \
    libusb \
 && echo "/usr/local/lib" > /etc/ld.so.conf.d/locallib.conf \
 && pacman -Scc --noconfirm

WORKDIR /root

COPY . /root/bladeRF

RUN test -d "/root/bladeRF/host/build" && rm -rf /root/bladeRF/host/build

RUN cd bladeRF/host \
 && mkdir -p build \
 && cd build \
 && cmake -DBUILD_DOCUMENTATION=ON ../ \
 && make \
 && make install \
 && ldconfig
