#include <stdlib.h>
#include <stdio.h>
#include <libbladeRF.h>

int main(void)
{
#ifdef LIBBLADERF_API_VERSION
    struct bladerf_version api_ver;
#endif

    struct bladerf_version lib_ver;

    /* Fetch the version from the library */
    bladerf_version(&lib_ver);

    printf("libbladeRF version values: %u.%u.%u\n",
           lib_ver.major, lib_ver.minor, lib_ver.patch);

    printf("libbladeRF version description: %s\n", lib_ver.describe);

    /* Fetch the version from the header */
#ifdef LIBBLADERF_API_VERSION
    api_ver.major = (LIBBLADERF_API_VERSION >> 24) & 0xff;
    api_ver.minor = (LIBBLADERF_API_VERSION >> 16) & 0xff;
    api_ver.patch = (LIBBLADERF_API_VERSION >> 8)  & 0xff;

    printf("libbladeRF version from header: %u.%u.%u(.%u)\n",
            api_ver.major, api_ver.minor, api_ver.patch,
            LIBBLADERF_API_VERSION & 0xff);

    if (lib_ver.major != api_ver.major) {
        fprintf(stderr, "Library and API header mismatch: major\n");
        return 1;
    }

    if (lib_ver.minor != api_ver.minor) {
        fprintf(stderr, "Library and API header mismatch: major\n");
        return 1;
    }

    if (lib_ver.patch != api_ver.patch) {
        fprintf(stderr, "Library and API header mismatch: major\n");
        return 1;
    }

    return 0;
#else
    printf("libbladeRF API header does not define LIBBLADERF_API_VERSION.\n");
    return 1;
#endif
}
