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

FROM centos:7

LABEL maintainer="Nuand LLC <bladeRF@nuand.com>"
LABEL version="0.0.1"
LABEL description="CI build environment for the bladeRF project (CentOS 7)"

# Install things
RUN yum groupinstall -y "Development Tools" \
 && yum install -y \
    cmake \
    doxygen \
    gcc-c++ \
    help2man \
    libusbx \
    libusbx-devel \
    pandoc \
    wget \
 && yum clean all

# CentOS does not look in /usr/local/lib* for libraries, so we must add
# this manually.
RUN echo "/usr/local/lib" > /etc/ld.so.conf.d/locallib.conf \
 && echo "/usr/local/lib64" >> /etc/ld.so.conf.d/locallib.conf

WORKDIR /root

# CentOS lacks libtecla packages, so download and build.
RUN (curl http://www.astro.caltech.edu/~mcs/tecla/libtecla.tar.gz | tar xzf -) \
 && cd libtecla \
 && CC=gcc ./configure \
 && make \
 && make install \
 && cd .. \
 && rm -rf libtecla

COPY . /root/bladeRF

RUN test -d "/root/bladeRF/host/build" && rm -rf /root/bladeRF/host/build

RUN cd bladeRF/host \
 && mkdir -p build \
 && cd build \
 && cmake -DBUILD_DOCUMENTATION=ON ../ \
 && make \
 && make install \
 && ldconfig
