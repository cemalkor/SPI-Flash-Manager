#include "SPI_Flash_Manager.h"

#if FLASHMAN_DEBUG == FLASHMAN_DEBUG_DISABLE
#define dprintf(...)
#else
#include <stdio.h>
#define dprintf(...) printf(__VA_ARGS__)
#endif

#if FLASHMAN_RTOS == FLASHMAN_RTOS_DISABLE
#elif FLASHMAN_RTOS == FLASHMAN_RTOS_CMSIS_V1
#include "cmsis_os.h"
#include "freertos.h"
#elif FLASHMAN_RTOS == FLASHMAN_RTOS_CMSIS_V2
#include "cmsis_os2.h"
#include "freertos.h"
#elif FLASHMAN_RTOS == FLASHMAN_RTOS_THREADX
#include "app_threadx.h"
#endif

static void     FLASHMAN_Delay(uint32_t Delay);
static void     FLASHMAN_Lock(FLASHMAN_HandleTypeDef *Handle);
static void     FLASHMAN_UnLock(FLASHMAN_HandleTypeDef *Handle);
static void     FLASHMAN_CsPin(FLASHMAN_HandleTypeDef *Handle, bool Select);
static bool     FLASHMAN_TransmitReceive(FLASHMAN_HandleTypeDef *Handle, uint8_t *Tx, uint8_t *Rx, size_t Size, uint32_t Timeout);
static bool     FLASHMAN_Transmit(FLASHMAN_HandleTypeDef *Handle, uint8_t *Tx, size_t Size, uint32_t Timeout);
static bool     FLASHMAN_Receive(FLASHMAN_HandleTypeDef *Handle, uint8_t *Rx, size_t Size, uint32_t Timeout);
static bool     FLASHMAN_WriteEnable(FLASHMAN_HandleTypeDef *Handle);
static bool     FLASHMAN_WriteDisable(FLASHMAN_HandleTypeDef *Handle);
static uint8_t  FLASHMAN_ReadReg1(FLASHMAN_HandleTypeDef *Handle);
#ifdef SPECIAL_CONF
static uint8_t  FLASHMAN_ReadReg2(FLASHMAN_HandleTypeDef *Handle);
static uint8_t  FLASHMAN_ReadReg3(FLASHMAN_HandleTypeDef *Handle);
static bool     FLASHMAN_WriteReg1(FLASHMAN_HandleTypeDef *Handle, uint8_t Data);
static bool     FLASHMAN_WriteReg2(FLASHMAN_HandleTypeDef *Handle, uint8_t Data);
static bool     FLASHMAN_WriteReg3(FLASHMAN_HandleTypeDef *Handle, uint8_t Data);
#endif
static bool     FLASHMAN_WaitForWriting(FLASHMAN_HandleTypeDef *Handle, uint32_t Timeout);
static bool     FLASHMAN_FindChip(FLASHMAN_HandleTypeDef *Handle);
static bool     FLASHMAN_WriteFn(FLASHMAN_HandleTypeDef *Handle, uint32_t PageNumber, uint8_t *Data, uint32_t Size, uint32_t Offset);
static bool     FLASHMAN_ReadFn(FLASHMAN_HandleTypeDef *Handle, uint32_t Address, uint8_t *Data, uint32_t Size);

static void FLASHMAN_Delay(uint32_t Delay)
{
#if FLASHMAN_RTOS == FLASHMAN_RTOS_DISABLE
  HAL_Delay(Delay);
#elif (FLASHMAN_RTOS == FLASHMAN_RTOS_CMSIS_V1) || (FLASHMAN_RTOS == FLASHMAN_RTOS_CMSIS_V2)
  uint32_t d = (configTICK_RATE_HZ * Delay) / 1000;
  if (d == 0)
      d = 1;
  osDelay(d);
#elif FLASHMAN_RTOS == FLASHMAN_RTOS_THREADX
  uint32_t d = (TX_TIMER_TICKS_PER_SECOND * Delay) / 1000;
  if (d == 0)
    d = 1;
  tx_thread_sleep(d);
#endif
}

static void FLASHMAN_Lock(FLASHMAN_HandleTypeDef *Handle)
{
  while (Handle->Lock)
  {
    FLASHMAN_Delay(1);
  }
  Handle->Lock = 1;
}

static void FLASHMAN_UnLock(FLASHMAN_HandleTypeDef *Handle)
{
  Handle->Lock = 0;
}

static void FLASHMAN_CsPin(FLASHMAN_HandleTypeDef *Handle, bool Select)
{
  HAL_GPIO_WritePin(Handle->gpio, Handle->Pin, (GPIO_PinState)Select);
  for (int i = 0; i < 10; i++);
}

static bool FLASHMAN_TransmitReceive(FLASHMAN_HandleTypeDef *Handle, uint8_t *Tx, uint8_t *Rx, size_t Size, uint32_t Timeout)
{
  bool retVal = false;
#if (FLASHMAN_PLATFORM == FLASHMAN_PLATFORM_HAL)
  if (HAL_SPI_TransmitReceive(Handle->hspi, Tx, Rx, Size, Timeout) == HAL_OK)
  {
    retVal = true;
  }
  else
  {
    dprintf("FLASHMAN TIMEOUT\r\n");
  }
#elif (FLASHMAN_PLATFORM == FLASHMAN_PLATFORM_HAL_DMA)
  uint32_t startTime = HAL_GetTick();
  if (HAL_SPI_TransmitReceive_DMA(Handle->hspi, Tx, Rx, Size) != HAL_OK)
  {
    dprintf("FLASHMAN TRANSFER ERROR\r\n");
  }
  else
  {
    while (1)
    {
      FLASHMAN_Delay(1);
      if (HAL_GetTick() - startTime >= Timeout)
      {
        dprintf("FLASHMAN TIMEOUT\r\n");
        HAL_SPI_DMAStop(Handle->hspi);
        break;
      }
      if (HAL_SPI_GetState(Handle->hspi) == HAL_SPI_STATE_READY)
      {
        retVal = true;
        break;
      }
    }
  }
#endif
  return retVal;
}

static bool FLASHMAN_Transmit(FLASHMAN_HandleTypeDef *Handle, uint8_t *Tx, size_t Size, uint32_t Timeout)
{
  bool retVal = false;
#if (FLASHMAN_PLATFORM == FLASHMAN_PLATFORM_HAL)
  if (HAL_SPI_Transmit(Handle->hspi, Tx, Size, Timeout) == HAL_OK)
  {
    retVal = true;
  }
  else
  {
    dprintf("FLASHMAN TIMEOUT\r\n");
  }
#elif (FLASHMAN_PLATFORM == FLASHMAN_PLATFORM_HAL_DMA)
  uint32_t startTime = HAL_GetTick();
  if (HAL_SPI_Transmit_DMA(Handle->hspi, Tx, Size) != HAL_OK)
  {
    dprintf("FLASHMAN TRANSFER ERROR\r\n");
  }
  else
  {
    while (1)
    {
      FLASHMAN_Delay(1);
      if (HAL_GetTick() - startTime >= Timeout)
      {
        dprintf("FLASHMAN TIMEOUT\r\n");
        HAL_SPI_DMAStop(Handle->hspi);
        break;
      }
      if (HAL_SPI_GetState(Handle->hspi) == HAL_SPI_STATE_READY)
      {
        retVal = true;
        break;
      }
    }
  }
#endif
  return retVal;
}

static bool FLASHMAN_Receive(FLASHMAN_HandleTypeDef *Handle, uint8_t *Rx, size_t Size, uint32_t Timeout)
{
  bool retVal = false;
#if (FLASHMAN_PLATFORM == FLASHMAN_PLATFORM_HAL)
  if (HAL_SPI_Receive(Handle->hspi, Rx, Size, Timeout) == HAL_OK)
  {
    retVal = true;
  }
  else
  {
    dprintf("FLASHMAN TIMEOUT\r\n");
  }
#elif (FLASHMAN_PLATFORM == FLASHMAN_PLATFORM_HAL_DMA)
  uint32_t startTime = HAL_GetTick();
  if (HAL_SPI_Receive_DMA(Handle->hspi, Rx, Size) != HAL_OK)
  {
    dprintf("FLASHMAN TRANSFER ERROR\r\n");
  }
  else
  {
    while (1)
    {
      FLASHMAN_Delay(1);
      if (HAL_GetTick() - startTime >= Timeout)
      {
        dprintf("FLASHMAN TIMEOUT\r\n");
        HAL_SPI_DMAStop(Handle->hspi);
        break;
      }
      if (HAL_SPI_GetState(Handle->hspi) == HAL_SPI_STATE_READY)
      {
        retVal = true;
        break;
      }
    }
  }
#endif
  return retVal;
}

static bool FLASHMAN_WriteEnable(FLASHMAN_HandleTypeDef *Handle)
{
  bool retVal = true;
  uint8_t tx[1] = {FLASHMAN_CMD_WRITEENABLE};
  FLASHMAN_CsPin(Handle, 0);
  if (FLASHMAN_Transmit(Handle, tx, 1, 100) == false)
  {
    retVal = false;
    dprintf("FLASHMAN_WriteEnable() Error\r\n");
  }
  FLASHMAN_CsPin(Handle, 1);
  return retVal;
}

static bool FLASHMAN_WriteDisable(FLASHMAN_HandleTypeDef *Handle)
{
  bool retVal = true;
  uint8_t tx[1] = {FLASHMAN_CMD_WRITEDISABLE};
  FLASHMAN_CsPin(Handle, 0);
  if (FLASHMAN_Transmit(Handle, tx, 1, 100) == false)
  {
    retVal = false;
    dprintf("FLASHMAN_WriteDisable() Error\r\n");
  }
  FLASHMAN_CsPin(Handle, 1);
  return retVal;
}

static uint8_t FLASHMAN_ReadReg1(FLASHMAN_HandleTypeDef *Handle)
{
  uint8_t retVal = 0;
  uint8_t tx[2] = {FLASHMAN_CMD_READSTATUS1, FLASHMAN_DUMMY_BYTE};
  uint8_t rx[2];
  FLASHMAN_CsPin(Handle, 0);
  if (FLASHMAN_TransmitReceive(Handle, tx, rx, 2, 100) == true)
  {
    retVal = rx[1];
  }
  FLASHMAN_CsPin(Handle, 1);
  return retVal;
}

#ifdef SPECIAL_CONF
static uint8_t FLASHMAN_ReadReg2(FLASHMAN_HandleTypeDef *Handle)
{
  uint8_t retVal = 0;
  uint8_t tx[2] = {FLASHMAN_CMD_READSTATUS2, FLASHMAN_DUMMY_BYTE};
  uint8_t rx[2];
  FLASHMAN_CsPin(Handle, 0);
  if (FLASHMAN_TransmitReceive(Handle, tx, rx, 2, 100) == true)
  {
    retVal = rx[1];
  }
  FLASHMAN_CsPin(Handle, 1);
  return retVal;
}

static uint8_t FLASHMAN_ReadReg3(FLASHMAN_HandleTypeDef *Handle)
{
  uint8_t retVal = 0;
  uint8_t tx[2] = {FLASHMAN_CMD_READSTATUS3, FLASHMAN_DUMMY_BYTE};
  uint8_t rx[2];
  FLASHMAN_CsPin(Handle, 0);
  if (FLASHMAN_TransmitReceive(Handle, tx, rx, 2, 100) == true)
  {
    retVal = rx[1];
  }
  FLASHMAN_CsPin(Handle, 1);
  return retVal;
}

static bool FLASHMAN_WriteReg1(FLASHMAN_HandleTypeDef *Handle, uint8_t Data)
{
  bool retVal = true;
  uint8_t tx[2] = {FLASHMAN_CMD_WRITESTATUS1, Data};
  uint8_t cmd = FLASHMAN_CMD_WRITESTATUSEN;
  do
  {
    FLASHMAN_CsPin(Handle, 0);
    if (FLASHMAN_Transmit(Handle, &cmd, 1, 100) == false)
    {
      retVal = false;
      FLASHMAN_CsPin(Handle, 1);
      break;
    }
    FLASHMAN_CsPin(Handle, 1);
    FLASHMAN_CsPin(Handle, 0);
    if (FLASHMAN_Transmit(Handle, tx, 2, 100) == false)
    {
      retVal = false;
      FLASHMAN_CsPin(Handle, 1);
      break;
    }
    FLASHMAN_CsPin(Handle, 1);
  } while (0);

  return retVal;
}

static bool FLASHMAN_WriteReg2(FLASHMAN_HandleTypeDef *Handle, uint8_t Data)
{
  bool retVal = true;
  uint8_t tx[2] = {FLASHMAN_CMD_WRITESTATUS2, Data};
  uint8_t cmd = FLASHMAN_CMD_WRITESTATUSEN;
  do
  {
    FLASHMAN_CsPin(Handle, 0);
    if (FLASHMAN_Transmit(Handle, &cmd, 1, 100) == false)
    {
      retVal = false;
      FLASHMAN_CsPin(Handle, 1);
      break;
    }
    FLASHMAN_CsPin(Handle, 1);
    FLASHMAN_CsPin(Handle, 0);
    if (FLASHMAN_Transmit(Handle, tx, 2, 100) == false)
    {
      retVal = false;
      FLASHMAN_CsPin(Handle, 1);
      break;
    }
    FLASHMAN_CsPin(Handle, 1);
  } while (0);

  return retVal;
}

static bool FLASHMAN_WriteReg3(FLASHMAN_HandleTypeDef *Handle, uint8_t Data)
{
  bool retVal = true;
  uint8_t tx[2] = {FLASHMAN_CMD_WRITESTATUS3, Data};
  uint8_t cmd = FLASHMAN_CMD_WRITESTATUSEN;
  do
  {
    FLASHMAN_CsPin(Handle, 0);
    if (FLASHMAN_Transmit(Handle, &cmd, 1, 100) == false)
    {
      retVal = false;
      FLASHMAN_CsPin(Handle, 1);
      break;
    }
    FLASHMAN_CsPin(Handle, 1);
    FLASHMAN_CsPin(Handle, 0);
    if (FLASHMAN_Transmit(Handle, tx, 2, 100) == false)
    {
      retVal = false;
      FLASHMAN_CsPin(Handle, 1);
      break;
    }
    FLASHMAN_CsPin(Handle, 1);
  } while (0);

  return retVal;
}
#endif

static bool FLASHMAN_WaitForWriting(FLASHMAN_HandleTypeDef *Handle, uint32_t Timeout)
{
  bool retVal = false;
  uint32_t startTime = HAL_GetTick();
  while (1)
  {
    FLASHMAN_Delay(1);
    if (HAL_GetTick() - startTime >= Timeout)
    {
      dprintf("FLASHMAN_WaitForWriting() TIMEOUT\r\n");
      break;
    }
    if ((FLASHMAN_ReadReg1(Handle) & FLASHMAN_STATUS1_BUSY) == 0)
    {
      retVal = true;
      break;
    }
  }
  return retVal;
}

static bool FLASHMAN_FindChip(FLASHMAN_HandleTypeDef *Handle)
{
  uint8_t tx[4] = {FLASHMAN_CMD_JEDECID, 0xFF, 0xFF, 0xFF};
  uint8_t rx[4];
  bool retVal = false;
  do
  {
    dprintf("FLASHMAN_FindChip()\r\n");
    FLASHMAN_CsPin(Handle, 0);
    if (FLASHMAN_TransmitReceive(Handle, tx, rx, 4, 100) == false)
    {
      FLASHMAN_CsPin(Handle, 1);
      break;
    }
    FLASHMAN_CsPin(Handle, 1);
    dprintf("CHIP ID: 0x%02X%02X%02X\r\n", rx[1], rx[2], rx[3]);
    Handle->MANUF = rx[1];
    Handle->MemType = rx[2];
    Handle->Size = rx[3];

    dprintf("FLASHMAN MANUFACTURE: ");
    switch (Handle->MANUF)
    {
    case FLASHMAN_MANUF_WINBOND:
      dprintf("WINBOND");
      break;
    case FLASHMAN_MANUF_SPANSION:
      dprintf("SPANSION");
      break;
    case FLASHMAN_MANUF_MICRON:
      dprintf("MICRON");
      break;
    case FLASHMAN_MANUF_MACRONIX:
      dprintf("MACRONIX");
      break;
    case FLASHMAN_MANUF_ISSI:
      dprintf("ISSI");
      break;
    case FLASHMAN_MANUF_GIGADEVICE:
      dprintf("GIGADEVICE");
      break;
    case FLASHMAN_MANUF_AMIC:
      dprintf("AMIC");
      break;
    case FLASHMAN_MANUF_SST:
      dprintf("SST");
      break;
    case FLASHMAN_MANUF_HYUNDAI:
      dprintf("HYUNDAI");
      break;
    case FLASHMAN_MANUF_FUDAN:
      dprintf("FUDAN");
      break;
    case FLASHMAN_MANUF_ESMT:
      dprintf("ESMT");
      break;
    case FLASHMAN_MANUF_INTEL:
      dprintf("INTEL");
      break;
    case FLASHMAN_MANUF_SANYO:
      dprintf("SANYO");
      break;
    case FLASHMAN_MANUF_FUJITSU:
      dprintf("FUJITSU");
      break;
    case FLASHMAN_MANUF_EON:
      dprintf("EON");
      break;
    case FLASHMAN_MANUF_PUYA:
      dprintf("PUYA");
      break;
    default:
      Handle->MANUF = FLASHMAN_MANUF_ERROR;
      dprintf("ERROR");
      break;
    }
    dprintf(" - MEMTYPE: 0x%02X", Handle->MemType);
    dprintf(" - SIZE: ");
    switch (Handle->Size)
    {
    case FLASHMAN_SIZE_1MBIT:
      Handle->BlockCnt = 2;
      dprintf("1 MBIT\r\n");
      break;
    case FLASHMAN_SIZE_2MBIT:
      Handle->BlockCnt = 4;
      dprintf("2 MBIT\r\n");
      break;
    case FLASHMAN_SIZE_4MBIT:
      Handle->BlockCnt = 8;
      dprintf("4 MBIT\r\n");
      break;
    case FLASHMAN_SIZE_8MBIT:
      Handle->BlockCnt = 16;
      dprintf("8 MBIT\r\n");
      break;
    case FLASHMAN_SIZE_16MBIT:
      Handle->BlockCnt = 32;
      dprintf("16 MBIT\r\n");
      break;
    case FLASHMAN_SIZE_32MBIT:
      Handle->BlockCnt = 64;
      dprintf("32 MBIT\r\n");
      break;
    case FLASHMAN_SIZE_64MBIT:
      Handle->BlockCnt = 128;
      dprintf("64 MBIT\r\n");
      break;
    case FLASHMAN_SIZE_128MBIT:
      Handle->BlockCnt = 256;
      dprintf("128 MBIT\r\n");
      break;
    case FLASHMAN_SIZE_256MBIT:
      Handle->BlockCnt = 512;
      dprintf("256 MBIT\r\n");
      break;
    case FLASHMAN_SIZE_512MBIT:
      Handle->BlockCnt = 1024;
      dprintf("512 MBIT\r\n");
      break;
    default:
      Handle->Size = FLASHMAN_SIZE_ERROR;
      dprintf("ERROR\r\n");
      break;
    }

    Handle->SectorCnt = Handle->BlockCnt * 16;
    Handle->PageCnt = (Handle->SectorCnt * FLASHMAN_SECTOR_SIZE) / FLASHMAN_PAGE_SIZE;
    dprintf("FLASHMAN BLOCK CNT: %ld\r\n", Handle->BlockCnt);
    dprintf("FLASHMAN SECTOR CNT: %ld\r\n", Handle->SectorCnt);
    dprintf("FLASHMAN PAGE CNT: %ld\r\n", Handle->PageCnt);
    dprintf("FLASHMAN STATUS1: 0x%02X\r\n", FLASHMAN_ReadReg1(Handle));
    dprintf("FLASHMAN STATUS2: 0x%02X\r\n", FLASHMAN_ReadReg2(Handle));
    dprintf("FLASHMAN STATUS3: 0x%02X\r\n", FLASHMAN_ReadReg3(Handle));
    retVal = true;

  } while (0);

  return retVal;
}

static bool FLASHMAN_WriteFn(FLASHMAN_HandleTypeDef *Handle, uint32_t PageNumber, uint8_t *Data, uint32_t Size, uint32_t Offset)
{
  bool retVal = false;
  uint32_t address = 0, maximum = FLASHMAN_PAGE_SIZE - Offset;
  uint8_t tx[5];
  do
  {
#if FLASHMAN_DEBUG != FLASHMAN_DEBUG_DISABLE
    uint32_t dbgTime = HAL_GetTick();
#endif
    dprintf("FLASHMAN_WritePage() START PAGE %ld\r\n", PageNumber);
    if (PageNumber >= Handle->PageCnt)
    {
      dprintf("FLASHMAN_WritePage() ERROR PageNumber\r\n");
      break;
    }
    if (Offset >= FLASHMAN_PAGE_SIZE)
    {
      dprintf("FLASHMAN_WritePage() ERROR Offset\r\n");
      break;
    }
    if (Size > maximum)
    {
      Size = maximum;
    }
    address = FLASHMAN_PageToAddress(PageNumber) + Offset;
#if FLASHMAN_DEBUG == FLASHMAN_DEBUG_FULL
      dprintf("FLASHMAN WRITING {\r\n0x%02X", Data[0]);
      for (int i = 1; i < Size; i++)
      {
        if (i % 8 == 0)
        {
          dprintf("\r\n");
        }
        dprintf(", 0x%02X", Data[i]);
      }
      dprintf("\r\n}\r\n");
#endif
    if (FLASHMAN_WriteEnable(Handle) == false)
    {
      break;
    }
    FLASHMAN_CsPin(Handle, 0);
    if (Handle->BlockCnt >= 512)
    {
      tx[0] = FLASHMAN_CMD_PAGEPROG4ADD;
      tx[1] = (address & 0xFF000000) >> 24;
      tx[2] = (address & 0x00FF0000) >> 16;
      tx[3] = (address & 0x0000FF00) >> 8;
      tx[4] = (address & 0x000000FF);
      if (FLASHMAN_Transmit(Handle, tx, 5, 100) == false)
      {
        FLASHMAN_CsPin(Handle, 1);
        break;
      }
    }
    else
    {
      tx[0] = FLASHMAN_CMD_PAGEPROG3ADD;
      tx[1] = (address & 0x00FF0000) >> 16;
      tx[2] = (address & 0x0000FF00) >> 8;
      tx[3] = (address & 0x000000FF);
      if (FLASHMAN_Transmit(Handle, tx, 4, 100) == false)
      {
        FLASHMAN_CsPin(Handle, 1);
        break;
      }
    }
    if (FLASHMAN_Transmit(Handle, Data, Size, 1000) == false)
    {
      FLASHMAN_CsPin(Handle, 1);
      break;
    }
    FLASHMAN_CsPin(Handle, 1);
    if (FLASHMAN_WaitForWriting(Handle, 100))
    {
      dprintf("FLASHMAN_WritePage() %d BYTES WITERN DONE AFTER %ld ms\r\n", (uint16_t)Size, HAL_GetTick() - dbgTime);
      retVal = true;
    }

  } while (0);

  FLASHMAN_WriteDisable(Handle);
  return retVal;
}

static bool FLASHMAN_ReadFn(FLASHMAN_HandleTypeDef *Handle, uint32_t Address, uint8_t *Data, uint32_t Size)
{
  bool retVal = false;
  uint8_t tx[5];
  do
  {
#if FLASHMAN_DEBUG != FLASHMAN_DEBUG_DISABLE
    uint32_t dbgTime = HAL_GetTick();
#endif
    dprintf("FLASHMAN_ReadAddress() START ADDRESS %ld\r\n", Address);
    FLASHMAN_CsPin(Handle, 0);
    if (Handle->BlockCnt >= 512)
    {
      tx[0] = FLASHMAN_CMD_READDATA4ADD;
      tx[1] = (Address & 0xFF000000) >> 24;
      tx[2] = (Address & 0x00FF0000) >> 16;
      tx[3] = (Address & 0x0000FF00) >> 8;
      tx[4] = (Address & 0x000000FF);
      if (FLASHMAN_Transmit(Handle, tx, 5, 100) == false)
      {
        FLASHMAN_CsPin(Handle, 1);
        break;
      }
    }
    else
    {
      tx[0] = FLASHMAN_CMD_READDATA3ADD;
      tx[1] = (Address & 0x00FF0000) >> 16;
      tx[2] = (Address & 0x0000FF00) >> 8;
      tx[3] = (Address & 0x000000FF);
      if (FLASHMAN_Transmit(Handle, tx, 4, 100) == false)
      {
        FLASHMAN_CsPin(Handle, 1);
        break;
      }
    }
    if (FLASHMAN_Receive(Handle, Data, Size, 2000) == false)
    {
      FLASHMAN_CsPin(Handle, 1);
      break;
    }
    FLASHMAN_CsPin(Handle, 1);
    dprintf("FLASHMAN_ReadAddress() %d BYTES READ DONE AFTER %ld ms\r\n", (uint16_t)Size, HAL_GetTick() - dbgTime);
#if FLASHMAN_DEBUG == FLASHMAN_DEBUG_FULL
    dprintf("{\r\n0x%02X", Data[0]);
    for (int i = 1; i < Size; i++)
    {
      if (i % 8 == 0)
      {
        dprintf("\r\n");
      }
      dprintf(", 0x%02X", Data[i]);
    }
    dprintf("\r\n}\r\n");
#endif
    retVal = true;

  } while (0);

  return retVal;
}

/**
  * @brief  Initialize the FLASHMAN.
  * @note   Enable and configure the SPI and Set GPIO as output for CS pin on the CubeMX
  *
  * @param  *Handle: Pointer to FLASHMAN_HandleTypeDef structure
  * @param  *hspi: Pointer to a SPI_HandleTypeDef structure
  * @param  *gpio: Pointer to a GPIO_TypeDef structure for CS
  * @param  Pin: Pin of CS
  *
  * @retval bool: true or false
  */
bool FLASHMAN_Init(FLASHMAN_HandleTypeDef *Handle, SPI_HandleTypeDef *hspi, GPIO_TypeDef *gpio, uint16_t Pin)
{
  bool retVal = false;
  do
  {
    if ((Handle == NULL) || (hspi == NULL) || (gpio == NULL) || (Handle->Inited == 1))
    {
      dprintf("FLASHMAN_Init() Error, Wrong Parameter\r\n");
      break;
    }
    memset(Handle, 0, sizeof(FLASHMAN_HandleTypeDef));
    Handle->hspi = hspi;
    Handle->gpio = gpio;
    Handle->Pin = Pin;
    FLASHMAN_CsPin(Handle, 1);
    /* wait for stable VCC */
    while (HAL_GetTick() < 20)
    {
      FLASHMAN_Delay(1);
    }
    if (FLASHMAN_WriteDisable(Handle) == false)
    {
      break;
    }
    retVal = FLASHMAN_FindChip(Handle);
    if (retVal)
    {
      Handle->Inited = 1;
      dprintf("FLASHMAN_Init() Done\r\n");
    }

  } while (0);

  return retVal;
}

/**
  * @brief  Full Erase chip.
  * @note   Send the Full-Erase-chip command and wait for completion
  *
  * @param  *Handle: Pointer to FLASHMAN_HandleTypeDef structure
  *
  * @retval bool: true or false
  */
bool FLASHMAN_EraseChip(FLASHMAN_HandleTypeDef *Handle)
{
  FLASHMAN_Lock(Handle);
  bool retVal = false;
  uint8_t tx[1] = {FLASHMAN_CMD_CHIPERASE2};
  do
  {
#if FLASHMAN_DEBUG != FLASHMAN_DEBUG_DISABLE
    uint32_t dbgTime = HAL_GetTick();
#endif
    dprintf("FLASHMAN_EraseChip() START\r\n");
    if (FLASHMAN_WriteEnable(Handle) == false)
    {
      break;
    }
    FLASHMAN_CsPin(Handle, 0);
    if (FLASHMAN_Transmit(Handle, tx, 1, 100) == false)
    {
      FLASHMAN_CsPin(Handle, 1);
      break;
    }
    FLASHMAN_CsPin(Handle, 1);
    if (FLASHMAN_WaitForWriting(Handle, Handle->BlockCnt * 1000))
    {
      dprintf("FLASHMAN_EraseChip() DONE AFTER %ld ms\r\n", HAL_GetTick() - dbgTime);
      retVal = true;
    }

  } while (0);

  FLASHMAN_WriteDisable(Handle);
  FLASHMAN_UnLock(Handle);
  return retVal;
}

/**
  * @brief  Erase Sector.
  * @note   Send the Erase-Sector command and wait for completion
  *
  * @param  *Handle: Pointer to FLASHMAN_HandleTypeDef structure
  * @param  Sector: Selected Sector
  *
  * @retval bool: true or false
  */
bool FLASHMAN_EraseSector(FLASHMAN_HandleTypeDef *Handle, uint32_t Sector)
{
  FLASHMAN_Lock(Handle);
  bool retVal = false;
  uint32_t address = Sector * FLASHMAN_SECTOR_SIZE;
  uint8_t tx[5];
  do
  {
#if FLASHMAN_DEBUG != FLASHMAN_DEBUG_DISABLE
    uint32_t dbgTime = HAL_GetTick();
#endif
    dprintf("FLASHMAN_EraseSector() START SECTOR %ld\r\n", Sector);
    if (Sector >= Handle->SectorCnt)
    {
      dprintf("FLASHMAN_EraseSector() ERROR Sector NUMBER\r\n");
      break;
    }
    if (FLASHMAN_WriteEnable(Handle) == false)
    {
      break;
    }
    FLASHMAN_CsPin(Handle, 0);
    if (Handle->BlockCnt >= 512)
    {
      tx[0] = FLASHMAN_CMD_SECTORERASE4ADD;
      tx[1] = (address & 0xFF000000) >> 24;
      tx[2] = (address & 0x00FF0000) >> 16;
      tx[3] = (address & 0x0000FF00) >> 8;
      tx[4] = (address & 0x000000FF);
      if (FLASHMAN_Transmit(Handle, tx, 5, 100) == false)
      {
        FLASHMAN_CsPin(Handle, 1);
        break;
      }
    }
    else
    {
      tx[0] = FLASHMAN_CMD_SECTORERASE3ADD;
      tx[1] = (address & 0x00FF0000) >> 16;
      tx[2] = (address & 0x0000FF00) >> 8;
      tx[3] = (address & 0x000000FF);
      if (FLASHMAN_Transmit(Handle, tx, 4, 100) == false)
      {
        FLASHMAN_CsPin(Handle, 1);
        break;
      }
    }
    FLASHMAN_CsPin(Handle, 1);
    if (FLASHMAN_WaitForWriting(Handle, 1000))
    {
      dprintf("FLASHMAN_EraseSector() DONE AFTER %ld ms\r\n", HAL_GetTick() - dbgTime);
      retVal = true;
    }

  } while (0);

  FLASHMAN_WriteDisable(Handle);
  FLASHMAN_UnLock(Handle);
  return retVal;
}

/**
  * @brief  Erase Block.
  * @note   Send the Erase-Block command and wait for completion
  *
  * @param  *Handle: Pointer to FLASHMAN_HandleTypeDef structure
  * @param  Sector: Selected Block
  *
  * @retval bool: true or false
  */
bool FLASHMAN_EraseBlock(FLASHMAN_HandleTypeDef *Handle, uint32_t Block)
{
  FLASHMAN_Lock(Handle);
  bool retVal = false;
  uint32_t address = Block * FLASHMAN_BLOCK_SIZE;
  uint8_t tx[5];
  do
  {
#if FLASHMAN_DEBUG != FLASHMAN_DEBUG_DISABLE
    uint32_t dbgTime = HAL_GetTick();
#endif
    dprintf("FLASHMAN_EraseBlock() START PAGE %ld\r\n", Block);
    if (Block >= Handle->BlockCnt)
    {
      dprintf("FLASHMAN_EraseBlock() ERROR Block NUMBER\r\n");
      break;
    }
    if (FLASHMAN_WriteEnable(Handle) == false)
    {
      break;
    }
    FLASHMAN_CsPin(Handle, 0);
    if (Handle->BlockCnt >= 512)
    {
      tx[0] = FLASHMAN_CMD_BLOCKERASE4ADD;
      tx[1] = (address & 0xFF000000) >> 24;
      tx[2] = (address & 0x00FF0000) >> 16;
      tx[3] = (address & 0x0000FF00) >> 8;
      tx[4] = (address & 0x000000FF);
      if (FLASHMAN_Transmit(Handle, tx, 5, 100) == false)
      {
        FLASHMAN_CsPin(Handle, 1);
        break;
      }
    }
    else
    {
      tx[0] = FLASHMAN_CMD_BLOCKERASE3ADD;
      tx[1] = (address & 0x00FF0000) >> 16;
      tx[2] = (address & 0x0000FF00) >> 8;
      tx[3] = (address & 0x000000FF);
      if (FLASHMAN_Transmit(Handle, tx, 4, 100) == false)
      {
        FLASHMAN_CsPin(Handle, 1);
        break;
      }
    }
    FLASHMAN_CsPin(Handle, 1);
    if (FLASHMAN_WaitForWriting(Handle, 3000))
    {
      dprintf("FLASHMAN_EraseBlock() DONE AFTER %ld ms\r\n", HAL_GetTick() - dbgTime);
      retVal = true;
    }

  } while (0);

  FLASHMAN_WriteDisable(Handle);
  FLASHMAN_UnLock(Handle);
  return retVal;
}

/**
  * @brief  Write data array to an Address
  * @note   Write a data array with specified size.
  * @note   All pages should be erased before write
  *
  * @param  *Handle: Pointer to FLASHMAN_HandleTypeDef structure
  * @param  Address: Start Address
  * @param  *Data: Pointer to Data
  * @param  Size: The length of data should be written. (in byte)
  *
  * @retval bool: true or false
  */
bool FLASHMAN_WriteAddress(FLASHMAN_HandleTypeDef *Handle, uint32_t Address, uint8_t *Data, uint32_t Size)
{
  FLASHMAN_Lock(Handle);
  bool retVal = false;
  uint32_t page, add, offset, remaining, length, maximum, index = 0;
  add = Address;
  remaining = Size;
  do
  {
    page = FLASHMAN_AddressToPage(add);
    offset = add % FLASHMAN_PAGE_SIZE;
    maximum = FLASHMAN_PAGE_SIZE - offset;
    if (remaining <= maximum)
    {
      length = remaining;
    }
    else
    {
      length = maximum;
    }
    if (FLASHMAN_WriteFn(Handle, page, &Data[index], length, offset) == false)
    {
      break;
    }
    add += length;
    index += length;
    remaining -= length;
    if (remaining == 0)
    {
      retVal = true;
      break;
    }

  } while (remaining > 0);

  FLASHMAN_UnLock(Handle);
  return retVal;
}

/**
  * @brief  Write data array to a Page
  * @note   Write a data array with specified size.
  * @note   The Page should be erased before write
  *
  * @param  *Handle: Pointer to FLASHMAN_HandleTypeDef structure
  * @param  PageNumber: Page Number
  * @param  *Data: Pointer to Data
  * @param  Size: The length of data should be written. (in byte)
  * @param  Offset: The start point for writing data. (in byte)
  *
  * @retval bool: true or false
  */
bool FLASHMAN_WritePage(FLASHMAN_HandleTypeDef *Handle, uint32_t PageNumber, uint8_t *Data, uint32_t Size, uint32_t Offset)
{
  FLASHMAN_Lock(Handle);
  bool retVal = false;
  retVal = FLASHMAN_WriteFn(Handle, PageNumber, Data, Size, Offset);
  FLASHMAN_UnLock(Handle);
  return retVal;
}

/**
  * @brief  Write data array to a Sector
  * @note   Write a data array with specified size.
  * @note   The Sector should be erased before write
  *
  * @param  *Handle: Pointer to FLASHMAN_HandleTypeDef structure
  * @param  SectorNumber: Sector Number
  * @param  *Data: Pointer to Data
  * @param  Size: The length of data should be written. (in byte)
  * @param  Offset: The start point for writing data. (in byte)
  *
  * @retval bool: true or false
  */
bool FLASHMAN_WriteSector(FLASHMAN_HandleTypeDef *Handle, uint32_t SectorNumber, uint8_t *Data, uint32_t Size, uint32_t Offset)
{
  FLASHMAN_Lock(Handle);
  bool retVal = true;
  do
  {
    if (Offset >= FLASHMAN_SECTOR_SIZE)
    {
      retVal = false;
      break;
    }
    if (Size > (FLASHMAN_SECTOR_SIZE - Offset))
    {
      Size = FLASHMAN_SECTOR_SIZE - Offset;
    }
    uint32_t bytesWritten = 0;
    uint32_t pageNumber = SectorNumber * (FLASHMAN_SECTOR_SIZE / FLASHMAN_PAGE_SIZE);
    pageNumber += Offset / FLASHMAN_PAGE_SIZE;
    uint32_t remainingBytes = Size;
    uint32_t pageOffset = Offset % FLASHMAN_PAGE_SIZE;
    while (remainingBytes > 0 && pageNumber < ((SectorNumber + 1) * (FLASHMAN_SECTOR_SIZE / FLASHMAN_PAGE_SIZE)))
    {
      uint32_t bytesToWrite = (remainingBytes > FLASHMAN_PAGE_SIZE) ? FLASHMAN_PAGE_SIZE : remainingBytes;
      if (FLASHMAN_WriteFn(Handle, pageNumber, Data + bytesWritten, bytesToWrite, pageOffset) == false)
      {
        retVal = false;
        break;
      }
      bytesWritten += bytesToWrite;
      remainingBytes -= bytesToWrite;
      pageNumber++;
      pageOffset = 0;
    }
  } while (0);
  FLASHMAN_UnLock(Handle);
  return retVal;
}

/**
  * @brief  Write data array to a Block
  * @note   Write a data array with specified size.
  * @note   The Block should be erased before write
  *
  * @param  *Handle: Pointer to FLASHMAN_HandleTypeDef structure
  * @param  SectorNumber: Block Number
  * @param  *Data: Pointer to Data
  * @param  Size: The length of data should be written. (in byte)
  * @param  Offset: The start point for writing data. (in byte)
  *
  * @retval bool: true or false
  */
bool FLASHMAN_WriteBlock(FLASHMAN_HandleTypeDef *Handle, uint32_t BlockNumber, uint8_t *Data, uint32_t Size, uint32_t Offset)
{
  FLASHMAN_Lock(Handle);
  bool retVal = true;
  do
  {
    if (Offset >= FLASHMAN_BLOCK_SIZE)
    {
      retVal = false;
      break;
    }
    if (Size > (FLASHMAN_BLOCK_SIZE - Offset))
    {
      Size = FLASHMAN_BLOCK_SIZE - Offset;
    }
    uint32_t bytesWritten = 0;
    uint32_t pageNumber = BlockNumber * (FLASHMAN_BLOCK_SIZE / FLASHMAN_PAGE_SIZE);
    pageNumber += Offset / FLASHMAN_PAGE_SIZE;
    uint32_t remainingBytes = Size;
    uint32_t pageOffset = Offset % FLASHMAN_PAGE_SIZE;
    while (remainingBytes > 0 && pageNumber < ((BlockNumber + 1) * (FLASHMAN_BLOCK_SIZE / FLASHMAN_PAGE_SIZE)))
    {
      uint32_t bytesToWrite = (remainingBytes > FLASHMAN_PAGE_SIZE) ? FLASHMAN_PAGE_SIZE : remainingBytes;
      if (FLASHMAN_WriteFn(Handle, pageNumber, Data + bytesWritten, bytesToWrite, pageOffset) == false)
      {
        retVal = false;
        break;
      }
      bytesWritten += bytesToWrite;
      remainingBytes -= bytesToWrite;
      pageNumber++;
      pageOffset = 0;
    }

  } while (0);

  FLASHMAN_UnLock(Handle);
  return retVal;
}

/**
  * @brief  Read From Address
  * @note   Read data from memory and copy to array
  *
  * @param  *Handle: Pointer to FLASHMAN_HandleTypeDef structure
  * @param  Address: Start Address
  * @param  *Data: Pointer to Data (output)
  * @param  Size: The length of data should be written. (in byte)
  *
  * @retval bool: true or false
  */
bool FLASHMAN_ReadAddress(FLASHMAN_HandleTypeDef *Handle, uint32_t Address, uint8_t *Data, uint32_t Size)
{
  FLASHMAN_Lock(Handle);
  bool retVal = false;
  retVal = FLASHMAN_ReadFn(Handle, Address, Data, Size);
  FLASHMAN_UnLock(Handle);
  return retVal;
}

/**
  * @brief  Read a Page
  * @note   Read a page and copy to array
  *
  * @param  *Handle: Pointer to FLASHMAN_HandleTypeDef structure
  * @param  PageNumber: Page Number
  * @param  *Data: Pointer to Data (output)
  * @param  Size: The length of data should be read. (in byte)
  * @param  Offset: The start point for Reading data. (in byte)
  *
  * @retval bool: true or false
  */
bool FLASHMAN_ReadPage(FLASHMAN_HandleTypeDef *Handle, uint32_t PageNumber, uint8_t *Data, uint32_t Size, uint32_t Offset)
{
  FLASHMAN_Lock(Handle);
  bool retVal = false;
  uint32_t address = FLASHMAN_PageToAddress(PageNumber);
  uint32_t maximum = FLASHMAN_PAGE_SIZE - Offset;
  if (Size > maximum)
  {
    Size = maximum;
  }
  retVal = FLASHMAN_ReadFn(Handle, address, Data, Size);
  FLASHMAN_UnLock(Handle);
  return retVal;
}

/**
  * @brief  Read a Sector
  * @note   Read a Sector and copy to array
  *
  * @param  *Handle: Pointer to FLASHMAN_HandleTypeDef structure
  * @param  SectorNumber: Sector Number
  * @param  *Data: Pointer to Data (output)
  * @param  Size: The length of data should be read. (in byte)
  * @param  Offset: The start point for Reading data. (in byte)
  *
  * @retval bool: true or false
  */
bool FLASHMAN_ReadSector(FLASHMAN_HandleTypeDef *Handle, uint32_t SectorNumber, uint8_t *Data, uint32_t Size, uint32_t Offset)
{
  FLASHMAN_Lock(Handle);
  bool retVal = false;
  uint32_t address = FLASHMAN_SectorToAddress(SectorNumber);
  uint32_t maximum = FLASHMAN_SECTOR_SIZE - Offset;
  if (Size > maximum)
  {
    Size = maximum;
  }
  retVal = FLASHMAN_ReadFn(Handle, address, Data, Size);
  FLASHMAN_UnLock(Handle);
  return retVal;
}

/**
  * @brief  Read a Block
  * @note   Read a Block and copy to array
  *
  * @param  *Handle: Pointer to FLASHMAN_HandleTypeDef structure
  * @param  BlockNumber: Block Number
  * @param  *Data: Pointer to Data (output)
  * @param  Size: The length of data should be read. (in byte)
  * @param  Offset: The start point for Reading data. (in byte)
  *
  * @retval bool: true or false
  */
bool FLASHMAN_ReadBlock(FLASHMAN_HandleTypeDef *Handle, uint32_t BlockNumber, uint8_t *Data, uint32_t Size, uint32_t Offset)
{
  FLASHMAN_Lock(Handle);
  bool retVal = false;
  uint32_t address = FLASHMAN_BlockToAddress(BlockNumber);
  uint32_t maximum = FLASHMAN_BLOCK_SIZE - Offset;
  if (Size > maximum)
  {
    Size = maximum;
  }
  retVal = FLASHMAN_ReadFn(Handle, address, Data, Size);
  FLASHMAN_UnLock(Handle);
  return retVal;
}
