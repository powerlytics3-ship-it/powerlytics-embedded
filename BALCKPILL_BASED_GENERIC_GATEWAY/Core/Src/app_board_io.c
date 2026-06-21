#include "app_board_io.h"

/* Same PinDef as your main */
typedef struct {
  GPIO_TypeDef *port;
  uint16_t pin;
} PinDef;

/* Relay pins */
static const PinDef g_relays[] = {
  {GPIOD, GPIO_PIN_6},
  {GPIOD, GPIO_PIN_4},
  {GPIOD, GPIO_PIN_0},
  {GPIOC, GPIO_PIN_7},
  {GPIOC, GPIO_PIN_6},
};

/* Digital input pins */
static const PinDef g_dis[] = {
  {GPIOD, GPIO_PIN_1},
  {GPIOD, GPIO_PIN_3},
  {GPIOD, GPIO_PIN_5},
  {GPIOD, GPIO_PIN_7},
  {GPIOA, GPIO_PIN_7},
  {GPIOC, GPIO_PIN_5},
  {GPIOB, GPIO_PIN_1},
};

#define ADC_MAX_COUNTS 4095U
#define ADC_VREF_MV    3320U

static ADC_HandleTypeDef *s_hadc = NULL;

/* ---------- RELAY ---------- */
static void Relay_GPIO_Init_User(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();


  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4 | GPIO_PIN_0, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7 | GPIO_PIN_6, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = GPIO_PIN_4 | GPIO_PIN_0;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_6;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void app_relay_test(uint32_t step_ms)
{
  for (int i = 0; i < (int)(sizeof(g_relays)/sizeof(g_relays[0])); i++)
  {
    HAL_GPIO_WritePin(g_relays[i].port, g_relays[i].pin, GPIO_PIN_SET);
    HAL_Delay(step_ms);
    HAL_GPIO_WritePin(g_relays[i].port, g_relays[i].pin, GPIO_PIN_RESET);
    HAL_Delay(step_ms);
  }
}

/* ---------- DI ---------- */
static void DI_GPIO_Init_User(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitStruct.Pin  = GPIO_PIN_1 | GPIO_PIN_3| GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  GPIO_InitStruct.Pin  = GPIO_PIN_7;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin  = GPIO_PIN_5;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin  = GPIO_PIN_1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static uint8_t di_read(GPIO_TypeDef *port, uint16_t pin)
{
  return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET) ? 1 : 0;
}

void app_di_read_all(uint8_t out8[8])
{
  for (int i = 0; i < 8; i++)
    out8[i] = di_read(g_dis[i].port, g_dis[i].pin);
}

/* ---------- ADC ---------- */
static void ADC1_GPIO_Analog_Init_User(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;

  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin =
      GPIO_PIN_0 |
      GPIO_PIN_1 |
      GPIO_PIN_2 |
      GPIO_PIN_3 |
      GPIO_PIN_4 |
      GPIO_PIN_5;

  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_0;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static uint16_t adc1_read_avg(uint32_t channel, uint8_t samples)
{
    if (!s_hadc) return 0xFFFF;

    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel      = channel;
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;

    if (HAL_ADC_ConfigChannel(s_hadc, &sConfig) != HAL_OK) return 0xFFFF;

    // Dummy conversion (discard)
    HAL_ADC_Start(s_hadc);
    if (HAL_ADC_PollForConversion(s_hadc, 20) != HAL_OK) { HAL_ADC_Stop(s_hadc); return 0xFFFE; }
    (void)HAL_ADC_GetValue(s_hadc);
    HAL_ADC_Stop(s_hadc);

    uint32_t sum = 0;

    for (uint8_t i = 0; i < samples; i++)
    {
        HAL_ADC_Start(s_hadc);
        if (HAL_ADC_PollForConversion(s_hadc, 20) != HAL_OK) { HAL_ADC_Stop(s_hadc); return 0xFFFD; }
        sum += HAL_ADC_GetValue(s_hadc);
        HAL_ADC_Stop(s_hadc);
    }

    return (uint16_t)(sum / samples);
}



static uint32_t adc_to_mV(uint16_t raw)
{
  return ((uint32_t)raw * ADC_VREF_MV) / ADC_MAX_COUNTS;
}


void app_adc_read_all_mv(uint32_t out10_mv[10])
{
  uint16_t a0 = adc1_read_avg(ADC_CHANNEL_10,16); // PC0
  uint16_t a1 = adc1_read_avg(ADC_CHANNEL_11,16); // PC1
  uint16_t a2 = adc1_read_avg(ADC_CHANNEL_12,16); // PC2
  uint16_t a3 = adc1_read_avg(ADC_CHANNEL_13,16); // PC3

  uint16_t a4 = adc1_read_avg(ADC_CHANNEL_0,16);  // PA0
  uint16_t a5 = adc1_read_avg(ADC_CHANNEL_1,16);  // PA1
  uint16_t a6 = adc1_read_avg(ADC_CHANNEL_2,16);  // PA2
  uint16_t a7 = adc1_read_avg(ADC_CHANNEL_3,16);  // PA3
  uint16_t a8 = adc1_read_avg(ADC_CHANNEL_4,16);  // PA4
  uint16_t a9 = adc1_read_avg(ADC_CHANNEL_5,16);  // PA5

  out10_mv[0] = adc_to_mV(a0);
  out10_mv[1] = adc_to_mV(a1);
  out10_mv[2] = adc_to_mV(a2)*3.94;
  out10_mv[3] = adc_to_mV(a3)*3.94;
  out10_mv[4] = adc_to_mV(a4)*3.94;
  out10_mv[5] = adc_to_mV(a5)*3.94;
  out10_mv[6] = adc_to_mV(a6)*3.94;
  out10_mv[7] = adc_to_mV(a7)*3.94;
  out10_mv[8] = adc_to_mV(a8)*3.94;
  out10_mv[9] = adc_to_mV(a9)*3.94;
}

/* ---------- public init ---------- */
void app_board_io_init(ADC_HandleTypeDef *hadc)
{
  s_hadc = hadc;

  ADC1_GPIO_Analog_Init_User();
//Relay_GPIO_Init_User();
  DI_GPIO_Init_User();
}
