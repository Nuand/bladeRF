#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libusb.h>

#if defined(WIN32) && !defined(__CYGWIN__)
#include <winbase.h>
typedef const struct libusb_version * (__stdcall * version_fn)(void);
#endif

int main(int argc, char *argv[])
{
    int status = 0;
    const struct libusb_version *ver;

#if defined(WIN32) && !defined(__CYGWIN__)
    if (argc > 1) {
        HINSTANCE dll;
        version_fn get_version;

        dll = LoadLibraryA(argv[1]);
        if (!dll) {
            fprintf(stderr, "Failed to load %s\n", argv[1]);
            return -1;
        }

        get_version = (version_fn) GetProcAddress(dll, "libusb_get_version");
        if (get_version) {
            ver = get_version();
            printf("%u.%u.%u\n", ver->major, ver->minor, ver->micro);
        } else{
            fprintf(stderr, "Failed to get address of libusb_get_version()\n");
            status = -1;
        }
        FreeLibrary(dll);
    } else {
        fprintf(stderr, "Windows usage: %s <libusb.dll>\n", argv[0]);
        return -1;
    }
#else
    ver = libusb_get_version();
    printf("%u.%u.%u\n", ver->major, ver->minor, ver->micro);
#endif

    return status;
}
