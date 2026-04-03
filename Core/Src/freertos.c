/* USER CODE BEGIN Header */
/**
 * @brief   : Module 7E2 - Mutex Shared OLED Display + ESP32 UART Data
 * @course  : STM32 Professional - Altrobyte Automation
 * @author  : Pawan Meena (modified)
 */
/* USER CODE END Header */

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "semphr.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
/* USER CODE END Includes */

/* USER CODE BEGIN Variables */
extern I2C_HandleTypeDef hi2c1;

SemaphoreHandle_t oledMutex = NULL;

volatile uint32_t g_counter = 0;

/* ── Shared with main.c UART ISR ── */
extern char             esp_data[16];
extern volatile uint8_t esp_data_new;
/* USER CODE END Variables */

/* USER CODE BEGIN FunctionPrototypes */
void StartDisplayTask(void const * argument);

/* USER CODE END FunctionPrototypes */

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t  **ppxIdleTaskStackBuffer,
                                   uint32_t      *pulIdleTaskStackSize)
{
  static StaticTask_t xIdleTaskTCBBuffer;
  static StackType_t  xIdleStack[configMINIMAL_STACK_SIZE];
  *ppxIdleTaskTCBBuffer  = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize  = configMINIMAL_STACK_SIZE;
}

/* USER CODE BEGIN Application */

/* ================================================================
 * DISPLAY TASK
 * Layout on 128x64 OLED:
 *   Line 0 (y=0 ): "ESP32 Data:"          — static label
 *   Line 1 (y=16): "XXXX"                 — live 4-digit value from ESP32
 *   Line 2 (y=36): " Up: NNs"   — internal uptime
 *   Line 3 (y=54): "Altrobyte FreeRTOS"   — branding
 * ================================================================ */
void StartDisplayTask(void const * argument)
{
    if (oledMutex == NULL)
    {
        printf("[DISPLAY] ERROR: oledMutex is NULL. Exiting task.\r\n");
        vTaskDelete(NULL);
        return;
    }

    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();

    printf("[DISPLAY] OLED initialized. Starting display loop.\r\n");

    char line_esp[20];
    char line_status[24];

    for (;;)
    {
        if (xSemaphoreTake(oledMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            /* ===== CRITICAL SECTION ===== */

            ssd1306_Fill(Black);

            /* ── Row 0: Static label ── */
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("ESP32 Data:", Font_7x10, White);

            /* ── Row 1: Live ESP32 value — large font, centred ──
             * Copy snapshot of esp_data inside the mutex so ISR
             * cannot overwrite it while we are reading             */
            strncpy(line_esp, (char *)esp_data, sizeof(line_esp) - 1);
            line_esp[sizeof(line_esp) - 1] = '\0';

            if (esp_data_new)
                esp_data_new = 0;   /* clear flag inside mutex */

            ssd1306_SetCursor(0, 16);
            ssd1306_WriteString(line_esp, Font_11x18, White);  /* big text */

            /* ── Row 2: Counter + Uptime ── */
            uint32_t uptime = xTaskGetTickCount() / 1000;
            snprintf(line_status, sizeof(line_status),
                     "Uptime:%lus",

                     (unsigned long)uptime);
            ssd1306_SetCursor(0, 38);
            ssd1306_WriteString(line_status, Font_6x8, White);

            /* ── Row 3: Branding ── */
            ssd1306_SetCursor(0, 54);
            ssd1306_WriteString("PRAVEEN KUMAR", Font_6x8, White);

            ssd1306_UpdateScreen();

            /* ===== END CRITICAL SECTION ===== */

            printf("[DISPLAY] ESP=%s | Uptime=%lus\r\n",
                   line_esp,

                   (unsigned long)uptime);

            xSemaphoreGive(oledMutex);
        }
        else
        {
            printf("[DISPLAY] WARNING: Mutex timeout! OLED update skipped.\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ================================================================
 * COUNTER TASK  (unchanged — still increments every 1000ms)
 * ================================================================ */


/* USER CODE END Application */
