#ifndef PARTITION_H
#define PARTITION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t start_lba;
    uint64_t sector_count;
    uint8_t  type; // MBR type or custom mapping for GPT
    int      is_gpt;
} partition_info_t;

int partition_scan(int port, partition_info_t *out_parts, int max_parts);

#ifdef __cplusplus
}
#endif

#endif