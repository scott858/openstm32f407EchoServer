/**
  ******************************************************************************
  * File Name          : ethernetif.c
  * Description        : This file provides code for the configuration
  *                      of the ethernetif.c MiddleWare.
  ******************************************************************************
  * COPYRIGHT(c) 2015 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "lwip/opt.h"

#include "lwip/lwip_timers.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include <string.h>
#include "cmsis_os.h"

/* Within 'USER CODE' section, code will be kept by default at each generation */
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/* Private define ------------------------------------------------------------*/
#include "stm32_eth.h"
/* The time to block waiting for input. */
#define TIME_WAITING_FOR_INPUT ( 100 )
/* Stack size of the interface thread */
#define INTERFACE_THREAD_STACK_SIZE ( 350 )

/* Network interface name */
#define IFNAME0 's'
#define IFNAME1 't'

/* USER CODE BEGIN 1 */
//#define LWIP_PTP 1

/* USER CODE END 1 */

/* Private variables ---------------------------------------------------------*/
#if defined ( __ICCARM__ ) /*!< IAR Compiler */
  #pragma data_alignment=4   
#endif
__ALIGN_BEGIN ETH_DMADescTypeDef  DMARxDscrTab[ETH_RXBUFNB] __ALIGN_END;/* Ethernet Rx MA Descriptor */

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
  #pragma data_alignment=4   
#endif
__ALIGN_BEGIN ETH_DMADescTypeDef  DMATxDscrTab[ETH_TXBUFNB] __ALIGN_END;/* Ethernet Tx DMA Descriptor */

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
  #pragma data_alignment=4   
#endif
__ALIGN_BEGIN uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE] __ALIGN_END; /* Ethernet Receive Buffer */

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
  #pragma data_alignment=4   
#endif
__ALIGN_BEGIN uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE] __ALIGN_END; /* Ethernet Transmit Buffer */

/* USER CODE BEGIN 2 */
//#if LWIP_PTP
//ETH_DMADESCTypeDef  DMAPTPRxDscrTab[ETH_RXBUFNB], DMAPTPTxDscrTab[ETH_TXBUFNB];/* Ethernet Rx & Tx PTP Helper Descriptors */
//extern __IO ETH_DMADESCTypeDef  *DMAPTPTxDescToSet;
//extern __IO ETH_DMADESCTypeDef  *DMAPTPRxDescToGet;
static void ETH_PTPStart(ETH_HandleTypeDef * heth);
//#endif

u32_t ETH_PTPSubSecond2NanoSecond(u32_t SubSecondValue)
{
  uint64_t val = SubSecondValue * 1000000000ll;
  val >>=31;
  return val;
}

u32_t ETH_PTPNanoSecond2SubSecond(u32_t SubSecondValue)
{
  uint64_t val = SubSecondValue * 0x80000000ll;
  val /= 1000000000;
  return val;
}

/* USER CODE END 2 */

/* Semaphore to signal incoming packets */
osSemaphoreId s_xSemaphore = NULL;

/* Global Ethernet handle*/
ETH_HandleTypeDef heth;

/* USER CODE BEGIN 3 */

/* USER CODE END 3 */

/* Private functions ---------------------------------------------------------*/

void HAL_ETH_MspInit(ETH_HandleTypeDef* heth)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  if(heth->Instance==ETH)
  {
  /* USER CODE BEGIN ETH_MspInit 0 */

  /* USER CODE END ETH_MspInit 0 */
    /* Peripheral clock enable */
    __ETH_CLK_ENABLE();
  
    /**ETH GPIO Configuration    
    PC1     ------> ETH_MDC
    PA1     ------> ETH_REF_CLK
    PA2     ------> ETH_MDIO
    PA7     ------> ETH_CRS_DV
    PC4     ------> ETH_RXD0
    PC5     ------> ETH_RXD1
    PB11     ------> ETH_TX_EN
    PB12     ------> ETH_TXD0
    PB13     ------> ETH_TXD1 
    */
    GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Peripheral interrupt init*/
    HAL_NVIC_SetPriority(ETH_IRQn, 5, 1);
    HAL_NVIC_EnableIRQ(ETH_IRQn);
  /* USER CODE BEGIN ETH_MspInit 1 */

  /* USER CODE END ETH_MspInit 1 */
  }
}

void HAL_ETH_MspDeInit(ETH_HandleTypeDef* heth)
{
  if(heth->Instance==ETH)
  {
  /* USER CODE BEGIN ETH_MspDeInit 0 */

  /* USER CODE END ETH_MspDeInit 0 */
    /* Peripheral clock disable */
    __ETH_CLK_DISABLE();
  
    /**ETH GPIO Configuration    
    PC1     ------> ETH_MDC
    PA1     ------> ETH_REF_CLK
    PA2     ------> ETH_MDIO
    PA7     ------> ETH_CRS_DV
    PC4     ------> ETH_RXD0
    PC5     ------> ETH_RXD1
    PB11     ------> ETH_TX_EN
    PB12     ------> ETH_TXD0
    PB13     ------> ETH_TXD1 
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5);

    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_7);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13);

    /* Peripheral interrupt Deinit*/
    HAL_NVIC_DisableIRQ(ETH_IRQn);

  /* USER CODE BEGIN ETH_MspDeInit 1 */

  /* USER CODE END ETH_MspDeInit 1 */
  }
}

/**
  * @brief  Ethernet Rx Transfer completed callback
  * @param  heth: ETH handle
  * @retval None
  */
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
  osSemaphoreRelease(s_xSemaphore);
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/*******************************************************************************
                       LL Driver Interface ( LwIP stack --> ETH) 
*******************************************************************************/
/**
 * In this function, the hardware should be initialized.
 * Called from ethernetif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void low_level_init(struct netif *netif)
{ 
  uint32_t regvalue = 0;
  HAL_StatusTypeDef hal_eth_init_status;
  
/* Init ETH */
  uint8_t MACAddr[6];
  heth.Instance = ETH;
  heth.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;

  heth.Init.PhyAddress = 1;
  MACAddr[0] = 0x11;
  MACAddr[1] = 0x11;
  MACAddr[2] = 0x11;
  MACAddr[3] = 0x11;
  MACAddr[4] = 0x11;
  MACAddr[5] = 11;

//  heth.Init.PhyAddress = 2;
//  MACAddr[0] = 0x22;
//  MACAddr[1] = 0x22;
//  MACAddr[2] = 0x22;
//  MACAddr[3] = 0x22;
//  MACAddr[4] = 0x22;
//  MACAddr[5] = 22;
  heth.Init.MACAddr = &MACAddr[0];
  heth.Init.RxMode = ETH_RXINTERRUPT_MODE;
  heth.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
  heth.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;
  hal_eth_init_status = HAL_ETH_Init(&heth);

  if (hal_eth_init_status == HAL_OK)
  {
    /* Set netif link flag */  
    netif->flags |= NETIF_FLAG_LINK_UP | NETIF_FLAG_IGMP;
  }

//#if LWIP_PTP
  /* Initialize Tx Descriptors list: Chain Mode */
  HAL_ETH_DMATxDescListInit(&heth, DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB);
  /* Initialize Rx Descriptors list: Chain Mode  */
  HAL_ETH_DMARxDescListInit(&heth, DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);
//#else
//  /* Initialize Tx Descriptors list: Chain Mode */
//  HAL_ETH_DMATxDescListInit(&heth, DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB);
//
//  /* Initialize Rx Descriptors list: Chain Mode  */
//  HAL_ETH_DMARxDescListInit(&heth, DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);
//#endif

#if LWIP_ARP || LWIP_ETHERNET 
  /* set MAC hardware address length */
  netif->hwaddr_len = ETHARP_HWADDR_LEN;
  
  /* set MAC hardware address */
  netif->hwaddr[0] =  heth.Init.MACAddr[0];
  netif->hwaddr[1] =  heth.Init.MACAddr[1];
  netif->hwaddr[2] =  heth.Init.MACAddr[2];
  netif->hwaddr[3] =  heth.Init.MACAddr[3];
  netif->hwaddr[4] =  heth.Init.MACAddr[4];
  netif->hwaddr[5] =  heth.Init.MACAddr[5];
  
  /* maximum transfer unit */
  netif->mtu = 1500;
  
  /* Accept broadcast address and ARP traffic */
  /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
  #if LWIP_ARP
    netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
  #else 
    netif->flags |= NETIF_FLAG_BROADCAST;
  #endif /* LWIP_ARP */
  
/* create a binary semaphore used for informing ethernetif of frame reception */
  osSemaphoreDef(SEM);
  s_xSemaphore = osSemaphoreCreate(osSemaphore(SEM) , 1 );

/* create the task that handles the ETH_MAC */
  osThreadDef(EthIf, ethernetif_input, osPriorityRealtime, 0, INTERFACE_THREAD_STACK_SIZE);
  osThreadCreate (osThread(EthIf), netif);
 
//#if LWIP_PTP
  /* Enable PTP Timestamping */
  ETH_PTPStart(&heth);
  /* ETH_PTPStart(ETH_PTP_CoarseUpdate); */
//#endif

  /* Enable MAC and DMA transmission and reception */
  HAL_ETH_Start(&heth);
  
  /**** Configure PHY to generate an interrupt when Eth Link state changes ****/
  /* Read Register Configuration */
  HAL_ETH_ReadPHYRegister(&heth, PHY_MICR, &regvalue);
  
  regvalue |= (PHY_MICR_INT_EN | PHY_MICR_INT_OE);

  /* Enable Interrupts */
  HAL_ETH_WritePHYRegister(&heth, PHY_MICR, regvalue );
  
  /* Read Register Configuration */
  HAL_ETH_ReadPHYRegister(&heth, PHY_MISR, &regvalue);
  
  regvalue |= PHY_MISR_LINK_INT_EN;
    
  /* Enable Interrupt on change of link status */
  HAL_ETH_WritePHYRegister(&heth, PHY_MISR, regvalue);   
#endif /* LWIP_ARP || LWIP_ETHERNET */
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
 *       strange results. You might consider waiting for space in the DMA queue
 *       to become availale since the stack doesn't retry to send a packet
 *       dropped because of memory failure (except for the TCP timers).
 */

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  err_t errval;
  struct pbuf *q;
  uint8_t *buffer = (uint8_t *)(heth.TxDesc->Buffer1Addr);
  __IO ETH_DMADescTypeDef *DmaTxDesc;
  uint32_t framelength = 0;
  uint32_t bufferoffset = 0;
  uint32_t byteslefttocopy = 0;
  uint32_t payloadoffset = 0;
  DmaTxDesc = heth.TxDesc;
  bufferoffset = 0;

//#if LWIP_PTP
//  struct ptptime_t timestamp;
//  buffer = ETH_PTPTxPkt_PrepareBuffer();
//#else
//  buffer = ETH_TxPkt_PrepareBuffer();
//#endif
  
  /* copy frame from pbufs to driver buffers */
  for(q = p; q != NULL; q = q->next)
    {
      /* Is this buffer available? If not, goto error */
      if((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET)
      {
        errval = ERR_USE;
        goto error;
      }
    
      /* Get bytes in current lwIP buffer */
      byteslefttocopy = q->len;
      payloadoffset = 0;
    
      /* Check if the length of data to copy is bigger than Tx buffer size*/
      while( (byteslefttocopy + bufferoffset) > ETH_TX_BUF_SIZE )
      {
        /* Copy data to Tx buffer*/
        memcpy( (uint8_t*)((uint8_t*)buffer + bufferoffset), (uint8_t*)((uint8_t*)q->payload + payloadoffset), (ETH_TX_BUF_SIZE - bufferoffset) );
      
        /* Point to next descriptor */
        DmaTxDesc = (ETH_DMADescTypeDef *)(DmaTxDesc->Buffer2NextDescAddr);
      
        /* Check if the buffer is available */
        if((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET)
        {
          errval = ERR_USE;
          goto error;
        }
      
        buffer = (uint8_t *)(DmaTxDesc->Buffer1Addr);
      
        byteslefttocopy = byteslefttocopy - (ETH_TX_BUF_SIZE - bufferoffset);
        payloadoffset = payloadoffset + (ETH_TX_BUF_SIZE - bufferoffset);
        framelength = framelength + (ETH_TX_BUF_SIZE - bufferoffset);
        bufferoffset = 0;
      }
    
      /* Copy the remaining bytes */
      memcpy( (uint8_t*)((uint8_t*)buffer + bufferoffset), (uint8_t*)((uint8_t*)q->payload + payloadoffset), byteslefttocopy );
      bufferoffset = bufferoffset + byteslefttocopy;
      framelength = framelength + byteslefttocopy;
    }
  
  /*Enable PTP time stamps*/
  heth.TxDesc->Status |= ETH_DMATXDESC_TTSE;



  /* Prepare transmit descriptors to give to DMA */
  if(HAL_OK == HAL_ETH_TransmitFrame(&heth, framelength))
  {
//#if LWIP_PTP
    p->time_sec = (heth.TxDesc)->TimeStampHigh;
    p->time_nsec = ETH_PTPSubSecond2NanoSecond((heth.TxDesc)->TimeStampLow);
//  if( ETH_SUCCESS == ETH_PTPTxPkt_ChainMode(l, &timestamp) ) {
//    p->time_sec = timestamp.tv_sec;
//    p->time_nsec = timestamp.tv_nsec;
//  } else {
//    return ERR_IF;
//  }
  };


  
  errval = ERR_OK;
  
error:
  
  /* When Transmit Underflow flag is set, clear it and issue a Transmit Poll Demand to resume transmission */
  if ((heth.Instance->DMASR & ETH_DMASR_TUS) != (uint32_t)RESET)
  {
    /* Clear TUS ETHERNET DMA flag */
    heth.Instance->DMASR = ETH_DMASR_TUS;

    /* Resume DMA transmission*/
    heth.Instance->DMATPDR = 0;
  }
  return errval;
}

/**
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return a pbuf filled with the received packet (including MAC header)
 *         NULL on memory error
   */
static struct pbuf * low_level_input(struct netif *netif)
{
  struct pbuf *p = NULL;
  struct pbuf *q;
  uint16_t len = 0;
  uint8_t *buffer;
  __IO ETH_DMADescTypeDef *dmarxdesc;
  uint32_t bufferoffset = 0;
  uint32_t payloadoffset = 0;
  uint32_t byteslefttocopy = 0;
  uint32_t i=0;
  
//#if LWIP_PTP
//  struct ptptime_t timestamp;
//#endif

//#if LWIP_PTP
//  frame.PTPdescriptor = NULL;
//  ETH_PTPRxPkt_ChainMode(&frame);
//#else
//  ETH_RxPkt_ChainMode(&frame);
//#endif

  /* get received frame */
  if (HAL_ETH_GetReceivedFrame_IT(&heth) != HAL_OK)
    return NULL;
  
  /* Obtain the size of the packet and put it into the "len" variable. */
  len = heth.RxFrameInfos.length;
  buffer = (uint8_t *)heth.RxFrameInfos.buffer;
  
  if (len > 0)
  {
    /* We allocate a pbuf chain of pbufs from the Lwip buffer pool */
    p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
  }
  
  if (p != NULL)
  {
    dmarxdesc = heth.RxFrameInfos.FSRxDesc;
    bufferoffset = 0;
    for(q = p; q != NULL; q = q->next)
    {
      byteslefttocopy = q->len;
      payloadoffset = 0;
      
      /* Check if the length of bytes to copy in current pbuf is bigger than Rx buffer size*/
      while( (byteslefttocopy + bufferoffset) > ETH_RX_BUF_SIZE )
      {
        /* Copy data to pbuf */
        memcpy( (uint8_t*)((uint8_t*)q->payload + payloadoffset), (uint8_t*)((uint8_t*)buffer + bufferoffset), (ETH_RX_BUF_SIZE - bufferoffset));
        
        /* Point to next descriptor */
        dmarxdesc = (ETH_DMADescTypeDef *)(dmarxdesc->Buffer2NextDescAddr);
        buffer = (uint8_t *)(dmarxdesc->Buffer1Addr);
        
        byteslefttocopy = byteslefttocopy - (ETH_RX_BUF_SIZE - bufferoffset);
        payloadoffset = payloadoffset + (ETH_RX_BUF_SIZE - bufferoffset);
        bufferoffset = 0;
      }
      /* Copy remaining data in pbuf */
      memcpy( (uint8_t*)((uint8_t*)q->payload + payloadoffset), (uint8_t*)((uint8_t*)buffer + bufferoffset), byteslefttocopy);
      bufferoffset = bufferoffset + byteslefttocopy;
    }
    
    /* Release descriptors to DMA */
    /* Point to first descriptor */
    dmarxdesc = heth.RxFrameInfos.FSRxDesc;
    /* Set Own bit in Rx descriptors: gives the buffers back to DMA */
    for (i=0; i< heth.RxFrameInfos.SegCount; i++)
    {  
      dmarxdesc->Status |= ETH_DMARXDESC_OWN;
      dmarxdesc = (ETH_DMADescTypeDef *)(dmarxdesc->Buffer2NextDescAddr);
    }
    
    /* Clear Segment_Count */
    heth.RxFrameInfos.SegCount =0;
  }    
  
  /* When Rx Buffer unavailable flag is set: clear it and resume reception */
  if ((heth.Instance->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET)  
  {
    /* Clear RBUS ETHERNET DMA flag */
    heth.Instance->DMASR = ETH_DMASR_RBUS;
    /* Resume DMA reception */
    heth.Instance->DMARPDR = 0;
  }

//#if LWIP_PTP
//  ETH_PTPRxPkt_ChainMode_CleanUp(&frame, &timestamp);
//  if(p != NULL)
//  {
//	p->time_sec = timestamp.tv_sec;
//	p->time_nsec = timestamp.tv_nsec;
//  }
//#endif

//#if LWIP_PTP
  if(p !=NULL)
  {
    p->time_sec = (heth.RxDesc)->TimeStampHigh;
    p->time_nsec = ETH_PTPSubSecond2NanoSecond((heth.RxDesc)->TimeStampLow);
  }

  return p;
}

/**
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface. Then the type of the received packet is determined and
 * the appropriate input function is called.
 *
 * @param netif the lwip network interface structure for this ethernetif
 */
void ethernetif_input( void const * argument ) 
 
{
 
  struct pbuf *p;
  struct netif *netif = (struct netif *) argument;
  
  for( ;; )
  {
    if (osSemaphoreWait( s_xSemaphore, TIME_WAITING_FOR_INPUT)==osOK)
    {
      do
      {   
        p = low_level_input( netif );
        if   (p != NULL)
        {
          if (netif->input( p, netif) != ERR_OK )
          {
            pbuf_free(p);
          }
        }
      } while(p!=NULL);
    }
 
  }
}

#if !LWIP_ARP
/**
 * This function has to be completed by user in case of ARP OFF.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if ...
 */
static err_t low_level_output_arp_off(struct netif *netif, struct pbuf *q, ip_addr_t *ipaddr)
{  
  err_t errval;
  errval = ERR_OK;
    
/* USER CODE BEGIN 5 */ 
    
/* USER CODE END 5 */  
    
  return errval;
  
}
#endif /* LWIP_ARP */ 

/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t ethernetif_init(struct netif *netif)
{
  LWIP_ASSERT("netif != NULL", (netif != NULL));
  
#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
#if LWIP_ARP || LWIP_ETHERNET
#if LWIP_ARP
  netif->output = etharp_output;
#else
  /* The user should write ist own code in low_level_output_arp_off function */
  netif->output = low_level_output_arp_off;
#endif /* LWIP_ARP */ 
#endif  /* LWIP_ARP || LWIP_ETHERNET */
  netif->linkoutput = low_level_output;

  /* initialize the hardware */
  low_level_init(netif);

  return ERR_OK;
}

/* USER CODE BEGIN 6 */

/**
* @brief  Returns the current time in milliseconds
*         when LWIP_TIMERS == 1 and NO_SYS == 1
* @param  None
* @retval Time
*/
u32_t sys_jiffies(void)
{
  return HAL_GetTick();
}

/**
* @brief  Returns the current time in milliseconds
*         when LWIP_TIMERS == 1 and NO_SYS == 1
* @param  None
* @retval Time
*/
u32_t sys_now(void)
{
  return HAL_GetTick();
}

/* USER CODE END 6 */

/**
  * @brief  This function sets the netif link status.
  * @param  netif: the network interface
  * @retval None
  */
void ethernetif_set_link(void const *argument)
{
  uint32_t regvalue = 0;
  struct link_str *link_arg = (struct link_str *)argument;
  
  for(;;)
  {
    if (osSemaphoreWait( link_arg->semaphore, 100)== osOK)
    {
      /* Read PHY_MISR*/
      HAL_ETH_ReadPHYRegister(&heth, PHY_MISR, &regvalue);
      
      /* Check whether the link interrupt has occurred or not */
      if((regvalue & PHY_LINK_INTERRUPT) != (uint16_t)RESET)
      {
        /* Read PHY_SR*/
        HAL_ETH_ReadPHYRegister(&heth, PHY_SR, &regvalue);
        
        /* Check whether the link is up or down*/
        if((regvalue & PHY_LINK_STATUS)!= (uint16_t)RESET)
        {
          netif_set_link_up(link_arg->netif);
        }
        else
        {
          netif_set_link_down(link_arg->netif);
        }
      }
    }
  }
}

/* USER CODE BEGIN 7 */
//#if LWIP_PTP
static void TargetTime_Init(ETH_HandleTypeDef * heth)
{
/* - program target time */
//  ETH_SetPTPTargetTime(10,0);
	WRITE_REG(heth->Instance->PTPTTHR, 10);
	WRITE_REG(heth->Instance->PTPTTLR, 0);

/* - unmask timestamp interrupt (9) ETH_MACIMR */
//  ETH_MACITConfig(ETH_MAC_IT_TST, ENABLE);
	WRITE_REG(heth->Instance->MACIMR, heth->Instance->MACIMR & (~(uint32_t)ETH_MAC_IT_TST));
/* - set TSCR bit 4 */
//  ETH_EnablePTPTimeStampInterruptTrigger();
	SET_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSITE);
}


/*******************************************************************************
* Function Name  : ETH_PTPStart
* Description    : Initialize timestamping ability of ETH
* Input          : UpdateMethod:
*                       ETH_PTP_FineUpdate   : Fine Update method
*                       ETH_PTP_CoarseUpdate : Coarse Update method
* Output         : None
* Return         : None
*******************************************************************************/
static void ETH_PTPStart(ETH_HandleTypeDef * heth) {
  /* Check the parameters */
//  assert_param(IS_ETH_PTP_UPDATE(UpdateMethod));
	GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* Mask the Time stamp trigger interrupt by setting bit 9 in the MACIMR register. */
  SET_BIT((heth->Instance)->MACIMR, ETH_MACIMR_TSTIM);
//  ETH_MACITConfig(ETH_MAC_IT_TST, DISABLE);

  /* Program Time stamp register bit 0 to enable time stamping. */
  SET_BIT((heth->Instance)->PTPTSCR, ETH_PTPTSCR_TSE);
//  ETH_PTPTimeStampCmd(ENABLE);

  /* Program the Subsecond increment register based on the PTP clock frequency. */
  WRITE_REG((heth->Instance)->PTPSSIR, 22);
//  ETH_SetPTPSubSecondIncrement(ADJ_FREQ_BASE_INCREMENT); /* to achieve 20 ns accuracy, the value is ~ 43 */

  if (0) {

    /* If you are using the Fine correction method, program the Time stamp addend register
     * and set Time stamp control register bit 5 (addend register update). */
	WRITE_REG(heth->Instance->PTPTSAR, 0xF9E395EA);
	SET_BIT(heth->Instance->PTPTSAR, ETH_PTPTSCR_TSARU);
//    ETH_SetPTPTimeStampAddend(ADJ_FREQ_BASE_ADDEND);
//    ETH_EnablePTPTimeStampAddend();

    /* Poll the Time stamp control register until bit 5 is cleared. */
    while(READ_BIT(heth->Instance->PTPTSAR, ETH_PTPTSCR_TSARU));
//    while(ETH_GetPTPFlagStatus(ETH_PTP_FLAG_TSARU) == SET);

	/* To select the Fine correction method (if required),
	* program Time stamp control register  bit 1. */
	SET_BIT(heth->Instance->PTPTSAR, ETH_PTPTSCR_TSFCU);
	//  ETH_PTPUpdateMethodConfig(UpdateMethod);

  } else
  {
	/* To select the Coarse correction method (if required),
	* program Time stamp control register  bit 1. */
	CLEAR_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSFCU);
	//  ETH_PTPUpdateMethodConfig(UpdateMethod);
  }

  /* Program the Time stamp high update and Time stamp low update registers
   * with the appropriate time value. */
  WRITE_REG(heth->Instance->PTPTSLUR, 0);
  WRITE_REG(heth->Instance->PTPTSHUR, 0);
//  ETH_SetPTPTimeStampUpdate(ETH_PTP_PositiveTime, 0, 0);

  /* Set Time stamp control register bit 2 (Time stamp init). */
  while(READ_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSSTI));
  SET_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSSTI);
  while(READ_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSSTI));
//  ETH_InitializePTPTimeStamp();

  /* The Time stamp counter starts operation as soon as it is initialized
   * with the value written in the Time stamp update register. */

  /* Enable the MAC receiver and transmitter for proper time stamping. ETH_Start(); */
  //enhanced descriptors
  SET_BIT((heth->Instance)->DMABMR, ETH_DMABMR_EDE);

  //all received frames
  SET_BIT((heth->Instance)->PTPTSCR, ETH_PTPTSSR_TSSARFE);
//  SET_BIT((heth->Instance)->PTPTSCR, ETH_PTPTSSR_TSSPTPOEFE);
  //1588v2
  SET_BIT((heth->Instance)->PTPTSCR, ETH_PTPTSSR_TSPTPPSV2E);

  SET_BIT((heth->Instance)->MACFFR, ETH_MACFFR_PAM);

  SET_BIT((heth->Instance)->PTPTSCR, ETH_PTPTSSR_TSSIPV4FE);
  SET_BIT((heth->Instance)->MACFFR, ETH_MACFFR_RA);

  TargetTime_Init(heth);
}

//#endif /* LWIP_PTP */

/*******************************************************************************
* Function Name  : ETH_PTPTimeStampAdjFreq
* Description    : Updates time stamp addend register
* Input          : Correction value in thousandth of ppm (Adj*10^9)
* Output         : None
* Return         : None
*******************************************************************************/
void ETH_PTPTime_AdjFreq(ETH_HandleTypeDef * heth, int32_t Adj)
{
    uint32_t addend;

    /* calculate the rate by which you want to speed up or slow down the system time
       increments */

    /* precise */
    /*
    int64_t addend;
    addend = Adj;
    addend *= ADJ_FREQ_BASE_ADDEND;
    addend /= 1000000000-Adj;
    addend += ADJ_FREQ_BASE_ADDEND;
    */

    /* 32bit estimation
    ADJ_LIMIT = ((1l<<63)/275/ADJ_FREQ_BASE_ADDEND) = 11258181 = 11 258 ppm*/
    if( Adj > 5120000) Adj = 5120000;
    if( Adj < -5120000) Adj = -5120000;

    addend = ((((275LL * Adj)>>8) * (ADJ_FREQ_BASE_ADDEND>>24))>>6) + ADJ_FREQ_BASE_ADDEND;

    /* Reprogram the Time stamp addend register with new Rate value and set ETH_TPTSCR */
    WRITE_REG(heth->Instance->PTPTSAR, (uint32_t)addend);
    while(READ_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSARU));
    SET_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSARU);
    while(READ_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSARU));
}

/*******************************************************************************
* Function Name  : ETH_PTPTimeStampUpdateOffset
* Description    : Updates time base offset
* Input          : Time offset with sign
* Output         : None
* Return         : None
*******************************************************************************/
void ETH_PTPTime_UpdateOffset(ETH_HandleTypeDef * heth, struct ptptime_t * timeoffset)
{
    uint32_t Sign;
    uint32_t SecondValue;
    uint32_t NanoSecondValue;
    uint32_t SubSecondValue;
    uint32_t addend;

    /* determine sign and correct Second and Nanosecond values */
    if(timeoffset->tv_sec< 0 || (timeoffset->tv_sec == 0 && timeoffset->tv_nsec < 0)) {
        Sign = ETH_PTP_NegativeTime;
        SecondValue = -timeoffset->tv_sec;
        NanoSecondValue = -timeoffset->tv_nsec;
    } else {
        Sign = ETH_PTP_PositiveTime;
        SecondValue = timeoffset->tv_sec;
        NanoSecondValue = timeoffset->tv_nsec;
    }

    /* convert nanosecond to subseconds */
    SubSecondValue = ETH_PTPNanoSecond2SubSecond(NanoSecondValue);

    /* read old addend register value*/
    addend = READ_REG(heth->Instance->PTPTSAR);

    while(READ_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSSTU));
    while(READ_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSSTI));

    /* Write the offset (positive or negative) in the Time stamp update high and low registers. */
    WRITE_REG(heth->Instance->PTPTSHUR, SecondValue);
    WRITE_REG(heth->Instance->PTPTSLUR, Sign | SubSecondValue);
    /* Set bit 3 (TSSTU) in the Time stamp control register. */
    SET_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSSTU);
    /* The value in the Time stamp update registers is added to or subtracted from the system */
    /* time when the TSSTU bit is cleared. */
    while(READ_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSSTU));

    /* write back old addend register value */
	WRITE_REG(heth->Instance->PTPTSAR, addend);
	while(READ_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSARU));
	SET_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSARU);

    /* Poll the Time stamp control register until bit 5 is cleared. */
    while(READ_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSARU));
}

/*******************************************************************************
* Function Name  : ETH_PTPTimeStampSetTime
* Description    : Initialize time base
* Input          : Time with sign
* Output         : None
* Return         : None
*******************************************************************************/
void ETH_PTPTime_SetTime(ETH_HandleTypeDef * heth, struct ptptime_t * timestamp)
{
    uint32_t Sign;
    uint32_t SecondValue;
    uint32_t NanoSecondValue;
    uint32_t SubSecondValue;

    /* determine sign and correct Second and Nanosecond values */
    if(timestamp->tv_sec < 0 || (timestamp->tv_sec == 0 && timestamp->tv_nsec < 0)) {
        Sign = ETH_PTP_NegativeTime;
        SecondValue = -timestamp->tv_sec;
        NanoSecondValue = -timestamp->tv_nsec;
    } else {
        Sign = ETH_PTP_PositiveTime;
        SecondValue = timestamp->tv_sec;
        NanoSecondValue = timestamp->tv_nsec;
    }

    /* convert nanosecond to subseconds */
    SubSecondValue = ETH_PTPNanoSecond2SubSecond(NanoSecondValue);

    /* Write the offset (positive or negative) in the Time stamp update high and low registers. */
    WRITE_REG(heth->Instance->PTPTSHUR, SecondValue);
    WRITE_REG(heth->Instance->PTPTSLUR, Sign | SubSecondValue);
    /* Set Time stamp control register bit 2 (Time stamp init). */
    while(READ_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSSTI));
    SET_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSSTI);
    /* The Time stamp counter starts operation as soon as it is initialized
     * with the value written in the Time stamp update register. */
    while(READ_BIT(heth->Instance->PTPTSCR, ETH_PTPTSCR_TSSTI));
}


void ETH_PTPTime_GetTime(ETH_HandleTypeDef * heth, struct ptptime_t * timestamp) {
  timestamp->tv_nsec = ETH_PTPSubSecond2NanoSecond(READ_REG(heth->Instance->PTPTSLR));
  timestamp->tv_sec = READ_REG(heth->Instance->PTPTSHR);
}

/* USER CODE END 7 */

#if LWIP_NETIF_LINK_CALLBACK
/**
  * @brief  Link callback function, this function is called on change of link status
  *         to update low level driver configuration.
* @param  netif: The network interface
  * @retval None
  */
void ethernetif_update_config(struct netif *netif)
{
  __IO uint32_t timeout = 0;
  uint32_t regvalue = 0;
  
  if(netif_is_link_up(netif))
  { 
    /* Restart the auto-negotiation */
    if(heth.Init.AutoNegotiation != ETH_AUTONEGOTIATION_DISABLE)
    {
      /* Enable Auto-Negotiation */
      HAL_ETH_WritePHYRegister(&heth, PHY_BCR, PHY_AUTONEGOTIATION);
      
      /* Wait until the auto-negotiation will be completed */
      do
      {
        timeout++;
        HAL_ETH_ReadPHYRegister(&heth, PHY_BSR, &regvalue);
      } while (!(regvalue & PHY_AUTONEGO_COMPLETE) && (timeout < PHY_READ_TO));
      
      if(timeout == PHY_READ_TO)
      {      
        goto error;
      }
      
      /* Reset Timeout counter */
      timeout = 0;
      
      /* Read the result of the auto-negotiation */
      HAL_ETH_ReadPHYRegister(&heth, PHY_SR, &regvalue);
      
      /* Configure the MAC with the Duplex Mode fixed by the auto-negotiation process */
      if((regvalue & PHY_DUPLEX_STATUS) != (uint32_t)RESET)
      {
        /* Set Ethernet duplex mode to Full-duplex following the auto-negotiation */
        heth.Init.DuplexMode = ETH_MODE_FULLDUPLEX;  
      }
      else
      {
        /* Set Ethernet duplex mode to Half-duplex following the auto-negotiation */
        heth.Init.DuplexMode = ETH_MODE_HALFDUPLEX;           
      }
      /* Configure the MAC with the speed fixed by the auto-negotiation process */
      if(regvalue & PHY_SPEED_STATUS)
      {  
        /* Set Ethernet speed to 10M following the auto-negotiation */
        heth.Init.Speed = ETH_SPEED_10M; 
      }
      else
      {   
        /* Set Ethernet speed to 100M following the auto-negotiation */ 
        heth.Init.Speed = ETH_SPEED_100M;
      }
    }
    else /* AutoNegotiation Disable */
    {
    error :
      /* Check parameters */
      assert_param(IS_ETH_SPEED(heth.Init.Speed));
      assert_param(IS_ETH_DUPLEX_MODE(heth.Init.DuplexMode));
      
      /* Set MAC Speed and Duplex Mode to PHY */
      HAL_ETH_WritePHYRegister(&heth, PHY_BCR, ((uint16_t)(heth.Init.DuplexMode >> 3) |
                                                     (uint16_t)(heth.Init.Speed >> 1))); 
    }

    /* ETHERNET MAC Re-Configuration */
    HAL_ETH_ConfigMAC(&heth, (ETH_MACInitTypeDef *) NULL);

    /* Restart MAC interface */
    HAL_ETH_Start(&heth);   
  }
  else
  {
    /* Stop MAC interface */
    HAL_ETH_Stop(&heth);
  }

  ethernetif_notify_conn_changed(netif);
}

/* USER CODE BEGIN 8 */
/**
  * @brief  This function notify user about link status changement.
  * @param  netif: the network interface
  * @retval None
  */
__weak void ethernetif_notify_conn_changed(struct netif *netif)
{
  /* NOTE : This is function could be implemented in user file 
            when the callback is needed,
  */

}
/* USER CODE END 8 */ 
#endif /* LWIP_NETIF_LINK_CALLBACK */

/* USER CODE BEGIN 9 */

/* USER CODE END 9 */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

