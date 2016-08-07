#include <stm32f4xx_hal.h>
#include "defines.h"
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

void LIS_INIT(void);
void LED_INIT(void);
void LISCMD(uint8_t, uint8_t);
uint8_t LISREAD(uint8_t);

signed int lis_get_axis(int);
I2C_HandleTypeDef I2C_InitStructure; //global


void SysTick_Handler(void)
{
	HAL_IncTick();
	HAL_SYSTICK_IRQHandler();
}

int main(void)
{
	HAL_Init();
	LIS_INIT();
	
		for (;;)
	{
		if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7))
		{
			uint8_t INT1 = LISREAD(INT1_SRC);
			uint8_t CLICKS = LISREAD(CLICK_SRC);
			
			HAL_GPIO_WritePin(LED_PORT, G_LED_PIN, GPIO_PIN_SET);
			HAL_Delay(500);
			HAL_GPIO_WritePin(LED_PORT, G_LED_PIN, GPIO_PIN_RESET);
			HAL_Delay(500);
		}
		
	}
}




void LED_INIT(void)
{
	//** Initializes GPIO for LED pins
	__GPIOA_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.Pin = G_LED_PIN;

	GPIO_InitStructure.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(LED_PORT, &GPIO_InitStructure);
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
	HAL_StatusTypeDef x = HAL_I2C_IsDeviceReady(&I2C_InitStructure, 0B00110000, 3, 1000);
	
	
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
	#define CLICKTHRESHHOLD 20
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

	GPIO_InitStructure.Mode = GPIO_MODE_INPUT;
	GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStructure.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	// -------------------------------------------------------------
	
}

void LISCMD(uint8_t registerAddr, uint8_t cmmd)
{
	uint8_t toSend[2] = { registerAddr, cmmd }; //create command data
	HAL_I2C_Master_Transmit(&I2C_InitStructure, LIS3DH_ADDR << 1, &toSend[0], 2, 1000); //send to LIS
}

uint8_t LISREAD(uint8_t registerAddr)
{
	uint8_t toSend = registerAddr;
	uint8_t toRead;
	HAL_I2C_Master_Transmit(&I2C_InitStructure, LIS3DH_ADDR << 1, &toSend, 1, 1000); //send to LIS
	HAL_I2C_Master_Receive(&I2C_InitStructure, LIS3DH_ADDR << 1, &toRead, 1, 1000); // receive
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