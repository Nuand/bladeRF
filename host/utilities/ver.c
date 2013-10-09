#include <stdio.h>
#include <libusb.h>
int main() {
	printf("%d\n", LIBUSBX_API_VERSION >= 0x01000102);
	return 0;
}

