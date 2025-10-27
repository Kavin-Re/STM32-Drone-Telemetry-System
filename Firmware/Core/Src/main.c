/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (Drone Telemetry Decoder, OLED rate + time display)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ssd1306.h"
#include "ssd1306_fonts.h"

/* Private typedef -----------------------------------------------------------*/
// --- Telemetry Data Structure ---
typedef struct {
    float altitude;
    float speed;
    float voltage;

    uint32_t timestamp_ms;   // From log (milliseconds from flight start)
    uint32_t hours;          // From log
    uint32_t minutes;
    uint32_t seconds;

    // Previous (for rate calculation)
    float altitude_prev;
    float speed_prev;
    float voltage_prev;
    uint32_t timestamp_prev;

    // Calculated rates
    float altitude_rate;
    float speed_rate;
    float voltage_rate;
} TelemetryData_t;

/* Private define ------------------------------------------------------------*/
#define RX_BUFFER_SIZE 128
#define SD_CS_PORT GPIOB
#define SD_CS_PIN  GPIO_PIN_10

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
TelemetryData_t g_telemetry = {0};
uint8_t rx_buffer[RX_BUFFER_SIZE];
uint8_t rx_char;
FATFS fs;
FIL fil;
FRESULT f_res;
uint32_t bytes_written;
uint8_t is_mounted = 0;
extern Diskio_drvTypeDef  USER_Driver;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
void Telemetry_ReceiveAndParse(void);
void Telemetry_Display(const TelemetryData_t *data);
void Telemetry_Log(const TelemetryData_t *data);
void Mount_SD_Card(void);
/* USER CODE END PFP */
/* USER CODE BEGIN 0 */

/** Attempt to mount SD card and write CSV header if file is new */
void Mount_SD_Card(void)
{
    if (!is_mounted) {
        f_res = f_mount(&fs, "", 1);
        if (f_res == FR_OK) {
            is_mounted = 1;
            f_res = f_open(&fil, "telemetry.csv", FA_OPEN_ALWAYS | FA_WRITE);
            if (f_res == FR_OK) {
                if (f_size(&fil) == 0) {
                    f_puts("TimeMS,Hour,Min,Sec,Altitude,Speed,Voltage\n", &fil);
                }
                f_close(&fil);
            }
        }
    }
}

void Telemetry_Log(const TelemetryData_t *data)
{
    if (!is_mounted) {
        Mount_SD_Card();
        return;
    }
    char logBuffer[100];
    sprintf(logBuffer, "%lu,%lu,%lu,%lu,%.2f,%.2f,%.2f\n",
        data->timestamp_ms, data->hours, data->minutes, data->seconds,
        data->altitude, data->speed, data->voltage);
    f_res = f_open(&fil, "telemetry.csv", FA_OPEN_APPEND | FA_WRITE);
    if (f_res == FR_OK)
    {
        f_write(&fil, logBuffer, strlen(logBuffer), (void*)&bytes_written);
        f_sync(&fil);
        f_close(&fil);
    }
}

void Telemetry_Display(const TelemetryData_t *data)
{
    char lineBuffer[32];

    ssd1306_Fill(Black);

    // Line 1: Time
    ssd1306_SetCursor(0, 0);
    sprintf(lineBuffer, "T:%02lu:%02lu:%02lu", data->hours, data->minutes, data->seconds);
    ssd1306_WriteString(lineBuffer, Font_7x10, White);

    // Line 2: Alt + its rate
    ssd1306_SetCursor(0, 12);
    sprintf(lineBuffer, "ALT:%.2f(%+.2f)", data->altitude, data->altitude_rate);
    ssd1306_WriteString(lineBuffer, Font_7x10, White);

    // Line 3: Speed + its rate
    ssd1306_SetCursor(0, 24);
    sprintf(lineBuffer, "SPD:%.2f(%+.2f)", data->speed, data->speed_rate);
    ssd1306_WriteString(lineBuffer, Font_7x10, White);

    // Line 4: Volt + its rate
    ssd1306_SetCursor(0, 36);
    sprintf(lineBuffer, "V:%.2f(%+.3f)", data->voltage, data->voltage_rate);
    ssd1306_WriteString(lineBuffer, Font_7x10, White);

    ssd1306_SetCursor(0, 48);
    ssd1306_WriteString(is_mounted ? "LOGGING: OK" : "LOGGING: FAIL", Font_6x8, White);

    ssd1306_UpdateScreen();
}

/** Receives one line of telemetry, parses CSV, calculates rates, updates times from log */
void Telemetry_ReceiveAndParse(void)
{
    static uint32_t buffer_index = 0;

    if (HAL_UART_Receive(&huart1, &rx_char, 1, 100) == HAL_OK)
    {
        if (rx_char == '\n' || buffer_index >= RX_BUFFER_SIZE - 1)
        {
            rx_buffer[buffer_index] = '\0';
            buffer_index = 0;

            // --- CSV FORMAT: TimeMS,Hour,Min,Sec,Altitude,Speed,Voltage ---
            uint32_t time_ms, h, m, s;
            float alt, spd, volt;

            int parsed = sscanf((char*)rx_buffer, "%lu,%lu,%lu,%lu,%f,%f,%f",
                &time_ms, &h, &m, &s, &alt, &spd, &volt);
            if (parsed == 7)
            {
                // Compute deltas for rate-of-change
                float delta_time = (time_ms - g_telemetry.timestamp_ms) / 1000.0f;
                if(delta_time < 0.001f) delta_time = 0.001f; // avoid zero

                g_telemetry.altitude_rate = (alt - g_telemetry.altitude) / delta_time;
                g_telemetry.speed_rate    = (spd - g_telemetry.speed) / delta_time;
                g_telemetry.voltage_rate  = (volt - g_telemetry.voltage) / delta_time;

                g_telemetry.timestamp_ms = time_ms;
                g_telemetry.hours = h;
                g_telemetry.minutes = m;
                g_telemetry.seconds = s;
                g_telemetry.altitude = alt;
                g_telemetry.speed = spd;
                g_telemetry.voltage = volt;

                Telemetry_Display(&g_telemetry);
                Telemetry_Log(&g_telemetry);
            }
        } else {
            rx_buffer[buffer_index++] = rx_char;
        }
    }
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  MX_FATFS_Init();

  HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
  ssd1306_Init();
  Mount_SD_Card();
  Telemetry_Display(&g_telemetry);

  while (1)
  {
      Telemetry_ReceiveAndParse();
      HAL_Delay(10);
  }
}
