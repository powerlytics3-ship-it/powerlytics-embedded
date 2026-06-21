#include "pub_queue.h"
#include <string.h>
#include <stdio.h>
#include "main.h" // Needed for __disable_irq()

static pub_item_t g_q[PUBQ_MAX];
static volatile uint8_t g_w = 0;
static volatile uint8_t g_r = 0;

// Move this to the top to fix "conflicting types" warning
static uint8_t is_full(void) {
  return (uint8_t)((g_w + 1) % PUBQ_MAX) == g_r;
}

void pubq_init(void) {
  g_w = 0; g_r = 0;
  memset(g_q, 0, sizeof(g_q));
}

uint8_t pubq_is_empty(void) {
  return g_w == g_r;
}

void pubq_push(const char *topic, const char *payload) {
  if (!topic || !payload || is_full()) return;

  uint32_t primask = __get_PRIMASK();
  __disable_irq(); // Protect the pointers from task switching

  snprintf(g_q[g_w].topic, 64, "%s", topic);
  snprintf(g_q[g_w].payload, 1024, "%s", payload);
  g_w = (g_w + 1) % PUBQ_MAX;

  if (!primask) __enable_irq();
}

uint8_t pubq_pop(pub_item_t *out) {
  if (pubq_is_empty()) return 0;

  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  memcpy(out, &g_q[g_r], sizeof(pub_item_t));
  g_r = (g_r + 1) % PUBQ_MAX;

  if (!primask) __enable_irq();
  return 1;
}
