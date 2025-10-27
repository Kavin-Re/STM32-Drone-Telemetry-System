/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    user_diskio.c
  * @brief   SD-SPI Disk I/O driver for FATFS
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdlib.h>
#include "ff_gen_drv.h"
#include "main.h" // Includes HAL and peripheral handles

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define SD_CS_PORT GPIOB
#define SD_CS_PIN  GPIO_PIN_10

#define SD_CS_LOW()   HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET)
#define SD_CS_HIGH()  HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET)

// SD Card Commands
#define CMD0   (0x40 + 0)  // GO_IDLE_STATE
#define CMD8   (0x40 + 8)  // SEND_IF_COND
#define CMD17  (0x40 + 17) // READ_SINGLE_BLOCK
#define CMD24  (0x40 + 24) // WRITE_BLOCK
#define CMD55  (0x40 + 55) // APP_CMD
#define CMD41  (0x40 + 41) // SEND_OP_COND
// --- SD Card Command Constants (Additions) ---
#define CMD9   (0x40 + 9)  // SEND_CSD
#define CMD10  (0x40 + 10) // SEND_CID
#define CMD12  (0x40 + 12) // STOP_TRANSMISSION  <-- MISSING (Needed for CMD18 stop)
#define CMD13  (0x40 + 13) // SEND_STATUS
#define CMD18  (0x40 + 18) // READ_MULTIPLE_BLOCK <-- MISSING
#define CMD25  (0x40 + 25) // WRITE_MULTIPLE_BLOCK <-- MISSING
#define CMD58  (0x40 + 58) // READ_OCR <-- MISSING
// ---------------------------------------------
// Data Tokens
#define DATA_START_BLOCK        0xFE
#define WRITE_START_BLOCK       0xFE
#define WRITE_MULTIPLE_BLOCK    0xFC

/* Private variables ---------------------------------------------------------*/
extern SPI_HandleTypeDef hspi1; // Your SPI handle

static volatile DSTATUS Stat = STA_NOINIT;
static BYTE CardType;  // Type of SD card (SDv1/SDv2/MMC)

/* Private function prototypes -----------------------------------------------*/
static void spi_xmit_byte(BYTE data);
static BYTE spi_rcvr_byte(void);
static BYTE spi_wait_ready(void);
static BYTE sd_send_cmd(BYTE cmd, DWORD arg);
static DRESULT sd_rcvr_datablock(BYTE *buff, UINT btr);
static DRESULT sd_xmit_datablock(const BYTE *buff, BYTE token);

/*-----------------------------------------------------------------------*/
/* Low-Level SPI Transfer Functions                                      */
/*-----------------------------------------------------------------------*/

/**
  * @brief Transmits a single byte over SPI1.
  */
static void spi_xmit_byte(BYTE data)
{
    HAL_SPI_Transmit(&hspi1, &data, 1, 100);
}

/**
  * @brief Receives a single byte over SPI1 (sends dummy byte).
  */
static BYTE spi_rcvr_byte(void)
{
    BYTE data;
    BYTE dummy = 0xFF;
    HAL_SPI_TransmitReceive(&hspi1, &dummy, &data, 1, 100);
    return data;
}

/**
  * @brief Waits for the card to transition from busy to ready.
  */
static BYTE spi_wait_ready(void)
{
    BYTE d;
    UINT tmr = 5000; // 500ms timeout at 1ms resolution

    do {
        d = spi_rcvr_byte();
        if (d == 0xFF) return d;
        HAL_Delay(1);
    } while (--tmr);
    return d;
}

/**
  * @brief Sends a command to the SD card.
  * @param cmd: Command byte.
  * @param arg: 32-bit command argument.
  * @retval R1 response.
  */
static BYTE sd_send_cmd(BYTE cmd, DWORD arg)
{
    BYTE n, res;

    // Check for card readiness
    if (cmd != CMD12 && spi_wait_ready() != 0xFF) return 0xFF;

    SD_CS_LOW(); // Select the card

    // Send command packet
    spi_xmit_byte(cmd);
    spi_xmit_byte((BYTE)(arg >> 24));
    spi_xmit_byte((BYTE)(arg >> 16));
    spi_xmit_byte((BYTE)(arg >> 8));
    spi_xmit_byte((BYTE)arg);

    // CRC (only mandatory for CMD0 and CMD8)
    n = 0x01;
    if (cmd == CMD0) n = 0x95;
    if (cmd == CMD8) n = 0x87;
    spi_xmit_byte(n);

    // Receive command response (R1 is single byte, wait up to 10 trials)
    for (n = 10; n; n--) {
        res = spi_rcvr_byte();
        if (!(res & 0x80)) break;
    }

    if (res == 0xFF) SD_CS_HIGH(); // Deselect on failure

    return res;
}

/**
  * @brief Receives a data block from the SD card.
  * @param buff: Pointer to the buffer to store data.
  * @param btr: Number of bytes to receive (should be 512).
  * @retval DRESULT: Operation result.
  */
static DRESULT sd_rcvr_datablock(BYTE *buff, UINT btr)
{
    BYTE token;
    UINT tmr = 10000; // 1000ms timeout

    do {                            // Wait for data start token
        token = spi_rcvr_byte();
        HAL_Delay(1);
    } while ((token == 0xFF) && --tmr);

    if(token != DATA_START_BLOCK) return RES_ERROR;

    // Receive data packet (512 bytes)
    HAL_SPI_Receive(&hspi1, buff, btr, HAL_MAX_DELAY);

    spi_rcvr_byte();                // Discard CRC
    spi_rcvr_byte();

    SD_CS_HIGH();
    spi_rcvr_byte();                // Idle clocks

    return RES_OK;
}

/**
  * @brief Sends a data block to the SD card.
  * @param buff: Pointer to the buffer containing data.
  * @param token: Data token (WRITE_START_BLOCK or WRITE_MULTIPLE_BLOCK).
  * @retval DRESULT: Operation result.
  */
static DRESULT sd_xmit_datablock(const BYTE *buff, BYTE token)
{
    BYTE resp;

    if (spi_wait_ready() != 0xFF) return RES_ERROR;

    spi_xmit_byte(token); // Send token

    if (token != WRITE_MULTIPLE_BLOCK) {
        // Send data packet (512 bytes)
        HAL_SPI_Transmit(&hspi1, (uint8_t*)buff, 512, HAL_MAX_DELAY);

        spi_xmit_byte(0xFF); // Dummy CRC
        spi_xmit_byte(0xFF);

        resp = spi_rcvr_byte(); // Get data response
        if ((resp & 0x1F) != 0x05) return RES_ERROR; // Check for acceptance
    }

    return RES_OK;
}


/*-----------------------------------------------------------------------*/
/* Disk I/O Functions (FATFS Interface)                                  */
/*-----------------------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_initialize (
	BYTE pdrv           /* Physical drive nmuber to identify the drive */
)
{
  /* USER CODE BEGIN INIT */
    BYTE n, ty, ocr[4];
    DWORD tmr;

    if (pdrv) return STA_NOINIT; // Only support drive 0

    // ************************************************************
    // 1. LOW SPEED INIT: Force SPI clock to the minimum speed (/256)
    // ************************************************************
    // Clear the Baud Rate Control bits (BR[2:0]) and set Prescaler /256.
    hspi1.Instance->CR1 &= (~SPI_CR1_BR_Msk);
    hspi1.Instance->CR1 |= SPI_BAUDRATEPRESCALER_256;


    // 2. Initial power-up sequence
    SD_CS_HIGH();
    // Send 80 dummy clock pulses (10 bytes of 0xFF)
    for (n = 10; n; n--) spi_xmit_byte(0xFF);

    // 3. CMD0: GO_IDLE_STATE
    ty = 0;
    if (sd_send_cmd(CMD0, 0) == 1) { // R1 response should be 0x01 (idle state)
        // 4. CMD8: SEND_IF_COND (for SDv2)
        if (sd_send_cmd(CMD8, 0x1AA) == 1) {
            // SDv2/SDHC/SDXC
            for (n = 0; n < 4; n++) ocr[n] = spi_rcvr_byte();
            if ((ocr[2] == 0x01) && (ocr[3] == 0xAA)) {
                // Wait for READY state (ACMD41 with HCS set)
                for (tmr = 1000; tmr; tmr--) {
                    if (sd_send_cmd(CMD55, 0) <= 1 && sd_send_cmd(CMD41, 1UL << 30) == 0) break;
                    HAL_Delay(1);
                }
                if (tmr && sd_send_cmd(CMD58, 0) == 0) { // Check CCS bit
                    for (n = 0; n < 4; n++) ocr[n] = spi_rcvr_byte();
                    ty = (ocr[0] & 0x40) ? 6 : 2; // SDHC/SDXC : SDv2
                }
            }
        } else {
            // SDv1 or MMC (simplified check)
            if (sd_send_cmd(CMD55, 0) <= 1 && sd_send_cmd(CMD41, 0) == 0) ty = 1; // SDv1
            else ty = 3; // MMCv3
        }
    }

    SD_CS_HIGH(); // Deselect
    spi_rcvr_byte(); // Idle clocks

    CardType = ty;
    if (ty) {
        Stat &= ~STA_NOINIT; // Clear NOINIT flag on success

        // ************************************************************
        // 5. HIGH SPEED SWITCH: Switch to fast SPI clock (Prescaler /2)
        // ************************************************************
        // Restore the high-speed setting from MX_SPI1_Init for data transfer.
        hspi1.Instance->CR1 &= (~SPI_CR1_BR_Msk); // Clear Baud Rate bits
        hspi1.Instance->CR1 |= SPI_BAUDRATEPRESCALER_2; // Set Prescaler to /2
    }

    return Stat;
  /* USER CODE END INIT */
}
/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_status (
	BYTE pdrv       /* Physical drive number to identify the drive */
)
{
  /* USER CODE BEGIN STATUS */
    if (pdrv) return STA_NOINIT;
    return Stat;
  /* USER CODE END STATUS */
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT USER_read (
	BYTE pdrv,      /* Physical drive nmuber to identify the drive */
	BYTE *buff,     /* Data buffer to store read data */
	DWORD sector,   /* Sector address in LBA */
	UINT count      /* Number of sectors to read */
)
{
  /* USER CODE BEGIN READ */
    if (pdrv || (Stat & STA_NOINIT)) return RES_NOTRDY;
    if (!(CardType & 4)) sector *= 512; // Convert LBA to byte address for SDv1/MMC

    // Read multiple sectors
    if (count > 1) {
        if (sd_send_cmd(CMD18, sector) != 0) return RES_ERROR;
        do {
            if (sd_rcvr_datablock(buff, 512) != RES_OK) break;
            buff += 512;
        } while (--count);
        sd_send_cmd(CMD12, 0); // Stop transmission
    }
    // Read single sector
    else {
        if (sd_send_cmd(CMD17, sector) == 0) {
            sd_rcvr_datablock(buff, 512);
        }
    }

    SD_CS_HIGH(); // Ensure deselect
    spi_rcvr_byte();

    return count ? RES_ERROR : RES_OK;
  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USER_write (
	BYTE pdrv,          /* Physical drive nmuber to identify the drive */
	const BYTE *buff,   /* Data to be written */
	DWORD sector,       /* Sector address in LBA */
	UINT count          /* Number of sectors to write */
)
{
  /* USER CODE BEGIN WRITE */
    if (pdrv || (Stat & STA_NOINIT)) return RES_NOTRDY;
    if (!(CardType & 4)) sector *= 512; // Convert LBA to byte address

    // Write multiple sectors
    if (count > 1) {
        if (sd_send_cmd(CMD25, sector) != 0) return RES_ERROR;
        do {
            if (sd_xmit_datablock(buff, WRITE_MULTIPLE_BLOCK) != RES_OK) return RES_ERROR;
            buff += 512;
        } while (--count);
        spi_xmit_byte(0xFD); // Stop token
    }
    // Write single sector
    else {
        if (sd_send_cmd(CMD24, sector) == 0) {
            sd_xmit_datablock(buff, WRITE_START_BLOCK);
        }
    }

    SD_CS_HIGH(); // Ensure deselect
    spi_rcvr_byte();

    return count ? RES_ERROR : RES_OK;
  /* USER CODE END WRITE */
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USER_ioctl (
	BYTE pdrv,      /* Physical drive nmuber (0..) */
	BYTE cmd,       /* Control code */
	void *buff      /* Buffer to send/receive control data */
)
{
  /* USER CODE BEGIN IOCTL */
    // Simplified IOCTL implementation
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    DRESULT res = RES_ERROR;

    switch (cmd) {
        case CTRL_SYNC: // Make sure that data has been written to the card
            if (spi_wait_ready() == 0xFF) res = RES_OK;
            break;
        case GET_SECTOR_SIZE: // Returns 512
            *(WORD*)buff = 512;
            res = RES_OK;
            break;
        case GET_BLOCK_SIZE: // Returns 1 (single block operation)
            *(DWORD*)buff = 1;
            res = RES_OK;
            break;
        default:
            res = RES_PARERR;
    }

    SD_CS_HIGH();
    spi_rcvr_byte();

    return res;
  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */

/* Private function prototypes -----------------------------------------------*/
// ... (Your standard function prototypes remain here)

Diskio_drvTypeDef  USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read,
#if  _USE_WRITE
  USER_write,
#endif  /* _USE_WRITE == 1 */
#if  _USE_IOCTL == 1
  USER_ioctl,
#endif /* _USE_IOCTL == 1 */
};
