#include <stm32f4xx_hal.h>
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
	
#define R_LED_PIN GPIO_PIN_1
#define G_LED_PIN GPIO_PIN_2
#define B_LED_PIN GPIO_PIN_3
#define LED_PORT GPIOA

void LIS_INIT(void);
void LED_INIT(void);

signed int lis_get_axis(int);
#define X_AXIS 0x1
#define Y_AXIS 0x2
#define Z_AXIS 0x3
#define INT1_PIN GPIO_PIN_7
#define INT1_PORT GPIOB


#define LIS3DH_ADDR							0B0011000
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
		HAL_GPIO_WritePin(LED_PORT, G_LED_PIN, GPIO_PIN_SET);
		HAL_Delay(500);
		HAL_GPIO_WritePin(LED_PORT, G_LED_PIN, GPIO_PIN_RESET);
		HAL_Delay(500);
		HAL_Delay(240);
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
	//cool lets set it up
	
	// enable all axes, normal mode
	// 400Hz rate
	uint8_t data[2] = { 0x20, 0x98 };
	HAL_I2C_Master_Transmit(&I2C_InitStructure, LIS3DH_ADDR << 1, &data[0], 2, 1000);
	
	// High res & BDU enabled
	uint8_t a[2] = { 0x23, 0x98 };
	HAL_I2C_Master_Transmit(&I2C_InitStructure, LIS3DH_ADDR << 1, &a[0], 2, 1000);

	// DRDY on INT1
	uint8_t b[2] = { 0x22, 0x10 };
	HAL_I2C_Master_Transmit(&I2C_InitStructure, LIS3DH_ADDR << 1, &b[0], 2, 1000);

	// Turn on orientation config
	//writeRegister8(LIS3DH_REG_PL_CFG, 0x40);

	// enable adcs
	uint8_t c[2] = { 0x1f, 0x80 };
	HAL_I2C_Master_Transmit(&I2C_InitStructure, LIS3DH_ADDR << 1, &c[0], 2, 1000);
	
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