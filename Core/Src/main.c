/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
osThreadId defaultTaskHandle;

/* USER CODE BEGIN PV */
/* ── ESP32 UART receive buffers ── */
volatile uint8_t  rx_byte;
char              rx_build[16];      /* assembles incoming bytes           */
volatile uint8_t  rx_build_idx = 0;

/* Shared with freertos.c DisplayTask */
char              esp_data[16]  = "Waiting...";   /* last complete frame  */
volatile uint8_t  esp_data_new  = 0;              /* flag: new data ready */
/* USER CODE END PV */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
void StartDefaultTask(void const * argument);

/* USER CODE BEGIN PFP */
void StartDisplayTask(void const * argument);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 10);
    return ch;
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  SystemClock_Config();

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  HAL_Delay(100);
  printf("[MAIN] Mutex - Shared OLED + ESP32 UART Display\r\n");

  extern SemaphoreHandle_t oledMutex;
  oledMutex = xSemaphoreCreateMutex();
  if (oledMutex == NULL)
  {
      printf("[MAIN] FATAL: Mutex creation failed!\r\n");
      Error_Handler();
  }
  printf("[MAIN] Mutex created OK.\r\n");

  /* ── Kick off USART1 byte-by-byte RX interrupt (ESP32 data) ── */
  HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte, 1);
  printf("[MAIN] USART1 RX interrupt armed. Listening for ESP32...\r\n");
  printf("[MAIN] Starting scheduler...\r\n");
  /* USER CODE END 2 */

  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  osThreadDef(DisplayTask, StartDisplayTask, osPriorityNormal, 0, 256);
  osThreadCreate(osThread(DisplayTask), NULL);


  /* USER CODE END RTOS_THREADS */

  osKernelStart();

  while (1) {}
}

/* ================================================================
 * USART1 RX COMPLETE CALLBACK
 * Fires after every byte received from ESP32.
 * Assembles bytes into rx_build[] until '\n', then copies the
 * complete frame into esp_data[] for DisplayTask to consume.
 * Re-arms the interrupt immediately for the next byte.
 * ================================================================ */
/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (rx_byte == '\n')
        {
            /* Complete frame received — null-terminate and publish */
            rx_build[rx_build_idx] = '\0';

            /* Copy into shared display buffer (short copy, safe in ISR) */
            strncpy((char *)esp_data, rx_build, sizeof(esp_data) - 1);
            esp_data[sizeof(esp_data) - 1] = '\0';
            esp_data_new = 1;

            /* Echo to PuTTY so laptop still sees the data */
            printf("[ESP32] %s\r\n", esp_data);

            rx_build_idx = 0;
        }
        else if (rx_byte != '\r')
        {
            if (rx_build_idx < sizeof(rx_build) - 1)
                rx_build[rx_build_idx++] = rx_byte;
            else
                rx_build_idx = 0;   /* overflow — reset frame */
        }

        /* Re-arm for next byte */
        HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte, 1);
    }
}
/* USER CODE END 4 */

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance             = I2C1;
  hi2c1.Init.ClockSpeed      = 100000;
  hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1     = 0;
  hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2     = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

static void MX_USART1_UART_Init(void)
{
  huart1.Instance          = USART1;
  huart1.Init.BaudRate     = 115200;
  huart1.Init.WordLength   = UART_WORDLENGTH_8B;
  huart1.Init.StopBits     = UART_STOPBITS_1;
  huart1.Init.Parity       = UART_PARITY_NONE;
  huart1.Init.Mode         = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

static void MX_USART2_UART_Init(void)
{
  huart2.Instance          = USART2;
  huart2.Init.BaudRate     = 115200;
  huart2.Init.WordLength   = UART_WORDLENGTH_8B;
  huart2.Init.StopBits     = UART_STOPBITS_1;
  huart2.Init.Parity       = UART_PARITY_NONE;
  huart2.Init.Mode         = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitStruct.Pin  = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

void StartDefaultTask(void const * argument)
{
  for(;;) { osDelay(1); }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1) HAL_IncTick();
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {}
#endif
