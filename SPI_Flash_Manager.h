#ifndef _FLASHMANAGER_H_
#define _FLASHMANAGER_H_

#ifdef __cplusplus
extern "C"
{
#endif  //  __cplusplus


#include <stdbool.h>
#include <string.h>
#include "spi.h"

#define FLASHMAN_DEBUG_DISABLE                    0
#define FLASHMAN_DEBUG_MIN                        1
#define FLASHMAN_DEBUG_FULL                       2

#define FLASHMAN_PLATFORM_HAL                     0
#define FLASHMAN_PLATFORM_HAL_DMA                 1

#define FLASHMAN_RTOS_DISABLE                     0
#define FLASHMAN_RTOS_CMSIS_V1                    1
#define FLASHMAN_RTOS_CMSIS_V2                    2
#define FLASHMAN_RTOS_THREADX                     3


#define FLASHMAN_DEBUG      FLASHMAN_DEBUG_DISABLE
#define FLASHMAN_PLATFORM      FLASHMAN_PLATFORM_HAL
#define FLASHMAN_RTOS      FLASHMAN_RTOS_DISABLE


#define FLASHMAN_PAGE_SIZE                      0x100
#define FLASHMAN_SECTOR_SIZE                    0x1000
#define FLASHMAN_BLOCK_SIZE                     0x10000

#define FLASHMAN_PageToSector(PageNumber)      ((PageNumber * FLASHMAN_PAGE_SIZE) / FLASHMAN_SECTOR_SIZE)
#define FLASHMAN_PageToBlock(PageNumber)       ((PageNumber * FLASHMAN_PAGE_SIZE) / FLASHMAN_BLOCK_SIZE)
#define FLASHMAN_SectorToBlock(SectorNumber)   ((SectorNumber * FLASHMAN_SECTOR_SIZE) / FLASHMAN_BLOCK_SIZE)
#define FLASHMAN_SectorToPage(SectorNumber)    ((SectorNumber * FLASHMAN_SECTOR_SIZE) / FLASHMAN_PAGE_SIZE)
#define FLASHMAN_BlockToPage(BlockNumber)      ((BlockNumber * FLASHMAN_BLOCK_SIZE) / FLASHMAN_PAGE_SIZE)
#define FLASHMAN_PageToAddress(PageNumber)     (PageNumber * FLASHMAN_PAGE_SIZE)
#define FLASHMAN_SectorToAddress(SectorNumber) (SectorNumber * FLASHMAN_SECTOR_SIZE)
#define FLASHMAN_BlockToAddress(BlockNumber)   (BlockNumber * FLASHMAN_BLOCK_SIZE)
#define FLASHMAN_AddressToPage(Address)        (Address / FLASHMAN_PAGE_SIZE)
#define FLASHMAN_AddressToSector(Address)      (Address / FLASHMAN_SECTOR_SIZE)
#define FLASHMAN_AddressToBlock(Address)       (Address / FLASHMAN_BLOCK_SIZE)

#define FLASHMAN_DUMMY_BYTE 0xA5

#define FLASHMAN_CMD_READSFDP 0x5A
#define FLASHMAN_CMD_ID 0x90
#define FLASHMAN_CMD_JEDECID 0x9F
#define FLASHMAN_CMD_UNIQUEID 0x4B
#define FLASHMAN_CMD_WRITEDISABLE 0x04
#define FLASHMAN_CMD_READSTATUS1 0x05
#define FLASHMAN_CMD_READSTATUS2 0x35
#define FLASHMAN_CMD_READSTATUS3 0x15
#define FLASHMAN_CMD_WRITESTATUSEN 0x50
#define FLASHMAN_CMD_WRITESTATUS1 0x01
#define FLASHMAN_CMD_WRITESTATUS2 0x31
#define FLASHMAN_CMD_WRITESTATUS3 0x11
#define FLASHMAN_CMD_WRITEENABLE 0x06
#define FLASHMAN_CMD_ADDR4BYTE_EN 0xB7
#define FLASHMAN_CMD_ADDR4BYTE_DIS 0xE9
#define FLASHMAN_CMD_PAGEPROG3ADD 0x02
#define FLASHMAN_CMD_PAGEPROG4ADD 0x12
#define FLASHMAN_CMD_READDATA3ADD 0x03
#define FLASHMAN_CMD_READDATA4ADD 0x13
#define FLASHMAN_CMD_FASTREAD3ADD 0x0B
#define FLASHMAN_CMD_FASTREAD4ADD 0x0C
#define FLASHMAN_CMD_SECTORERASE3ADD 0x20
#define FLASHMAN_CMD_SECTORERASE4ADD 0x21
#define FLASHMAN_CMD_BLOCKERASE3ADD 0xD8
#define FLASHMAN_CMD_BLOCKERASE4ADD 0xDC
#define FLASHMAN_CMD_CHIPERASE1 0x60
#define FLASHMAN_CMD_CHIPERASE2 0xC7
#define FLASHMAN_CMD_SUSPEND 0x75
#define FLASHMAN_CMD_RESUME 0x7A
#define FLASHMAN_CMD_POWERDOWN 0xB9
#define FLASHMAN_CMD_RELEASE 0xAB
#define FLASHMAN_CMD_FRAMSERNO 0xC3

#define FLASHMAN_STATUS1_BUSY (1 << 0)
#define FLASHMAN_STATUS1_WEL (1 << 1)
#define FLASHMAN_STATUS1_BP0 (1 << 2)
#define FLASHMAN_STATUS1_BP1 (1 << 3)
#define FLASHMAN_STATUS1_BP2 (1 << 4)
#define FLASHMAN_STATUS1_TP (1 << 5)
#define FLASHMAN_STATUS1_SEC (1 << 6)
#define FLASHMAN_STATUS1_SRP0 (1 << 7)

#define FLASHMAN_STATUS2_SRP1 (1 << 0)
#define FLASHMAN_STATUS2_QE (1 << 1)
#define FLASHMAN_STATUS2_RESERVE1 (1 << 2)
#define FLASHMAN_STATUS2_LB0 (1 << 3)
#define FLASHMAN_STATUS2_LB1 (1 << 4)
#define FLASHMAN_STATUS2_LB2 (1 << 5)
#define FLASHMAN_STATUS2_CMP (1 << 6)
#define FLASHMAN_STATUS2_SUS (1 << 7)

#define FLASHMAN_STATUS3_RESERVE1 (1 << 0)
#define FLASHMAN_STATUS3_RESERVE2 (1 << 1)
#define FLASHMAN_STATUS3_WPS (1 << 2)
#define FLASHMAN_STATUS3_RESERVE3 (1 << 3)
#define FLASHMAN_STATUS3_RESERVE4 (1 << 4)
#define FLASHMAN_STATUS3_DRV0 (1 << 5)
#define FLASHMAN_STATUS3_DRV1 (1 << 6)
#define FLASHMAN_STATUS3_HOLD (1 << 7)

typedef enum
{
  FLASHMAN_MANUF_ERROR = 0,
  FLASHMAN_MANUF_WINBOND = 0xEF,
  FLASHMAN_MANUF_ISSI = 0xD5,
  FLASHMAN_MANUF_MICRON = 0x20,
  FLASHMAN_MANUF_GIGADEVICE = 0xC8,
  FLASHMAN_MANUF_MACRONIX = 0xC2,
  FLASHMAN_MANUF_SPANSION = 0x01,
  FLASHMAN_MANUF_AMIC = 0x37,
  FLASHMAN_MANUF_SST = 0xBF,
  FLASHMAN_MANUF_HYUNDAI = 0xAD,
  FLASHMAN_MANUF_ATMEL = 0x1F,
  FLASHMAN_MANUF_FUDAN = 0xA1,
  FLASHMAN_MANUF_ESMT = 0x8C,
  FLASHMAN_MANUF_INTEL = 0x89,
  FLASHMAN_MANUF_SANYO = 0x62,
  FLASHMAN_MANUF_FUJITSU = 0x04,
  FLASHMAN_MANUF_EON = 0x1C,
  FLASHMAN_MANUF_PUYA = 0x85,

} FLASHMAN_MANUFTypeDef;

typedef enum
{
  FLASHMAN_SIZE_ERROR = 0,
  FLASHMAN_SIZE_1MBIT = 0x11,
  FLASHMAN_SIZE_2MBIT = 0x12,
  FLASHMAN_SIZE_4MBIT = 0x13,
  FLASHMAN_SIZE_8MBIT = 0x14,
  FLASHMAN_SIZE_16MBIT = 0x15,
  FLASHMAN_SIZE_32MBIT = 0x16,
  FLASHMAN_SIZE_64MBIT = 0x17,
  FLASHMAN_SIZE_128MBIT = 0x18,
  FLASHMAN_SIZE_256MBIT = 0x19,
  FLASHMAN_SIZE_512MBIT = 0x20,

} FLASHMAN_SizeTypeDef;

typedef struct
{
  SPI_HandleTypeDef      *hspi;
  GPIO_TypeDef           *gpio;
  FLASHMAN_MANUFTypeDef MANUF;
  FLASHMAN_SizeTypeDef       Size;
  uint8_t                Inited;
  uint8_t                MemType;
  uint8_t                Lock;
  uint8_t                Reserved;
  uint32_t               Pin;
  uint32_t               PageCnt;
  uint32_t               SectorCnt;
  uint32_t               BlockCnt;

} FLASHMAN_HandleTypeDef;

bool FLASHMAN_Init(FLASHMAN_HandleTypeDef *Handle, SPI_HandleTypeDef *hspi, GPIO_TypeDef *gpio, uint16_t Pin);

bool FLASHMAN_EraseChip(FLASHMAN_HandleTypeDef *Handle);
bool FLASHMAN_EraseSector(FLASHMAN_HandleTypeDef *Handle, uint32_t Sector);
bool FLASHMAN_EraseBlock(FLASHMAN_HandleTypeDef *Handle, uint32_t Block);

bool FLASHMAN_WriteAddress(FLASHMAN_HandleTypeDef *Handle, uint32_t Address, uint8_t *Data, uint32_t Size);
bool FLASHMAN_WritePage(FLASHMAN_HandleTypeDef *Handle, uint32_t PageNumber, uint8_t *Data, uint32_t Size, uint32_t Offset);
bool FLASHMAN_WriteSector(FLASHMAN_HandleTypeDef *Handle, uint32_t SectorNumber, uint8_t *Data, uint32_t Size, uint32_t Offset);
bool FLASHMAN_WriteBlock(FLASHMAN_HandleTypeDef *Handle, uint32_t BlockNumber, uint8_t *Data, uint32_t Size, uint32_t Offset);

bool FLASHMAN_ReadAddress(FLASHMAN_HandleTypeDef *Handle, uint32_t Address, uint8_t *Data, uint32_t Size);
bool FLASHMAN_ReadPage(FLASHMAN_HandleTypeDef *Handle, uint32_t PageNumber, uint8_t *Data, uint32_t Size, uint32_t Offset);
bool FLASHMAN_ReadSector(FLASHMAN_HandleTypeDef *Handle, uint32_t SectorNumber, uint8_t *Data, uint32_t Size, uint32_t Offset);
bool FLASHMAN_ReadBlock(FLASHMAN_HandleTypeDef *Handle, uint32_t BlockNumber, uint8_t *Data, uint32_t Size, uint32_t Offset);

#ifdef __cplusplus
}
#endif  //  __cplusplus
#endif  //  _FLASHMANAGER_H_