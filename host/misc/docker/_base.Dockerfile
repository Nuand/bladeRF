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
FROM busybox AS first

LABEL maintainer="Nuand LLC <bladeRF@nuand.com>"

WORKDIR /root
COPY . /root/bladeRF
RUN test -d "/root/bladeRF/host/build" && rm -rf /root/bladeRF/host/build || exit 0
RUN test -d "/root/bladeRF/build" && rm -rf /root/bladeRF/build || exit 0
RUN test -d "/root/bladeRF/hdl" && rm -rf /root/bladeRF/hdl || exit 0

FROM scratch
COPY --from=first /root/bladeRF /root/bladeRF
