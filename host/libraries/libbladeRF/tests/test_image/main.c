#include <libbladeRF.h>
#include <host_config.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>

char *file;

void fprint_hex(FILE *f, uint8_t *buf, size_t len)
{
    size_t i;

    for(i=0; i < len; i++) {
        fprintf(f, "%02x", buf[i]);
    }
}

void print_hex(uint8_t *buf, size_t len)
{
    fprint_hex(stdout, buf, len);
}

void write_image(size_t len)
{
    int rv;
    struct bladerf_image img;

    char *data = malloc(len);
    assert(data);

    memset(data, 0xee, len);

    rv = bladerf_image_fill(&img, BLADERF_IMAGE_TYPE_RAW, data, len);
    if(rv < 0) {
        fprintf(stderr, "bladerf_image_fill() failed: %s\n",
                bladerf_strerror(rv));
        goto out;
    }

    uint32_t address = HOST_TO_BE32(0x12345678);
    rv = bladerf_image_meta_add(&img.meta,
                                BLADERF_IMAGE_META_ADDRESS,
                                (void*) &address,
                                sizeof(address));
    if(rv < 0) {
        fprintf(stderr, "bladerf_image_meta_add() failed: %s\n",
                bladerf_strerror(rv));

        goto out;
    }


    rv = bladerf_image_write(&img, file);
    if(rv < 0) {
        fprintf(stderr, "bladerf_image_write() failed: %s\n",
                bladerf_strerror(rv));
        goto out;
    }

    printf("Yey!\n");

    return;
out:
    printf("Failed\n");
}

void read_image()
{
    int rv;
    struct bladerf_image img;

    rv = bladerf_image_read(&img, file);
    if(rv < 0) {
        fprintf(stderr, "bladerf_image_read() failed: %s\n",
                bladerf_strerror(rv));
        goto out;
    }

    uint32_t address;
    rv = bladerf_image_meta_get(&img.meta,
                                BLADERF_IMAGE_META_ADDRESS,
                                (void*) &address,
                                sizeof(address));
    if(rv < 0 || BE32_TO_HOST(address) != 0x12345678) {
        fprintf(stderr, "bladerf_image_meta_get() failed: %s\n",
                bladerf_strerror(rv));
        goto out;
    }

    print_hex((void*)img.sha256, 32);

    printf("\nYey!\n");

    return;
out:
    printf("Failed\n");
}

static void print_metadata(struct bladerf_image_meta *meta)
{
    uint32_t i;
    for(i=0; i < meta->n_entries; i++) {
        struct bladerf_image_meta_entry *ent = &meta->entries[i];


        uint32_t *address;
        struct bladerf_image_meta_version *ver;
        switch(ent->tag) {

        case BLADERF_IMAGE_META_ADDRESS:
            address = (uint32_t*)ent->data;
            fprintf(stderr, "BLADERF_IMAGE_META_ADDRESS: 0x%02x\n",
                    BE32_TO_HOST(*address));
            break;
        case BLADERF_IMAGE_META_VERSION:
            ver = (struct bladerf_image_meta_version*)ent->data;

            fprintf(stderr, "BLADERF_IMAGE_META_VERSION: %d.%d.%d\n",
                    ver->major, ver->minor, ver->patch);
            break;
        case BLADERF_IMAGE_META_SERIAL:
            fprintf(stderr, "BLADERF_IMAGE_META_SERIAL: ");
            fprint_hex(stderr, (uint8_t*)ent->data, 16);
            fprintf(stderr, "\n");
            break;
        case BLADERF_IMAGE_META_INVALID:
        default:
            break;
        }



    }


}

void dump_image()
{
    int rv;
    struct bladerf_image img;

    rv = bladerf_image_read(&img, file);
    if(rv < 0) {
        fprintf(stderr, "bladerf_image_read() failed: %s\n",
                bladerf_strerror(rv));
        goto out;
    }

    uint32_t address;
    rv = bladerf_image_meta_get(&img.meta,
                                BLADERF_IMAGE_META_ADDRESS,
                                (void*) &address,
                                sizeof(address));
    if(rv < 0) {
        fprintf(stderr, "bladerf_image_meta_get() failed: %s\n",
                bladerf_strerror(rv));
        goto out;
    }

    print_metadata(&img.meta);

    intptr_t written = 0;
    do {
        rv = write(STDOUT_FILENO, img.data + written, img.len - written);
        assert(rv > 0);
        written += rv;
    } while(img.len - written > 0);


    return;
out:
    return;
}


int main(int argc, char **argv)
{
    int rv = -1;

    if(argc != 3)
        goto out;

    file = argv[2];

    if(strcmp(argv[1], "read") == 0)
        read_image();
    else if(strcmp(argv[1], "write") == 0)
        write_image(BLADERF_FLASH_PAGE_SIZE);
    else if(strcmp(argv[1], "zero_write") == 0)
        write_image(0);
    else if(strcmp(argv[1], "dump") == 0)
        dump_image();


out:
    return rv;
}
