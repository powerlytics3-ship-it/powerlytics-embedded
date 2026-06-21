/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32F407 + FreeRTOS (CMSIS-RTOS2)
  *                   - Debug UART1 on PB6/PB7 (USB-TTL)
  *                   - GSM/MQTT on UART2 (DMA + IDLE using uart2_rx_engine)
  *                   - Modbus RTU Master on UART3 + DMA + IDLE (MAX485, DE=PD11)
  *
  * IMPORTANT:
  * - UART2 RX MUST be ONLY via uart2_rx_engine (DMA-to-IDLE). No polling RX on UART2.
  * - USART2_IRQn must be enabled and USART2_IRQHandler must exist in stm32f4xx_it.c
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdio.h>

#include "debug_uart.h"

/* MODBUS */
#include "rs485.h"
#include "modbus_rtu_master.h"
#include "modbus_config_json.h"

/* RTC */
#include "rtc_manager.h"

/* APP */
#include "app_board_io.h"
#include "app_modbus_payload.h"
#include "w25q16.h"

/* GSM / MQTT */
#include "config.h"
#include "mqtt_handler.h"
#include "modem_handler.h"
#include "mqtt_config.h"
#include "uart2_rx_engine.h"
#include "pub_queue.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
RTC_TimeTypeDef sTime;
RTC_DateTypeDef sDate;

char topic_d2c_telemetry[64];
char topic_c2d_commands[64];
char topic_d2c_message[64];
char topic_d2c_logs[64];

/* IMPORTANT: must match mqtt_handler.c -> extern uint8_t buffer[...] */
uint8_t buffer[2048];

/* UART2 DMA-to-IDLE RX engine */
uart2_rx_t g_u2rx;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DBG_BAUD       115200U
#define MB_BAUD        4800U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;


UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;
UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_usart3_rx;
DMA_HandleTypeDef hdma_usart6_rx;
DMA_HandleTypeDef hdma_usart6_tx;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 1024 * 8,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for ModbusTask */
osThreadId_t ModbusTaskHandle;
const osThreadAttr_t ModbusTask_attributes = {
  .name = "ModbusTask",
  .stack_size = 512 * 8,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */
static uint8_t mb_rx_buf[256];
/* GSM / Modem UART abstraction */
#define MODEM_UART_HANDLE (&huart6)
#define MODEM_UART_INSTANCE USART6
osSemaphoreId_t mbRxSemHandle;
osSemaphoreId_t u2RxSemHandle;

static rs485_t g_rs485;
static mbm_t   g_mbm;

static W25Q_Handle s_flash;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_RTC_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART6_UART_Init(void);
void StartDefaultTask(void *argument);
void StartTask02(void *argument);

/* USER CODE BEGIN PFP */
static void print_reset_reason(void);
static void flash_test(void);
osMutexId_t g_modemMutex;
const osMutexAttr_t g_modemMutexAttr = { .name = "modemMutex" };

osEventFlagsId_t g_sysEvt;
#define EVT_MQTT_READY (1u << 0)

volatile uint32_t g_uplink_pause_until = 0;

static inline void uplink_pause_ms(uint32_t ms)
{
  g_uplink_pause_until = HAL_GetTick() + ms;
}

static inline uint8_t uplink_is_paused(void)
{
  return ((int32_t)(HAL_GetTick() - g_uplink_pause_until) < 0);
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void flash_test(void)
{
    s_flash.hspi = &hspi1;
    s_flash.cs_port = GPIOB;
    s_flash.cs_pin  = GPIO_PIN_0;

    uint32_t id = w25q_read_jedec_id(&s_flash);
    print("JEDEC raw = 0x%06lX\r\n", id);

    if (!w25q_init(&s_flash)) {
        print("W25Q init FAIL\r\n");
        return;
    }

    id = w25q_read_jedec_id(&s_flash);
    print("W25Q JEDEC: %02lX %02lX %02lX (0x%06lX)\r\n",
          (id >> 16) & 0xFF, (id >> 8) & 0xFF, id & 0xFF, id);
}

static void print_reset_reason(void)
{
    uint32_t csr = RCC->CSR;

    print("[RST] CSR=0x%08lX\r\n", (unsigned long)csr);
    if (csr & RCC_CSR_PORRSTF)  print("[RST] POR/PDR reset\r\n");
    if (csr & RCC_CSR_BORRSTF)  print("[RST] BOR reset\r\n");
    if (csr & RCC_CSR_PINRSTF)  print("[RST] PIN reset (NRST)\r\n");
    if (csr & RCC_CSR_SFTRSTF)  print("[RST] Software reset\r\n");
    if (csr & RCC_CSR_IWDGRSTF) print("[RST] IWDG reset\r\n");
    if (csr & RCC_CSR_WWDGRSTF) print("[RST] WWDG reset\r\n");
    if (csr & RCC_CSR_LPWRRSTF) print("[RST] Low power reset\r\n");

    __HAL_RCC_CLEAR_RESET_FLAGS();
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART3_UART_Init();
  MX_ADC1_Init();
  MX_USART1_UART_Init();
  MX_RTC_Init();
  MX_SPI1_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */
  debug_uart_init(&huart1);
  HAL_Delay(100);
  print_reset_reason();
  print("\r\n=== Device boot ===\r\n");

  /* Quick UART2 TX sanity */
//  const char *u2 = "USART2 TEST\r\n";
//  HAL_UART_Transmit(&huart2, (uint8_t*)u2, strlen(u2), 1000);
  const char *u2 = "MODEM UART TEST\r\n";
  HAL_UART_Transmit(MODEM_UART_HANDLE,
                    (uint8_t*)u2,
                    strlen(u2),
                    1000);
  HAL_Delay(100);

  /* RTC */
  if (rtc_clock_configure_with_fallback() == HAL_OK) {
    print("[RTC] Clock source set OK\r\n");
  } else {
    print("[RTC][ERR] Clock source config failed\r\n");
  }
  rtc_init_with_bkp_once();

  /* Board IO init */



//  while(1)
//  {
//      HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_3);
//      HAL_Delay(1000);
//      print("Hi\r\n");
//  }

  app_board_io_init(&hadc1);

  /* Flash */
  flash_test();

  /* Load config from flash */
  modbus_cfg_t cfg_from_flash;
  bool cfg_ok = modbus_cfg_load(&cfg_from_flash);

    print("Load from FLASH: %s\r\n", cfg_ok ? "VALID" : "DEFAULTS");
    print("CFG device_id=%s config_id=%s imei=%s slaves=%u\r\n",
           cfg_from_flash.device_id,
           cfg_from_flash.config_id,
           cfg_from_flash.imei,
           cfg_from_flash.slave_count);

    for (uint8_t si = 0; si < cfg_from_flash.slave_count; si++) {
      const mb_slave_cfg_t *sc = &cfg_from_flash.slaves[si];

      print("SLAVE[%u] uuid=%s sid=%u baud=%lu db=%u sb=%u par=%u interval=%lu timeout=%lu retries=%u reads=%u\r\n",
             si,
             sc->unique_slave_id,
             sc->slave_id,
             (unsigned long)sc->baud,
             sc->data_bits,
             sc->stop_bits,
             sc->parity,
             (unsigned long)sc->interval_ms,
             (unsigned long)sc->timeout_ms,
             sc->retries,
             sc->read_count);

      for (uint8_t ri = 0; ri < sc->read_count; ri++) {
        const mb_read_item_t *rc = &sc->reads[ri];
        print("  READ[%u] readId=%s fn=%u start=%u qty=%u\r\n",
               ri,
               rc->read_id,
               rc->func,
               rc->start_reg,
               rc->qty);
      }
    }

  HAL_Delay(5000);
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* ================= UART6 GSM RX ENGINE ================= */

  /* UART RX semaphore + engine init */
  u2RxSemHandle = osSemaphoreNew(1, 0, NULL);

  u2rx_init(&g_u2rx,
             MODEM_UART_HANDLE,
             u2RxSemHandle);

  /* Start UART DMA-to-IDLE RX */
  if (u2rx_start(&g_u2rx) != HAL_OK)
  {
      print("[UART6][ERR] DMA-to-IDLE start failed\r\n");
  }
  else
  {
      print("[UART6] DMA-to-IDLE started\r\n");
  }

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of ModbusTask */
  ModbusTaskHandle = osThreadNew(StartTask02, NULL, &ModbusTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();
  print("Scheduler returned!\r\n");

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x1;
  sTime.Minutes = 0x35;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_THURSDAY;
  sDate.Month = RTC_MONTH_DECEMBER;
  sDate.Date = 0x25;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the reference Clock input
  */
  if (HAL_RTCEx_SetRefClock(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  /* DMA2_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
  /* DMA2_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream6_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream6_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
//static void MX_GPIO_Init(void)
//{
//  GPIO_InitTypeDef GPIO_InitStruct = {0};
//  /* USER CODE BEGIN MX_GPIO_Init_1 */
//
//  /* USER CODE END MX_GPIO_Init_1 */
//
//  /* GPIO Ports Clock Enable */
//  __HAL_RCC_GPIOC_CLK_ENABLE();
//  __HAL_RCC_GPIOA_CLK_ENABLE();
//  __HAL_RCC_GPIOB_CLK_ENABLE();
//  __HAL_RCC_GPIOD_CLK_ENABLE();
//
//  /*Configure GPIO pin Output Level */
//  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
//
//  /*Configure GPIO pin Output Level */
//  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);
//
//  /*Configure GPIO pin : PB0 */
//  GPIO_InitStruct.Pin = GPIO_PIN_0;
//  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//  GPIO_InitStruct.Pull = GPIO_NOPULL;
//  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
//  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
//
//  /*Configure GPIO pin : PD2 */
//  GPIO_InitStruct.Pin = GPIO_PIN_2;
//  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//  GPIO_InitStruct.Pull = GPIO_NOPULL;
//  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
//  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
//
//  /*Configure GPIO pins : UART2_TX_Pin UART2_RX_Pin */
//  GPIO_InitStruct.Pin = UART2_TX_Pin|UART2_RX_Pin;
//  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//  GPIO_InitStruct.Pull = GPIO_NOPULL;
//  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
//  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
//  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
//
//  /* USER CODE BEGIN MX_GPIO_Init_2 */
//
//  /* USER CODE END MX_GPIO_Init_2 */
//
//}
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_2|GPIO_PIN_3, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_2|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */
/* Put this helper somewhere accessible (main.c USER CODE BEGIN 0 or a small debug file) */
static void dbg_dump_dma_chunk(const uint8_t *data, uint16_t len)
{
    print("\r\n[UART2 DMA CHUNK] len=%u\r\n", len);
    print("ASCII: ");
    for (uint16_t i = 0; i < len; i++) {
        uint8_t c = data[i];
        if (c >= 32 && c <= 126) print("%c", c);
        else if (c == '\r') print("\\r");
        else if (c == '\n') print("\\n");
        else print(".");
    }
    print("\r\n");
}


/* USER CODE BEGIN 4 */
//void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
//{
//    if (huart->Instance == USART3) {
//        mbm_on_rx_event(&g_mbm, huart, Size);
//        return;
//    }
//
//    if (huart->Instance == MODEM_UART_INSTANCE)
//    {
////           dbg_dump_dma_chunk(g_u2rx.dma_buf, Size);
//            u2rx_on_idle_event(&g_u2rx, Size);
//
//            /* 1) CLEAR ERROR FLAGS AND RESET STATE */
//            __HAL_UART_CLEAR_OREFLAG(huart);
//            huart->RxState = HAL_UART_STATE_READY;
//            huart->ErrorCode = HAL_UART_ERROR_NONE;
//
//            /* 2) Clear buffer to stop the "appending" visual bug */
//            memset(g_u2rx.dma_buf, 0, U2_DMA_RX_SZ);
//
//            /* 3) Restart DMA */
////            if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, g_u2rx.dma_buf, U2_DMA_RX_SZ) != HAL_OK) {
////                HAL_UART_AbortReceive(huart);
////                HAL_UARTEx_ReceiveToIdle_DMA(&huart2, g_u2rx.dma_buf, U2_DMA_RX_SZ);
////            }
//            if (HAL_UARTEx_ReceiveToIdle_DMA(MODEM_UART_HANDLE,
//                                             g_u2rx.dma_buf,
//                                             U2_DMA_RX_SZ) != HAL_OK)
//            {
//                HAL_UART_AbortReceive(huart);
//
//                HAL_UARTEx_ReceiveToIdle_DMA(MODEM_UART_HANDLE,
//                                             g_u2rx.dma_buf,
//                                             U2_DMA_RX_SZ);
//            }
//        }
//}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART3)
    {
        mbm_on_rx_event(&g_mbm, huart, Size);
        return;
    }

    if (huart->Instance == MODEM_UART_INSTANCE)
    {
        print("\r\n[DMA RX EVENT] Size=%u\r\n", Size);

        for(uint16_t i = 0; i < Size; i++)
        {
            uint8_t c = g_u2rx.dma_buf[i];

            if(c >= 32 && c <= 126)
                print("%c", c);
            else if(c == '\r')
                print("\\r");
            else if(c == '\n')
                print("\\n");
            else
                print(".");
        }

        print("\r\n");

        u2rx_on_idle_event(&g_u2rx, Size);

        /* Clear possible UART errors */
        __HAL_UART_CLEAR_OREFLAG(huart);
        huart->RxState = HAL_UART_STATE_READY;
        huart->ErrorCode = HAL_UART_ERROR_NONE;

        /* Clear DMA buffer */
        memset(g_u2rx.dma_buf, 0, U2_DMA_RX_SZ);

        /* Re-arm DMA */
        if (HAL_UARTEx_ReceiveToIdle_DMA(MODEM_UART_HANDLE,
                                         g_u2rx.dma_buf,
                                         U2_DMA_RX_SZ) != HAL_OK)
        {
            HAL_UART_AbortReceive(huart);

            HAL_UARTEx_ReceiveToIdle_DMA(MODEM_UART_HANDLE,
                                         g_u2rx.dma_buf,
                                         U2_DMA_RX_SZ);
        }
    }
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  (void)argument;

  /* Give modem time after power-up */
  osDelay(5000);
  HAL_Delay(5000);
  /* Basic bring-up */
  (void)SIM800_SendATCommand2("ATE0", "OK", 1500,true);

  GetIMEI();
  Prepare_MQTT_Topics();
  osDelay(1000);
 WaitForNetworkAndBlinkLED();
 osDelay(1000);

// Check_Internet_Service();

  if (!GPRS_Init(APN)) {
      print("[GPRS] Init failed\r\n");
      osDelay(1000);
      NVIC_SystemReset();
  }

  Check_Internet_Service();
  osDelay(3000);
//
  if (!MQTT_Connect(MQTT_HOST_URI, MQTT_PORT, MQTT_USERNAME, MQTT_PASSWORD)) {
      print("[MQTT] Connect failed\r\n");
      osDelay(1000);
      NVIC_SystemReset();
  }
//
  if (!MQTT_Subscribe(topic_c2d_commands)) {
      print("[MQTT] Subscribe failed\r\n");
      osDelay(1000);
      NVIC_SystemReset();
  }
//
  print("[MQTT] Connected + Subscribed\r\n");
  MQTT_Publish(topic_d2c_logs," logs Device rebooted");

  for (;;)
  {
	  static uint32_t last = 0;



//	      char *rx = MQTT_Subscriber_Run();
//	      if (rx) {
//	          print("[DOWNLINK] %s\r\n", rx);   // print before JSON parse
//	          handle_downlink_cjson(rx);
//	      }
//	      else
//	      {
//
	    	  const char *payload = app_build_payload_json_once();
	    	  print(payload);
	    	  print("\n\n");
	    	  print("Before publish\r\n");

	    	  uint8_t ok = mqttPublish(payload);
//	    	  HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_3);
//	    	  print("After publish\r\n");
//	    	  if (mqttPublish(payload))
//	    	  {
//	    	      // LED ON (assuming active-low LED)
//	    	      HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_RESET);
//	    	      osDelay(1000);
//	    	      HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET);
//	    	  }

	    	  osDelay(5000);
//
//
//
//	      }

	      osDelay(2000);
  }
}


/* USER CODE BEGIN Header_StartTask02 */
/**
* @brief Function implementing the ModbusTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask02 */
void StartTask02(void *argument)
{
  /* USER CODE BEGIN StartTask02 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTask02 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
