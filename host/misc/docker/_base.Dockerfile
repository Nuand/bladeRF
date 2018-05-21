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

# This image is a cleaned version of the current working directory.
# It is used to speed up the copy/clean process for later builds.
FROM ubuntu AS first

LABEL maintainer="Nuand LLC <bladeRF@nuand.com>"

RUN apt-get update \
 && apt-get install -y \
        git \
 && apt-get clean

COPY . /root/bladeRF
ENV work="/root/bladeRF/hdl/quartus/work"
ENV rbf="output_files/hosted.rbf"
ENV cfgdir="/root/.config/Nuand/bladeRF"

# copy FPGA images from FPGA work dir
# TODO: build these in docker, as well
RUN mkdir -p ${cfgdir}
RUN test -f "${work}/bladerf-40-hosted/${rbf}" \
 && cp "${work}/bladerf-40-hosted/${rbf}" ${cfgdir}/hostedx40.rbf \
 || exit 0
RUN test -f "${work}/bladerf-115-hosted/${rbf}" \
 && cp "${work}/bladerf-115-hosted/${rbf}" ${cfgdir}/hostedx115.rbf \
 || exit 0
RUN test -f "${work}/bladerf-micro-A2-hosted/${rbf}" \
 && cp "${work}/bladerf-micro-A2-hosted/${rbf}" ${cfgdir}/hostedxA2.rbf \
 || exit 0
RUN test -f "${work}/bladerf-micro-A4-hosted/${rbf}" \
 && cp "${work}/bladerf-micro-A4-hosted/${rbf}" ${cfgdir}/hostedxA4.rbf \
 || exit 0
RUN test -f "${work}/bladerf-micro-A9-hosted/${rbf}" \
 && cp "${work}/bladerf-micro-A9-hosted/${rbf}" ${cfgdir}/hostedxA9.rbf \
 || exit 0

# clean up unnecessary stuff
WORKDIR /root/bladeRF
RUN git clean -xdf && git gc --aggressive

# build a scratch image with nothing but the bladeRF source and FPGA images
FROM scratch
COPY --from=first /root /root
