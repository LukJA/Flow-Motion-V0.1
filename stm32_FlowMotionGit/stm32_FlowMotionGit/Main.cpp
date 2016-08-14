#include <stm32f4xx_hal.h>
#include "stm32fxxx_hal.h"
#include "defines.h"

#include "tm_stm32_fatfs.h"
#include "tm_stm32_delay.h"

//------------------------------------------------------------------------
//FlowMotion V0.1 firmware --
//To do=
//- self test on start-up
//- sd file system
//- sd MSC
//- Lis control
//------------------------------------------------------------------------

#ifdef __cplusplus
extern "C"
#endif

void LED_INIT(void);
TIM_HandleTypeDef PWMTimer; //global

void RTC_INIT(void);
uint8_t RTC_DateTime(void);
RTC_HandleTypeDef RTCconfig; // global
 
void LIS_INIT(void);
void LISCMD(uint8_t, uint8_t);
uint8_t LISREAD(uint8_t);
signed int lis_get_axis(int);
I2C_HandleTypeDef I2C_InitStructure; //global


bool RECORDING = false;

/* Fatfs structure */
FATFS FS;
FIL fil;
FRESULT fres;

/* Size structure for FATFS */
TM_FATFS_Size_t CardSize;
 
/* Buffer variable */
char buffer[100];


extern "C" void SysTick_Handler(void)
{
	HAL_IncTick();
	HAL_SYSTICK_IRQHandler();
}

int main(void)
{
	// Perform first time boot initiation of all hardware
	HAL_Init(); 
	LED_INIT();
	LIS_INIT();
	
	for (;;)
	{
		// enter low power stop mode - exit by any EXTI - disable systick to prevent exit
		SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; // disable systick
		HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI); //stop
		SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk; // enable systick
		
		
		// clear the LIS internal interrupts by reading their registers
		uint8_t INT1 = LISREAD(INT1_SRC);
		uint8_t CLICKS = LISREAD(CLICK_SRC);
		
	
		//double check recording is in a know state
		if (RECORDING)
		{
			
			// Blink the green Led to symbol recording start
			HAL_GPIO_WritePin(LED_PORT, G_LED_PIN, GPIO_PIN_SET);
			HAL_Delay(200);
			HAL_GPIO_WritePin(LED_PORT, G_LED_PIN, GPIO_PIN_RESET);
		
		
			// Start recording light through hardware pwm
			HAL_TIM_PWM_Start(&PWMTimer, TIM_CHANNEL_4);
		
		
			// Mount SD with hardware code "SD" in force mode
			if (f_mount(&FS, "SD:", 1) == FR_OK) {
				// Open File to write
				if ((fres = f_open(&fil, "SD:TELEMETRY2.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE)) == FR_OK) {
							
					while (RECORDING)
					{
						// get acceleration data
						uint16_t XA = lis_get_axis(X_AXIS);
						uint16_t YA = lis_get_axis(Y_AXIS);
						uint16_t ZA = lis_get_axis(Z_AXIS);
					
						// format and write to SD in CSV
						sprintf(buffer, " %u, %u, %u,\n", XA, YA, ZA);
						f_puts(buffer, &fil);
					
						// Sample rate
						HAL_Delay(25);
					}
					// Close the file
					f_close(&fil);
				}
				// Unmount the SD
				f_mount(NULL, "SD:", 0);
			}
		
			// stop hardware PWM recording light
			HAL_TIM_PWM_Stop(&PWMTimer, TIM_CHANNEL_4);
		
			// blink to specify end
			HAL_GPIO_WritePin(LED_PORT, G_LED_PIN, GPIO_PIN_SET);
			HAL_Delay(200);
			HAL_GPIO_WritePin(LED_PORT, G_LED_PIN, GPIO_PIN_RESET);
			
		} // end if(RECORDING)
	} // end for ;;
} // end main

void RTC_INIT(void)
{
	// Init the RTC hardware used in file timestamping and periodic system checks
	// default time and date (24H)
	RTC_TimeTypeDef time;
	RTC_DateTypeDef date;
	time.Hours = 12;
	time.Minutes = 0;
	time.Seconds = 0;
	time.SubSeconds = 0;
	time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	time.StoreOperation = RTC_STOREOPERATION_SET;
	
	date.Date = 1;
	date.Month = RTC_MONTH_JANUARY;
	date.Year = 00; //Y2K
	date.WeekDay = RTC_WEEKDAY_SATURDAY;
	
	// enable PWR interface
	__HAL_RCC_PWR_CLK_ENABLE();
	// allow access to the backup domain (RTC is write protected)
	HAL_PWR_EnableBkUpAccess();
	// config RTC clock source NOTE: change to Low Speed External LSE
	__HAL_RCC_RTC_CONFIG(RCC_RTCCLKSOURCE_LSI);
	// enable the RTC clock
	__HAL_RCC_RTC_ENABLE();
	
	// begin RTC initiation of clock dividers
	RTCconfig.Init.HourFormat = RTC_HOURFORMAT_24;
	RTCconfig.Init.AsynchPrediv = 124; // change to 127 for LSE
	RTCconfig.Init.SynchPrediv = 295; // change to 255 for LSE
	// insert into RTC init
	HAL_RTC_Init(&RTCconfig);
	
	// Set RTC default values
	HAL_RTC_SetTime(&RTCconfig, &time, RTC_FORMAT_BCD);
	HAL_RTC_SetDate(&RTCconfig, &date, RTC_FORMAT_BCD);
}

void LED_INIT(void)
{
	//** Initializes GPIO for Green LED
	__GPIOA_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.Pin = G_LED_PIN;

	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(LED_PORT, &GPIO_InitStructure);
	
		
	//** Initializes PWM for (blue) red LED
	// Init Timer 2
	__TIM2_CLK_ENABLE();
	
	PWMTimer.Init.Period = 500;
	PWMTimer.Init.Prescaler = 40000;
	PWMTimer.Init.CounterMode = TIM_COUNTERMODE_UP;
	PWMTimer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV2;
	PWMTimer.Instance = TIM2;

	HAL_TIM_PWM_Init(&PWMTimer);
	
	// init compare registers for pwm
	TIM_OC_InitTypeDef sConfig;
	
	sConfig.OCMode     = TIM_OCMODE_PWM1;
	sConfig.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfig.OCFastMode = TIM_OCFAST_DISABLE;
	sConfig.Pulse = 100;
	
	HAL_TIM_PWM_ConfigChannel(&PWMTimer, &sConfig, TIM_CHANNEL_4);
	__HAL_TIM_ENABLE(&PWMTimer);
	
	// init GPIO for pwm
	__GPIOA_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_Init;

	GPIO_Init.Pin = B_LED_PIN;
	GPIO_Init.Mode = GPIO_MODE_AF_PP;
	GPIO_Init.Speed = GPIO_SPEED_HIGH;
	GPIO_Init.Pull = GPIO_NOPULL;
	GPIO_Init.Alternate = GPIO_AF1_TIM2;
	GPIOA->AFR[0] |= 0x00001000;
	
	HAL_GPIO_Init(LED_PORT, &GPIO_Init);
}

void LIS_INIT(void)
{
	__I2C1_CLK_ENABLE();
	__HAL_RCC_I2C1_FORCE_RESET();
	__HAL_RCC_I2C1_RELEASE_RESET();
	
	__GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	
	//setup i2c
	GPIO_InitTypeDef GPIO_InitStructureI2C;
	//set up sda,scl and interupt
	GPIO_InitStructureI2C.Pin = GPIO_PIN_8 | GPIO_PIN_9;
	GPIO_InitStructureI2C.Alternate = GPIO_AF4_I2C1;
	GPIO_InitStructureI2C.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructureI2C.Pull = GPIO_PULLUP;
	GPIO_InitStructureI2C.Mode = GPIO_MODE_AF_OD;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructureI2C);
	
	//TD interupt--
	//i2c:
	I2C_InitStructure.Init.ClockSpeed = 400000;
	I2C_InitStructure.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	I2C_InitStructure.Init.DutyCycle = I2C_DUTYCYCLE_2;
	I2C_InitStructure.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	I2C_InitStructure.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	I2C_InitStructure.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	
	I2C_InitStructure.Instance = I2C1;
	
	HAL_I2C_Init(&I2C_InitStructure);
	//lets see if the lis is online
	HAL_StatusTypeDef x = HAL_I2C_IsDeviceReady(&I2C_InitStructure, 0B00110000, 3, 10);
	
	
	//cool lets set it up --------------------------------
	
	// enable all axes, normal mode 1.25KHz rate
	// ~ 10010111 
	LISCMD(CTRL_REG1, 0x97);
	
	// Turn on INT1 click
	// ~ 10000000
	LISCMD(CTRL_REG3, 0x80);
	
	// High res, 4G+- & BDU enabled
	// ~ 10011000
	LISCMD(CTRL_REG4, 0x98);
	
	// latch interupt on INT1 (Cleared by reading INT1SRC)
	// ~ 00001000
	LISCMD(CTRL_REG5, 0x08);
	
	// turn on all axes & doubletap detection
	// ~ 00101010
	LISCMD(CLICK_CFG, 0x2A); 
	
	// set parameters for detection ----
	
	// Adjust this number for the sensitivity of the 'click' force
	// this strongly depend on the range! for 16G, try 5-10
	// for 8G, try 10-20. for 4G try 20-40. for 2G try 40-80
	#define CLICKTHRESHHOLD 80
	// timing params in units of ODR sample rate
	#define TIMELIMIT 10
	#define TIMELATENCY 20
	#define TIMEWINDOW 255
	
	LISCMD(CLICK_THS, CLICKTHRESHHOLD); // arbitrary
	LISCMD(TIME_LIMIT, TIMELIMIT); // arbitrary
	LISCMD(TIME_LATENCY, TIMELATENCY); // arbitrary
	LISCMD(TIME_WINDOW, TIMEWINDOW); // arbitrary
	
	// -------------
	
	// GPIO setup for INT1 input: (PB7)
	__GPIOB_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.Pin = GPIO_PIN_7;

	GPIO_InitStructure.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 1);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
	
	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7);
	
	// -------------------------------------------------------------
	
}

extern "C" void EXTI9_5_IRQHandler()
{
	// handler for tap - inverts recording bool
	HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7);
	uint8_t x = 0xF;
	RECORDING = !RECORDING;
}

void LISCMD(uint8_t registerAddr, uint8_t cmmd)
{
	// send a register write to the LIS
	uint8_t toSend[2] = { registerAddr, cmmd }; //create command data
	HAL_I2C_Master_Transmit(&I2C_InitStructure, LIS3DH_ADDR << 1, &toSend[0], 2, 10); //send to LIS
}

uint8_t LISREAD(uint8_t registerAddr)
{
	// Get a register from the LIS
	uint8_t toSend = registerAddr;
	uint8_t toRead;
	HAL_I2C_Master_Transmit(&I2C_InitStructure, LIS3DH_ADDR << 1, &toSend, 1, 10); //send to LIS
	HAL_I2C_Master_Receive(&I2C_InitStructure, LIS3DH_ADDR << 1, &toRead, 1, 10); // receive
	return toRead;
}

signed int lis_get_axis(int axis)
{
	uint8_t ADDR;
	uint8_t ADDR2;
	//switch address to axis registers for access
	switch (axis)
	{
	case X_AXIS:
		ADDR = 0x28;
		ADDR2 = 0x29;
		break;
	case Y_AXIS:
		ADDR = 0x2A;
		ADDR2 = 0x2B;
		break;
	case Z_AXIS:
		ADDR = 0x2C;
		ADDR2 = 0x2D;
		break;
	default:
		return 0;
	}
	uint8_t dataH;
	uint8_t dataL;
	//get the axis data
	HAL_I2C_Master_Transmit(&I2C_InitStructure, LIS3DH_ADDR << 1, &ADDR, 1, 1000);
	HAL_I2C_Master_Receive(&I2C_InitStructure, LIS3DH_ADDR << 1, &dataL, 1, 1000);
	HAL_I2C_Master_Transmit(&I2C_InitStructure, LIS3DH_ADDR << 1, &ADDR2, 1, 1000);
	HAL_I2C_Master_Receive(&I2C_InitStructure, LIS3DH_ADDR << 1, &dataH, 1, 1000);
	uint16_t prepro = (dataH << 8) | dataL;
	
	// bias to 32768
	if ((prepro | 0x7fff) == 0xffff) //sign bit is 1 ~ negative
	{
		prepro = ~(prepro) + 1;
		prepro = 0x8000 - prepro;
	}	
	else
	{
		prepro += 0x8000;
	}
	return prepro;
}