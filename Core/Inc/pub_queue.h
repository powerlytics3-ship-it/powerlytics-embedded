#ifndef PUB_QUEUE_H
#define PUB_QUEUE_H

#include <stdint.h>

#define PUBQ_MAX 10

typedef struct {
    char topic[64];
    char payload[1024];
} pub_item_t;

void pubq_init(void);
void pubq_push(const char *topic, const char *payload);
uint8_t pubq_pop(pub_item_t *out);
uint8_t pubq_is_empty(void);

#endif /* PUB_QUEUE_H */
