#include "config.h"

static void loop(void);

int main(void)
{
    /* Init */
    SystemClock_HSI(9);           //init system clock
    cycleCounterInit();				    // Init cycle counter
    SysTick_Config(SystemCoreClock / 1000);	//open sysTick, 1ms
    UART1_init(115200); //init uart1
    NVIC_INIT();	                //init interrupt
    LedInit();	                  //init led_GPIO
    //TIM4_Init(SysClock,TIME4_Preiod);	  //init Timer4
	SPI1_INIT();
	RF_Init();
#ifdef TX_XN
	RF_TxMode();
#else
	RF_RxMode();
#endif
	SPI_CE_H();
    /* Loop */
    loop();
}
uint8_t rxData[8] = {0,51,52};
void loop(void)
{			
    while (1)
    {        
#ifdef TX_XN
		ucRF_TxData(rxData,8);
#else	
		ucRF_DumpRxData(rxData, 8);
#endif	
        //delay_ms(5);
	}
}

