#ifndef PACK_H
#define PACK_H

#include <stdio.h>
#include <stdint.h>

/* Read const uint8 pointer */
uint8_t unpack_u8 (const uint8_t **);
uint16_t unpack_u16 (const uint8_t **);
uint32_t unpack_u32 (const uint8_t **);
uint8_t *unpack_bytes(const uint8_t **, size_t, uint8_t *);
uint16_t unpack_string16 (uint8_t **buf, uint8_t **dest);
void pack_u8(uint8_t **, uint8_t);
void pack_u16(uint8_t **, uint16_t);
void pack_u32(uint8_t **, uint32_t);
void pack_bytes(uint8_t **, uint8_t *);

struct bytestring {
    size_t size;
    size_t last;
    unsigned char *data;
};

/*
 * const struct bytestring constructor, it require a size cause we use a bounded
 * bytestring, e.g. no resize over a defined size
 */
struct bytestring *bytestring_create(size_t);
void bytestring_init(struct bytestring *, size_t);
void bytestring_release(struct bytestring *);
void bytestring_reset(struct bytestring *);

#endif