/* image_header.c
 * Licensed under GPLv2+ or LGPLv2+
 * Copyright (C) 2013  Daniel Gröber <dxld AT darkboxed DOT org>
 */

#include <sha256.h>
#include <host_config.h>
#include <libbladeRF.h>
#include <log.h>
#include <minmax.h>
#include <file_ops.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <rel_assert.h>
#include <limits.h>

const char image_signature[] = "bladeRF Image v1";

static void
sha256_buffer(char *buf, size_t len, char digest[SHA256_DIGEST_SIZE])
{
    SHA256_CTX ctx;

    SHA256_Init(&ctx);
    SHA256_Update(&ctx, buf, len);
    SHA256_Final((uint8_t*)digest, &ctx);
}

const char *bladerf_image_strmeta(bladerf_image_meta_tag tag)
{
    switch(tag) {
    case BLADERF_IMAGE_META_ADDRESS:
        return "BLADERF_IMAGE_META_ADDRESS";
    case BLADERF_IMAGE_META_VERSION:
        return "BLADERF_IMAGE_META_VERSION";
    case BLADERF_IMAGE_META_INVALID:
    default:
        return "BLADERF_IMAGE_META_INVALID";
    }
}

int bladerf_image_fill(struct bladerf_image *img,
                       bladerf_image_type_t type,
                       char *data,
                       size_t len)
{
    assert(len <= BLADERF_FLASH_TOTAL_SIZE);

    memcpy(img->signature, image_signature, BLADERF_SIGNATURE_SIZE);
    img->type = type;
    memset(&img->meta, 0x00, sizeof(img->meta));
    img->len = len;
    memcpy(img->data, data, len);
    sha256_buffer(data, len, img->sha256);

    return 0;
}

int bladerf_image_check_signature(char *sig)
{
    /* if this fails we got the wrong size in libbladerf.h! */
    assert(BLADERF_SIGNATURE_SIZE == sizeof(image_signature));

    return memcmp(image_signature, sig, BLADERF_SIGNATURE_SIZE);
}

static int verify_checksum(FILE *f)
{
    int rv;
    ssize_t len;
    char *buf;
    char sha256[SHA256_DIGEST_SIZE];
    char sha256_image[SHA256_DIGEST_SIZE];

    len = file_size(f);
    if(len < 0) {
        assert(len > INT_MIN);
        return (int)len;
    }

    if(len <= SHA256_DIGEST_SIZE)
        return -BLADERF_ERR_UNEXPECTED;

    buf = malloc(len);
    if(!buf)
        return BLADERF_ERR_MEM;

    rv = file_read(f, buf, len - SHA256_DIGEST_SIZE);
    if(rv < 0)
        goto error;

    sha256_buffer(buf, len - SHA256_DIGEST_SIZE, sha256);

    rv = file_read(f, sha256_image, SHA256_DIGEST_SIZE);
    if(rv < 0)
        goto error;

    rv = (memcmp(sha256, sha256_image, SHA256_DIGEST_SIZE) != 0);

    if(fseek(f, 0, SEEK_SET))
        goto error;

error:
    free(buf);
    return rv;
}

static int write_checksum(FILE *f)
{
    int rv;
    ssize_t len;
    char *buf;
    char sha256[SHA256_DIGEST_SIZE];

    len = file_size(f);
    if(len < 0) {
        assert(len > INT_MIN);
        return (int)len;
    }

    buf = malloc(len);
    if(!buf)
        return BLADERF_ERR_MEM;

    if(fseek(f, 0, SEEK_SET))
        goto error;

    rv = file_read(f, buf, len);
    if(rv < 0)
        goto error;

    sha256_buffer(buf, len, sha256);

    rv = file_write(f, sha256, sizeof(sha256));
    if(rv < 0)
        goto error;

error:
    free(buf);
    return rv;
}

int bladerf_image_probe_file(char *file)
{
    int rv;
    FILE *f;
    char sig[BLADERF_SIGNATURE_SIZE];
    ssize_t len;

    f = fopen(file, "rb");
    if(!f)
        return BLADERF_ERR_IO;

    rv = file_read(f, sig, sizeof(sig));
    if(rv < 0)
        goto out;

    rv = bladerf_image_check_signature(sig);
    if(rv < 0)
        goto out;
    else if(rv) {
        rv = BLADERF_ERR_INVAL;
        goto out;
    }

    rv = BLADERF_ERR_IO;
    if(fseek(f, 0, SEEK_SET))
        goto out;

    len = file_size(f);
    if(len < 0) {
        assert(len > INT_MAX);
        rv = len;
        goto out;
    }

    if(len < (ssize_t)BLADERF_IMAGE_MIN_SIZE) {
        rv = BLADERF_ERR_INVAL;
        goto out;
    }

    rv = verify_checksum(f);
    if(rv < 0)
        goto out;
    else if(rv) {
        rv = BLADERF_ERR_CHECKSUM;
        goto out;
    }

out:
    fclose(f);

    return rv;
}

int bladerf_image_meta_add(struct bladerf_image_meta *meta,
                           bladerf_image_meta_tag tag,
                           void *data,
                           uint32_t len)
{
    uint32_t i = meta->n_entries;

    struct bladerf_image_meta_entry *ent = &meta->entries[i];

    assert(tag >= 0 && tag < BLADERF_IMAGE_META_LAST);
    assert(len <= sizeof(ent->data));

    ent->tag = tag;
    ent->len = len;
    memcpy(ent->data, data, len);

    meta->n_entries++;

    return 0;
}

int bladerf_image_meta_get(struct bladerf_image_meta *meta,
                           int32_t tag,
                           void *data,
                           uint32_t len)
{
    struct bladerf_image_meta_entry *ent;
    uint32_t n_entries = meta->n_entries;

    uint32_t i;
    for(i=0; i < n_entries; i++) {
        ent = &meta->entries[i];
        if(ent->tag == tag) {
            assert(len <= sizeof(ent->data));
            assert(ent->len == len); /* format probably changed */
            memcpy(data, ent->data, len);

            return 0;
        }
    }

    return -1;
}

int bladerf_image_meta_write(struct bladerf_image_meta *meta, FILE *f)
{
    int rv;
    uint32_t n_entries_be;
    uint32_t i, count;

    n_entries_be = HOST_TO_BE32(meta->n_entries);
    rv = file_write(f, (char*) &n_entries_be, sizeof(n_entries_be));
    if(rv < 0)
        goto out;

    count = min_sz(meta->n_entries, BLADERF_IMAGE_META_NENT);
    for(i=0; i < count; i++) {
        int32_t tag_be;
        uint32_t len_be;

        struct bladerf_image_meta_entry *ent = &meta->entries[i];

        tag_be = HOST_TO_BE32(ent->tag);
        rv = file_write(f, (char*) &tag_be, sizeof(tag_be));
        if(rv < 0)
            goto out;

        len_be = HOST_TO_BE32(ent->len);
        rv = file_write(f, (char*) &len_be, sizeof(tag_be));
        if(rv < 0)
            goto out;

        rv = file_write(f, ent->data, sizeof(ent->data));
        if(rv < 0)
            goto out;
    }

out:
    return rv;
}

int bladerf_image_meta_read(struct bladerf_image_meta *meta, FILE *f)
{
    int rv;
    uint32_t n_entries_be;
    uint32_t i;

    rv = file_read(f, (char*) &n_entries_be, sizeof(n_entries_be));
    if(rv < 0)
        goto out;

    meta->n_entries = BE32_TO_HOST(n_entries_be);

    for(i=0; i < meta->n_entries; i++) {
        struct bladerf_image_meta_entry *ent = &meta->entries[i];
        int32_t tag_be;
        uint32_t len_be;

        rv = file_read(f, (char*) &tag_be, sizeof(tag_be));
        if(rv < 0)
            goto out;
        ent->tag = BE32_TO_HOST(tag_be);

        rv = file_read(f, (char*) &len_be, sizeof(len_be));
        if(rv < 0)
            goto out;
        ent->len = BE32_TO_HOST(len_be);

        rv = file_read(f, ent->data, sizeof(ent->data));
        if(rv < 0)
            goto out;
    }

out:
    return rv;
}

int bladerf_image_write(struct bladerf_image *img, char* file)
{
    int rv = -1;
    FILE *f;
    int32_t type;
    uint32_t len;

    f = fopen(file, "w+b");
    if(!f)
        return BLADERF_ERR_IO;

    assert(memcmp(img->signature, image_signature, BLADERF_SIGNATURE_SIZE) == 0);

    rv = file_write(f, img->signature, strlen(img->signature));
    if(rv < 0)
        goto out;

    type = HOST_TO_BE32(img->type);
    rv = file_write(f, (char*) &type, sizeof(type));
    if(rv < 0)
        goto out;

    rv = bladerf_image_meta_write(&img->meta, f);
    if(rv < 0)
        goto out;

    assert(img->len <= BLADERF_FLASH_TOTAL_SIZE);

    len = HOST_TO_BE32(img->len);
    rv = file_write(f, (char*) &len, sizeof(len));
    if(rv < 0)
        goto out;

    rv = file_write(f, img->data, img->len);
    if(rv < 0)
        goto out;

    rv = write_checksum(f);
    if(rv < 0)
        goto out;

    rv = 0;
out:
    fclose(f);

    return rv;
}

int bladerf_image_read(struct bladerf_image *img, char* file)
{
    int rv = -1;
    FILE *f;
    int32_t type_be;
    uint32_t len_be;

    f = fopen(file, "rb");
    if(!f)
        return BLADERF_ERR_IO;

    rv = verify_checksum(f);
    if(rv < 0) {
        goto out;
    } else if(rv) {
        rv = BLADERF_ERR_CHECKSUM;
        goto out;
    }

    rv = file_read(f, img->signature, BLADERF_SIGNATURE_SIZE);
    if(rv < 0)
        goto out;

    if(memcmp(img->signature, image_signature, BLADERF_SIGNATURE_SIZE) != 0) {
        goto out;
    }

    rv = file_read(f, (char*) &type_be, sizeof(type_be));
    if(rv < 0)
        goto out;

    img->type = BE32_TO_HOST(type_be);

    rv = bladerf_image_meta_read(&img->meta, f);
    if(rv < 0)
        goto out;

    rv = file_read(f, (char*) &len_be, sizeof(len_be));
    if(rv < 0)
        goto out;

    img->len = BE32_TO_HOST(len_be);

    assert(img->len <= BLADERF_FLASH_TOTAL_SIZE);

    rv = file_read(f, img->data, img->len);
    if(rv < 0)
        goto out;

    rv = file_read(f, img->sha256, sizeof(img->sha256));
    if(rv < 0)
        goto out;

    rv = 0;
out:
    fclose(f);

    return rv;
}
