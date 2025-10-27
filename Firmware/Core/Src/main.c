/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (Drone Telemetry Decoder)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>    // For sprintf
#include <string.h>   // For strtok
#include <stdlib.h>   // For atof (float conversion)

// Assuming this external library header is present (SSD1306)
#include "ssd1306.h"
#include "ssd1306_fonts.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// --- Telemetry Data Structure ---
typedef struct {
    float altitude; // Extracted: meters (m)
    float speed;    // Extracted: km/h or m/s
    float voltage;  // Extracted: Volts (V)
    uint32_t timestamp; // Time in milliseconds
} TelemetryData_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define RX_BUFFER_SIZE 128
#define SD_CS_PORT GPIOB
#define SD_CS_PIN  GPIO_PIN_10
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
TelemetryData_t g_telemetry_data = {0.0f, 0.0f, 0.0f, 0};
uint8_t rx_buffer[RX_BUFFER_SIZE]; // Buffer to store one full line/packet
uint8_t rx_char;                   // Buffer for single character reception

// FATFS Variables
FATFS fs;         // File system object
FIL fil;          // File object
FRESULT f_res;    // File result variable
uint32_t bytes_written;
uint8_t is_mounted = 0; // Flag to check if FS is mounted

// External driver definition for the SD card (implemented in user_diskio.c)
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
void Telemetry_Display(TelemetryData_t *data);
void Telemetry_Log(TelemetryData_t *data);
void Mount_SD_Card(void);
/* USER CODE END PFP */
/* USER CODE BEGIN 0 */

/**
 * @brief Attempts to mount the SD card and write a header if successful.
 */
void Mount_SD_Card(void)
{
    if (!is_mounted) {
        // Mount File System
        f_res = f_mount(&fs, "", 1);
        if (f_res == FR_OK) {
            is_mounted = 1;

            // Open/Create the log file
            f_res = f_open(&fil, "telemetry.csv", FA_OPEN_ALWAYS | FA_WRITE);
            if (f_res == FR_OK) {
                // Move to end of file and write header only if file is new/empty
                if (f_size(&fil) == 0) {
                    f_puts("Time_ms,Altitude_m,Speed_kph,Voltage_V\n", &fil);
                }
                f_close(&fil); // Close file to save changes
            }
        }
    }
}

/**
 * @brief Logs the parsed data to the SD card via FATFS.
 * @param data Pointer to the telemetry data structure.
 */
void Telemetry_Log(TelemetryData_t *data)
{
    if (!is_mounted) {
        Mount_SD_Card(); // Try to mount again if failed
        return;
    }

    char logBuffer[100];
    // Format the data into a CSV line
    sprintf(logBuffer, "%lu,%.2f,%.1f,%.3f\n",
            data->timestamp, data->altitude, data->speed, data->voltage);

    // Re-open the file in append mode
    f_res = f_open(&fil, "telemetry.csv", FA_OPEN_APPEND | FA_WRITE);
    if (f_res == FR_OK)
    {
        // Write data to the file
        f_res = f_write(&fil, logBuffer, strlen(logBuffer), (void*)&bytes_written);
        // Ensure data is written immediately (optional, but safer)
        f_sync(&fil);
        f_close(&fil);
    }
}

/**
 * @brief Updates the SSD1306 OLED display with the latest telemetry data.
 * @param data Pointer to the telemetry data structure.
 */
void Telemetry_Display(TelemetryData_t *data)
{
    char lineBuffer[32];

    // Clear and reset screen (SSD1306 function calls)
    ssd1306_Fill(Black);

    // Line 1: Altitude
    ssd1306_SetCursor(0, 0);
    sprintf(lineBuffer, "ALT: %.2f m", data->altitude);
    ssd1306_WriteString(lineBuffer, Font_7x10, White);

    // Line 2: Speed
    ssd1306_SetCursor(0, 16);
    sprintf(lineBuffer, "SPD: %.1f km/h", data->speed);
    ssd1306_WriteString(lineBuffer, Font_7x10, White);

    // Line 3: Voltage
    ssd1306_SetCursor(0, 32);
    sprintf(lineBuffer, "BAT: %.3f V", data->voltage);
    ssd1306_WriteString(lineBuffer, Font_7x10, White);

    // Line 4: Status (Log status check)
    ssd1306_SetCursor(0, 48);
    if (is_mounted) {
        ssd1306_WriteString("LOGGING: OK", Font_6x8, White);
    } else {
        ssd1306_WriteString("LOGGING: FAIL", Font_6x8, White);
    }

    // Push buffer to physical display
    ssd1306_UpdateScreen();
}

/**
 * @brief Reads incoming UART data character by character and parses a full line.
 */
void Telemetry_ReceiveAndParse(void)
{
    static uint32_t buffer_index = 0;
    static char *token;

    // Receive one character with a short timeout (polling method)
    if (HAL_UART_Receive(&huart1, &rx_char, 1, 100) == HAL_OK)
    {
        // Check for line termination ('\n' from Python script)
        if (rx_char == '\n' || buffer_index >= RX_BUFFER_SIZE - 1)
        {
            rx_buffer[buffer_index] = '\0'; // Null-terminate the received string
            buffer_index = 0; // Reset for next line

            // --- CSV Parsing Logic (Expected format: Altitude,Speed,Voltage) ---

            // 1. Get Altitude
            token = strtok((char*)rx_buffer, ",");
            if (token != NULL) {
                g_telemetry_data.altitude = atof(token);
            } else { return; }

            // 2. Get Speed
            token = strtok(NULL, ",");
            if (token != NULL) {
                g_telemetry_data.speed = atof(token);
            } else { return; }

            // 3. Get Voltage
            token = strtok(NULL, ",");
            if (token != NULL) {
                g_telemetry_data.voltage = atof(token);
            } else { return; }

            g_telemetry_data.timestamp = HAL_GetTick(); // Update time

            // --- Trigger Display and Log after successful parse ---
            Telemetry_Display(&g_telemetry_data);
            Telemetry_Log(&g_telemetry_data);

        } else {
            // Store character and increment index
            rx_buffer[buffer_index++] = rx_char;
        }
    }
}
/* USER CODE END 0 */

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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
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
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  MX_FATFS_Init();
  /* USER CODE BEGIN 2 */

  // 1. Set SD Card CS HIGH (Deselect) initially (PB10 is set to output in MX_GPIO_Init)
  HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);

  // 2. Initialize the OLED display (I2C)
  // This must be called after MX_I2C1_Init
  ssd1306_Init();

  // 3. Initial SD Card Mount Attempt (FATFS)
  // This must be called after MX_SPI1_Init and MX_FATFS_Init
  Mount_SD_Card();

  // 4. Initial Display to show status
  Telemetry_Display(&g_telemetry_data);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      // Continuously check and process incoming telemetry data
      Telemetry_ReceiveAndParse();

      // Small delay to prevent the loop from hogging all CPU cycles
      HAL_Delay(10);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{
  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
  huart1.Init.BaudRate = 9600;
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
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin (PC13) */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PB10 (SD_CS) */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
