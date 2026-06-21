#ifndef SHA256_H
#define SHA256_H

#include <stdint.h>
#include <stddef.h>

void sha256_hex(const uint8_t *data, size_t len, char out_hex[65]);

#endif
