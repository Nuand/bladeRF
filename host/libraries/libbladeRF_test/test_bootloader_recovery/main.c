#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libbladeRF.h>
#include "rel_assert.h"
#include "conversions.h"

/* Returns list size or negative error */
static int get_and_show_bootloader_list(struct bladerf_devinfo **list_out)
{
    int list_size, i;
    struct bladerf_devinfo *list;

    *list_out = NULL;

    list_size = bladerf_get_bootloader_list(&list);
    if (list_size < 0) {
        return list_size;
    } else if (list_size == 0) {
        /* It should be returning BLADERF_ERR_NODEV in this case... */
        fprintf(stderr, "bladerf_get_bootloader_list() should not return 0!\n");
        return BLADERF_ERR_UNEXPECTED;
    }

    putchar('\n');
    printf(" Bootloader devices\n");
    printf("---------------------------------------------------------------\n");

    for (i = 0; i < list_size; i++) {
        printf(" Option %d:\n", i);
        printf("   Backend:     %s\n", backend_description(list[i].backend));
        printf("   Serial:      %s\n", list[i].serial);
        printf("   USB Bus:     %u\n", list[i].usb_bus);
        printf("   USB Address: %u\n", list[i].usb_addr);
        printf("   Instance:    %u\n", list[i].instance);
        putchar('\n');
    }

    *list_out = list;
    return list_size;
}

static int interactive(const char *filename)
{
    int status;
    int list_size;
    struct bladerf_devinfo *list;
    bool valid_input;
    int selection;
    char input[32];

    list_size = get_and_show_bootloader_list(&list);
    if (list_size < 0) {
        return list_size;
    }

    memset(input, 0, sizeof(input));
    printf("Enter the option # associated with the device to recover> ");
    if (fgets(input, sizeof(input), stdin) == NULL) {
        status = BLADERF_ERR_UNEXPECTED;
        goto out;
    }

    if (input[strlen(input) - 1] == '\n') {
        input[strlen(input) - 1] = '\0';
    } else {
        putchar('\n');
    }

    selection = str2int(input, 0, list_size - 1, &valid_input);
    if (!valid_input) {
        status = BLADERF_ERR_INVAL;
    } else {
        assert(selection >= 0 && selection < list_size);
        status = bladerf_load_fw_from_bootloader(NULL,
                                                 BLADERF_BACKEND_ANY,
                                                 list[selection].usb_bus,
                                                 list[selection].usb_addr,
                                                 filename);
    }

out:
    bladerf_free_device_list(list);
    return status;
}

int main(int argc, char *argv[])
{
    int status = 0;

    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  List detected bootloader devices:\n");
        fprintf(stderr, "   %s list\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "  Command line recovery:\n");
        fprintf(stderr, "   %s <bus> <addr> <firmware file>\n", argv[0]);
        fprintf(stderr, "   %s <device identifier> <firmware file>\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "  Interactive recovery:\n");
        fprintf(stderr, "   %s <firmware file>\n", argv[0]);
        fprintf(stderr, "\n");
        return 1;
    }

    bladerf_log_set_verbosity(BLADERF_LOG_LEVEL_VERBOSE);

    if (argc == 2) {
        if (!strcasecmp("list", argv[1])) {
            struct bladerf_devinfo *list;
            status = get_and_show_bootloader_list(&list);
            if (status > 0) {
                bladerf_free_device_list(list);
                status = 0;
            }
        } else {
            status = interactive(argv[1]);
        }
    } else if (argc == 3) {
        status = bladerf_load_fw_from_bootloader(argv[1],
                                                 0, 0, 0, /* "Don't care" */
                                                 argv[2]);
    } else if (argc == 4) {
        uint8_t bus, addr;
        bool valid_input;

        bus = (uint8_t) str2int(argv[1], 0, UINT8_MAX - 1, &valid_input);
        if (!valid_input) {
            status = BLADERF_ERR_INVAL;
        }

        if (status == 0) {
            addr = (uint8_t) str2int(argv[2], 0, UINT8_MAX - 1, &valid_input);
            if (!valid_input) {
                status = BLADERF_ERR_INVAL;
            }
        }

        if (status == 0) {
            status = bladerf_load_fw_from_bootloader(NULL,
                                                     BLADERF_BACKEND_ANY,
                                                     bus,
                                                     addr,
                                                     argv[3]);
        }
    }

    if (status != 0) {
        fprintf(stderr, "%s\n", bladerf_strerror(status));
    }

    return status;
}
