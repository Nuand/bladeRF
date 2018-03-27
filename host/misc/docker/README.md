# Dockerfiles for various OS environments #

This directory contains Dockerfiles for building libbladeRF and associated
utilities under various operating systems, primarily as a means to ensure it
builds properly across a variety of OSes.

Currently, the following operating systems are supported:

 * Linux
  * Arch Linux
  * CentOS
    * 7
  * Debian
    * Jessie (8.x)
    * Stretch (9.x)
  * Fedora
  * Ubuntu
    * Trusty (14.04)
    * Xenial (16.04)
    * Artful (17.10)
    * Bionic (18.04)

## Building ##

Build a Docker image from the root of the Nuand/bladeRF checkout:

```
docker build -f host/misc/docker/archlinux.Dockerfile .
```

Optionally, add a -t option to tag the image:

```
docker build -f host/misc/docker/archlinux.Dockerfile \
             -t nuand/bladerf-buildenv:archlinux .
```

## Running ##

For a quick test to ensure the build was successful, try:

```
docker run --rm -t nuand/bladerf-buildenv:archlinux bladeRF-cli --version
```

At this time, actually using a bladeRF device is not supported, although it
would be really useful.

## Future goals ##

 * More distribution support
 * FreeBSD
 * clang builds
 * Hardware access
