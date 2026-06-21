#include "rs485.h"

void rs485_init(rs485_t *h, GPIO_TypeDef *de_port, uint16_t de_pin)
{
  h->de_port = de_port;
  h->de_pin  = de_pin;

  GPIO_InitTypeDef gi = {0};
  gi.Pin   = de_pin;
  gi.Mode  = GPIO_MODE_OUTPUT_PP;
  gi.Pull  = GPIO_NOPULL;
  gi.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(de_port, &gi);

  rs485_rx(h);
}
