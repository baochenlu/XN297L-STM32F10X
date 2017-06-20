#include "NRF24L01.h"
#include "spi.h"
#include "UART1.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "delay.h"
#include "config.h"
#include "CommUAV.h"

uint8_t TX_ADDRESS_DEF[5] = {0xC0,0xC0,0xC0,0xC0,0xC0};    		//RF 地址：接收端和发送端需一致
unsigned char ucCurrent_Channel;

int spi_sendZeroRecvByte(void)
{ 
	int recv = 0,i;
	MOSILOW;
	for (i = 7 ; i >=0 ; i--)
	{
		recv = recv<<1;
		
		SCKHIGH;
		
		if ( READMISO ) recv= recv|1;

		SCKLOW;
		
	}	
    return recv;
}

void testXN(void)
{
	uint8_t status;
	u8 buf[5]={0xC2,0xC2,0xC2,0xC2,0xC2}; 
	u8 buf1[5];
	RF_WriteBuf(W_REGISTER+TX_ADDR, buf, 5); 
	RF_ReadBuf(TX_ADDR, buf1, 5); 
	printf("status:%d\n",buf1[0]);
}

/******************************************************************************/
//            SPI_RW
//                SPI Write/Read Data
//            SPI写入一个BYTE的同时，读出一个BYTE返回
/******************************************************************************/
//u8 SPI_RW(u8 dat) 
//{ 
//    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET); 
//    SPI_I2S_SendData(SPI1, dat); 
//    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);  
//    return SPI_I2S_ReceiveData(SPI1); 
//	//return 1;
//}
unsigned char SPI_RW(uint8_t R_REG)
{
    uint8_t	  i;
    for(i = 0; i < 8; i++)
    {
        SCKLOW;
        if(R_REG & 0x80)
        {
            MOSIHIGH;
        }
        else
        {
            MOSILOW;
        }		
        R_REG = R_REG << 1;
        SCKHIGH;
        if(READMISO==1)
        {
          R_REG = R_REG | 0x01;
        }
    }
    SCKLOW;
    return R_REG;
}
/******************************************************************************/
//            RF_WriteReg
//                Write Data(1 Byte Address ,1 byte data)
/******************************************************************************/
void RF_WriteReg( uint8_t reg,  uint8_t wdata)
{
    SPI_CSN_L(); 
    SPI_RW(reg);
    SPI_RW(wdata);
    SPI_CSN_H();
}


/******************************************************************************/
//            RF_ReadReg
//                Read Data(1 Byte Address ,1 byte data return)
/******************************************************************************/
 uint8_t ucRF_ReadReg( uint8_t reg)
{
     uint8_t tmp;
    
    SPI_CSN_L(); 
    SPI_RW(reg);
    tmp = SPI_RW(0);
    SPI_CSN_H();
    
    return tmp;
}

/******************************************************************************/
//            RF_WriteBuf
//                Write Buffer
/******************************************************************************/
void RF_WriteBuf( uint8_t reg, uint8_t *pBuf, uint8_t length)
{
     uint8_t j;
    SPI_CSN_L(); 
    j = 0;
    SPI_RW(reg);
    for(j = 0;j < length; j++)
    {
        SPI_RW(pBuf[j]);
    }
    j = 0;
    SPI_CSN_H();
}

/******************************************************************************/
//            RF_ReadBuf
//                Read Data(1 Byte Address ,length byte data read)
/******************************************************************************/
void RF_ReadBuf( uint8_t reg, unsigned char *pBuf,  uint8_t length)
{
    uint8_t byte_ctr;

    SPI_CSN_L();                    		                               			
    SPI_RW(reg);       		                                                		
    for(byte_ctr=0;byte_ctr<length;byte_ctr++)
    	pBuf[byte_ctr] = SPI_RW(0);                                                 		
    SPI_CSN_H();                                                                 		
}



/******************************************************************************/
//            RF_TxMode
//                Set RF into TX mode
/******************************************************************************/
void RF_TxMode(void)
{
    SPI_CE_L();
    RF_WriteReg(W_REGISTER + CONFIG,  0X8E);							// 将RF设置成TX模式
    delay_us(10);
}


/******************************************************************************/
//            RF_RxMode
//            将RF设置成RX模式，准备接收数据
/******************************************************************************/
void RF_RxMode(void)
{
    SPI_CE_L();
    RF_WriteReg(W_REGISTER + CONFIG,  0X8F );							// 将RF设置成RX模式
    SPI_CE_H(); 											// Set CE pin high 开始接收数据
    delay_ms(2);
}

/******************************************************************************/
//            RF_GetStatus
//            read RF IRQ status,3bits return
/******************************************************************************/
unsigned char ucRF_GetStatus(void)
{
   uint8_t state;
   state=ucRF_ReadReg(STATUS)&0x70;
   return state;								//读取RF的状态 
}

/******************************************************************************/
//            RF_ClearStatus
//                clear RF IRQ
/******************************************************************************/
void RF_ClearStatus(void)
{
    RF_WriteReg(W_REGISTER + STATUS,0x70);							//清除RF的IRQ标志 
}

/******************************************************************************/
//            RF_ClearFIFO
//                clear RF TX/RX FIFO
/******************************************************************************/
void RF_ClearFIFO(void)
{
    RF_WriteReg(FLUSH_TX, 0);			                                		//清除RF 的 TX FIFO		
    RF_WriteReg(FLUSH_RX, 0);                                                   		//清除RF 的 RX FIFO	
}

/******************************************************************************/
//            RF_SetChannel
//                Set RF TX/RX channel:Channel
/******************************************************************************/
void RF_SetChannel( uint8_t Channel)
{    
    SPI_CE_L();
	ucCurrent_Channel = Channel;
    RF_WriteReg(W_REGISTER + RF_CH, Channel);
}

/******************************************************************************/
//            发送数据：
//            参数：
//              1. ucPayload：需要发送的数据首地址
//              2. length:  需要发送的数据长度
//              Return:
//              1. MAX_RT: TX Failure  (Enhance mode)
//              2. TX_DS:  TX Successful (Enhance mode)
//              note: Only use in Tx Mode
//              length 通常等于 PAYLOAD_WIDTH
/******************************************************************************/

void ucRF_TxData( uint8_t *ucPayload,  uint8_t length)
{
    /*if(0==ucRF_GetStatus())                                                                        // rf free status                                                                                                                                                                   
   {
    RF_WriteBuf(W_TX_PAYLOAD, ucPayload, length); 
    CE=1;                                                                    		//rf entery tx mode start send data 
    Delay20us();                                                              		//keep ce high at least 600us
    CE=0;                                                                                     //rf entery stb3                                                        			
   } */
   uint8_t   Status_Temp;
//    printf("state:%d\n",ucRF_GetStatus());
//    SPI_CE_H();                                                                    		//rf entery tx mode start send data 
//    delay_us(20);                                                             		//keep ce high at least 16us
//    SPI_CE_L();
	RF_WriteBuf(W_TX_PAYLOAD, ucPayload, length);                               		//write data to txfifo        
//	SPI_CE_H();
	                                                                     		//rf entery tx mode start send data 
//    delay_us(20);                                                             		//keep ce high at least 16us
//    SPI_CE_L();
			
//	printf("state:%d\n",ucRF_ReadReg(STATUS));			                                                                   		//rf entery stb3
    while(IRQ_STATUS);
	printf("state:%d\n",ucRF_ReadReg(STATUS));
	
    Status_Temp = ucRF_ReadReg(STATUS) & 0x70;                                                  //读取发送完成后的status
    RF_WriteReg(W_REGISTER + STATUS, Status_Temp);                                 		//清除Status
    RF_WriteReg(FLUSH_TX,0); 
	/* if(ucRF_ReadReg(OBSERVE_TX) > 0xCF)						
    {
	RF_SetChannel(ucCurrent_Channel);					//清0 OBSERVE_TX
    } */                                                  			//清 FIFO
   
    //return Status_Temp;
   
   	
}	


/******************************************************************************/
//            ucRF_DumpRxData
//            读出接收到的数据：
//            参数：
//              1. ucPayload：存储读取到的数据的Buffer
//              2. length:    读取的数据长度
//              Return:
//              1. 0: 没有接收到数据
//              2. 1: 读取接收到的数据成功
//              note: Only use in Rx Mode
//              length 通常等于 PAYLOAD_WIDTH
/******************************************************************************/
unsigned char ucRF_DumpRxData( uint8_t *ucPayload,  uint8_t length)
{
	
	//printf("state:%d\n",ucRF_ReadReg(STATUS));
	//printf("DATAOUT:%d\n",ucRF_ReadReg(RPD));
    if(IRQ_STATUS==1)
    {
      return 0;                                                                 		//若IRQ PIN为高，则没有接收到数据
    }
	printf("aa:%d\n",IRQ_STATUS);
    RF_ReadBuf(R_RX_PAYLOAD, ucPayload, length);                                		//将接收到的数据读出到ucPayload，且清除rxfifo
    RF_WriteReg(FLUSH_RX, 0);	
    RF_WriteReg(W_REGISTER + STATUS, 0x70);                                     		//清除Status
    //SPI_CE_H();                                                                    		//继续开始接收
    
    return 1;
}


////////////////////////////////////////////////////////////////////////////////

//          以下部分与RF通信相关，不建议修改
////////////////////////////////////////////////////////////////////////////////
/******************************************************************************/
//            PN006_Initial
//                Initial RF
/******************************************************************************/
void RF_Init(void)
{
    uint8_t  BB_cal_data[]    = { 0x0a,0x6d,0x67,0x8c,0x46}; 
    uint8_t  RF_cal_data[]    = {0xf6,0x37,0x5d};
    uint8_t  RF_cal2_data[]   = {0x45,0x21,0xef,0x2c,0x5a,0x40};
    uint8_t  Dem_cal_data[]   = {0x01};   
    uint8_t  Dem_cal2_data[]  = {0x0b,0xdf,0x02};   

	
    SPI_CE_L();
	//SPI_CSN_H();
	//SCKLOW;
    ucCurrent_Channel = DEFAULT_CHANNEL;                
	
    RF_WriteReg(RST_FSPI, 0x5A);								//Software Reset    			
    RF_WriteReg(RST_FSPI, 0XA5);
//#ifdef TX_XN    
	
    RF_WriteReg(FLUSH_TX, 0);									// CLEAR TXFIFO		
	RF_WriteBuf(W_REGISTER + TX_ADDR,TX_ADDRESS_DEF, 5);	// Writes TX_Address to PN006	
//#else
    RF_WriteReg(FLUSH_RX, 0);									// CLEAR  RXFIFO
	RF_WriteReg(W_REGISTER + EN_RXADDR, 0x01);							// Enable Pipe0
	RF_WriteReg(W_REGISTER + RX_PW_P0,  PAYLOAD_WIDTH);						// 8 bytes
	RF_WriteBuf(W_REGISTER + RX_ADDR_P0,TX_ADDRESS_DEF,5);	// RX_Addr0 same as TX_Adr for Auto.Ack   
//#endif
    RF_WriteReg(W_REGISTER + STATUS, 0x70);							// CLEAR  STATUS	
	RF_WriteBuf(W_REGISTER + BB_CAL,    BB_cal_data,  sizeof(BB_cal_data));
    RF_WriteBuf(W_REGISTER + RF_CAL2,   RF_cal2_data, sizeof(RF_cal2_data));
    RF_WriteBuf(W_REGISTER + DEM_CAL,   Dem_cal_data, sizeof(Dem_cal_data));
    RF_WriteBuf(W_REGISTER + RF_CAL,    RF_cal_data,  sizeof(RF_cal_data));
    RF_WriteBuf(W_REGISTER + DEM_CAL2,  Dem_cal2_data,sizeof(Dem_cal2_data));
    RF_WriteReg(W_REGISTER + SETUP_AW,  0x03);							// address witdth is 5 bytes
    RF_WriteReg(W_REGISTER + RF_CH,     0);                                       // 2478M HZ
        
    RF_WriteReg(W_REGISTER + DYNPD, 0x00);					
    RF_WriteReg(W_REGISTER + FEATURE, 0x00);
    RF_WriteReg(W_REGISTER + RF_SETUP,  RF_POWER);						//DBM  		
  
    
//#if(TRANSMIT_TYPE == TRANS_ENHANCE_MODE)      
//    RF_WriteReg(W_REGISTER + SETUP_RETR,0x03);							//  3 retrans... 	
//    RF_WriteReg(W_REGISTER + EN_AA,     0x01);							// Enable Auto.Ack:Pipe0  
//#elif(TRANSMIT_TYPE == TRANS_BURST_MODE)                                                                
    RF_WriteReg(W_REGISTER + SETUP_RETR,0x00);							// Disable retrans... 	
    RF_WriteReg(W_REGISTER + EN_AA,     0x00);							// Disable AutoAck 
//#endif

//if(PAYLOAD_WIDTH <33)											
//{
//	RF_WriteReg(W_REGISTER +FEATURE, 0x00);							//切换到32byte模式
//}
//else
//{
//	RF_WriteReg(W_REGISTER +FEATURE, 0x18);							//切换到64byte模式	   
//}

}


/******************************************************************************/
//            		进入载波模式
/******************************************************************************/
void RF_Carrier( uint8_t ucChannel_Set)
{
    uint8_t BB_cal_data[]    = {0x0A,0x6D,0x67,0x9C,0x46}; 
    uint8_t RF_cal_data[]    = {0xF6,0x37,0x5D};
    uint8_t RF_cal2_data[]   = {0x45,0x21,0xEF,0xAC,0x5A,0x50};
    uint8_t Dem_cal_data[]   = {0xE1}; 								
    uint8_t Dem_cal2_data[]  = {0x0B,0xDF,0x02};      
    
    SPI_CE_L();
    RF_WriteReg(RST_FSPI, 0x5A);								//Software Reset    			
    RF_WriteReg(RST_FSPI, 0XA5);
    delay_ms(200);
    RF_WriteReg(W_REGISTER + RF_CH, ucChannel_Set);						//单载波频点	   
    RF_WriteReg(W_REGISTER + RF_SETUP, RF_POWER);      						//dbm
    RF_WriteBuf(W_REGISTER + BB_CAL,    BB_cal_data,  sizeof(BB_cal_data));
    RF_WriteBuf(W_REGISTER + RF_CAL2,   RF_cal2_data, sizeof(RF_cal2_data));
    RF_WriteBuf(W_REGISTER + DEM_CAL,   Dem_cal_data, sizeof(Dem_cal_data));
    RF_WriteBuf(W_REGISTER + RF_CAL,    RF_cal_data,  sizeof(RF_cal_data));
    RF_WriteBuf(W_REGISTER + DEM_CAL2,  Dem_cal2_data,sizeof(Dem_cal2_data));
    delay_ms(5);	
}

/***************************************end of file ************************************/

