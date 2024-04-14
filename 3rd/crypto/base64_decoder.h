#ifndef __BASE64_DECODE_H__
#define __BASE64_DECODE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

char *
base64_decode(const uint8_t *text, size_t sz, int *out_size);

void base64_decode_free(char *result);

#ifdef __cplusplus
}
#endif

#endif
