#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libusb.h>

int main(int argc, char *argv[])
{
    const struct libusb_version *ver = libusb_get_version();
    printf("%u.%u.%u\n", ver->major, ver->minor, ver->micro);
    return 0;
}
