/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

typedef uint8_t bool;
#define false 0
#define true 1

#define MCP9808_ADDR    0x18   // MCP9808 I2C address (shifted left for 8-bit address)
#define MCP9808_TEMP_REG 0x05 // Temperature register address

#define INA219_ADDRESS 0x40
#define INA219_REG_BUS_VOLTAGE  0x02
#define INA219_REG_SHUNT_VOLTAGE 0x01
#define INA219_REG_POWER 0x03
#define INA219_REG_CURRENT 0x04

#define ADXL326_ADDRESS 0x1D << 1 // Replace with the correct I2C address (shifted for 8-bit address)
#define ADXL326_REG_X 0x32        // Replace with the correct X-axis register
#define ADXL326_REG_Y 0x34        // Replace with the correct Y-axis register
#define ADXL326_REG_Z 0x36        // Replace with the correct Z-axis register

#define PA_ADDRESS 0x42   // Adjust according to your PA1010D address
#define PA_BFR_SIZE 255

#define MMC5603_ADDRESS 0x0C  // MMC5603 I2C address
#define MMC5603_REG_X_LSB 0x00
#define MMC5603_REG_X_MSB 0x01
#define MMC5603_REG_Y_LSB 0x02
#define MMC5603_REG_Y_MSB 0x03
#define MMC5603_REG_Z_LSB 0x04
#define MMC5603_REG_Z_MSB 0x05

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c2;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

float temperature = 0.0;

float voltage = 0.0;

float x_axis = 0.0;
float y_axis = 0.0;
float z_axis = 0.0;

HAL_StatusTypeDef pa_ret;
uint8_t pa_buf[PA_BFR_SIZE];
HAL_StatusTypeDef pa_init_ret[5];
uint8_t prev_time = 0;
uint8_t i2cret[128];
char parse_buf[255];
char gps_lat_dir;
char gps_long_dir;
uint8_t gps_time_hr = 0;
uint8_t gps_time_min = 0;
uint8_t gps_time_sec = 0;
float gps_altitude = 0;
float gps_latitude = 0;
float gps_longitude = 0;
uint8_t gps_sats = 0;

int16_t mag_x, mag_y, mag_z;

float read_mcp9808(void) {
    uint8_t temp_reg = MCP9808_TEMP_REG; // Register to read
    uint8_t temp_data[2];                // Buffer to store temperature data
    uint16_t temp_raw;
    float temperature = 0.0;

    // Send register address
    if (HAL_I2C_Master_Transmit(&hi2c2, MCP9808_ADDR, &temp_reg, 1, 100) != HAL_OK) {
        return -1.0; // Transmission error, return an invalid temperature
    }

    // Receive temperature data (2 bytes)
    if (HAL_I2C_Master_Receive(&hi2c2, MCP9808_ADDR, temp_data, 2, 100) != HAL_OK) {
        return -1.0; // Reception error, return an invalid temperature
    }

    // Combine the received data
    temp_raw = (temp_data[0] << 8) | temp_data[1];

    // Convert raw data to temperature in Celsius
    temperature = (temp_raw & 0x0FFF) * 0.0625;
    if (temp_raw & 0x1000) {
        temperature -= 256.0;
    }

    return temperature;
}

uint8_t read_ina219() {
    uint8_t reg_addr = INA219_REG_BUS_VOLTAGE; // Set register to Bus Voltage
    uint8_t ina_buf[2];                        // Buffer to store the received data
    uint8_t ina_ret;                           // Return status of I2C communication

    // Check if INA219 is ready for communication
    ina_ret = HAL_I2C_IsDeviceReady(&hi2c2, INA219_ADDRESS, 3, 5);
    if (ina_ret == HAL_OK) {
        // Transmit the address of the Bus Voltage Register
        ina_ret = HAL_I2C_Master_Transmit(&hi2c2, INA219_ADDRESS, &reg_addr, 1, 100);
        if (ina_ret == HAL_OK) {
            // Receive 2 bytes of data from the Bus Voltage Register
            HAL_I2C_Master_Receive(&hi2c2, INA219_ADDRESS, ina_buf, 2, 10);

            // Combine the received bytes into a single 16-bit value
            int16_t raw_bus_voltage = (int16_t)(ina_buf[0] << 8 | ina_buf[1]);

            // Convert the raw value to bus voltage (in volts)
            float bus_voltage = raw_bus_voltage / 1600.0;

            // Store or return the calculated voltage as needed
            voltage = bus_voltage;
        }
    }

    return ina_ret;  // Return the I2C communication status
}

uint8_t read_adxl326() {
    uint8_t reg_addr = ADXL326_REG_X; // Start with the X-axis register
    uint8_t adxl_buf[6];              // Buffer to store the received data for X, Y, Z axes (2 bytes each)
    uint8_t adxl_ret;                 // Return status of I2C communication

    // Check if ADXL326 is ready for communication
    adxl_ret = HAL_I2C_IsDeviceReady(&hi2c2, ADXL326_ADDRESS, 3, 5);
    if (adxl_ret == HAL_OK) {
        // Transmit the address of the X-axis register
        adxl_ret = HAL_I2C_Master_Transmit(&hi2c2, ADXL326_ADDRESS, &reg_addr, 1, 100);
        if (adxl_ret == HAL_OK) {
            // Receive 6 bytes of data from the X, Y, and Z registers (2 bytes each)
            HAL_I2C_Master_Receive(&hi2c2, ADXL326_ADDRESS, adxl_buf, 6, 100);

            // Combine the received bytes into 16-bit values for each axis
            int16_t raw_x = (int16_t)(adxl_buf[0] | (adxl_buf[1] << 8));
            int16_t raw_y = (int16_t)(adxl_buf[2] | (adxl_buf[3] << 8));
            int16_t raw_z = (int16_t)(adxl_buf[4] | (adxl_buf[5] << 8));

            // Convert the raw values to g-forces (you may need to adjust the scale factor)
            float x_g = raw_x * 0.004; // Assuming 4mg/LSB (adjust this based on your sensor configuration)
            float y_g = raw_y * 0.004;
            float z_g = raw_z * 0.004;

            // Store or return the calculated g-forces as needed
            x_axis = x_g;
            y_axis = y_g;
            z_axis = z_g;
        }
    }

    return adxl_ret;  // Return the I2C communication status
}

uint8_t set_gps(char* buf, uint8_t order){
	char tmp[2];
	char tmp2[3];

	if(strlen(buf)==0)
		return 0;

	switch(order) {
	case 0: //STATUS
		if (strlen(buf)<5 || buf[0] != 'G' || buf[2] != 'G' || buf[3] != 'G' || buf[4] != 'A'){
			return 1;
		}
		break;
	case 1: //TIME
		memcpy(tmp, &buf[0], 2);
		gps_time_hr = atoi(tmp);
		memcpy(tmp, &buf[2], 2);
		gps_time_min = atoi(tmp);
		memcpy(tmp, &buf[4], 2);
		gps_time_sec = atoi(tmp);

		break;
	case 2: //LATITUDE
		gps_latitude = atof(buf) / 100;
		break;
	case 3: //LATITUDE_DIR
		gps_lat_dir = *buf;
		if (gps_lat_dir == 'S') {
			gps_latitude*= -1;
		}
		break;
	case 4: //LONGITUDE
		gps_longitude = atof(buf) / 100;
		break;
	case 5: //LONGITUDE DIR
		gps_long_dir = *buf;
		if (gps_long_dir == 'W') {
			gps_longitude*= -1;
		}
		break;
	case 7: //SATS
		gps_sats = atoi(buf);
		break;
	case 9: //ALTITUDE
		gps_altitude = atof(buf);
		break;
	default:
		break;
	}

	return 0;
}

bool parse_nmea(char *buf){
	uint8_t i;
	uint8_t last = 0;
	uint8_t order = 0;

	for(i=0; i<255;i++){
		if ( buf[i] == 44 ){
			if (last != i){
				memset(parse_buf, '\000', sizeof parse_buf);
				memcpy(parse_buf, &buf[last], i-last);
				if(set_gps(parse_buf, order)){
					return false;
				}
			}
			last = i + 1;
			order = order + 1;
		} else if (buf[i] == 42) {
			break;
		}
	}

	return true;
}

uint8_t read_pa1010d(void) {
	uint8_t pa1010d_i;
	uint8_t pa1010d_bytebuf;

	/* PA1010D (GPS) */
	for(pa1010d_i=0; pa1010d_i<255; pa1010d_i++){
		pa_ret = HAL_I2C_Master_Receive(&hi2c2, (uint16_t)(PA_ADDRESS << 1), &pa1010d_bytebuf, 1, 100);
		if (pa1010d_bytebuf == '$'){
			break;
		}
		pa_buf[pa1010d_i] = pa1010d_bytebuf;
	}
	parse_nmea(pa_buf);

	return 1;
}

uint8_t read_mmc5603(void) {
    uint8_t reg_addr = MMC5603_REG_X_LSB;  // Start with X-axis LSB register
    uint8_t mmc5603_buf[6];                // Buffer to store 6 bytes of data
    uint8_t mmc5603_ret;                   // Return status of I2C communication

    // Check if MMC5603 is ready for communication
    mmc5603_ret = HAL_I2C_IsDeviceReady(&hi2c2, MMC5603_ADDRESS << 1, 3, 5);
    if (mmc5603_ret == HAL_OK) {
        // Transmit the address of the X-axis LSB register
        mmc5603_ret = HAL_I2C_Master_Transmit(&hi2c2, MMC5603_ADDRESS << 1, &reg_addr, 1, 100);
        if (mmc5603_ret == HAL_OK) {
            // Receive 6 bytes of data from the X, Y, and Z registers (2 bytes each)
            mmc5603_ret = HAL_I2C_Master_Receive(&hi2c2, MMC5603_ADDRESS << 1, mmc5603_buf, 6, 100);

            if (mmc5603_ret == HAL_OK) {
                // Combine the received bytes into 16-bit values for each axis
                mag_x = (int16_t)(mmc5603_buf[1] << 8 | mmc5603_buf[0]);
                mag_y = (int16_t)(mmc5603_buf[3] << 8 | mmc5603_buf[2]);
                mag_z = (int16_t)(mmc5603_buf[5] << 8 | mmc5603_buf[4]);
            }
        }
    }

    return mmc5603_ret;  // Return the I2C communication status
}

/* USER CODE END 0 */

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
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // Temperature ----------------------------------------------------------
	  float temperature = read_mcp9808(); // Call the function and store the result

	  if (temperature != -1.0) {
		  // If reading is successful, process the temperature value
		  printf("Temperature: %.2f°C\n", temperature);
	  } else {
		  // Handle the error if the sensor reading fails
		  printf("Failed to read temperature from MCP9808\n");
	  }

	  // Voltage ----------------------------------------------------------
	  if (read_ina219() == HAL_OK) {
		  // If reading is successful, process the voltage value
		  // For example, print the voltage value to the console
		  printf("Bus Voltage: %.2f V\n", voltage);
	  } else {
		  // Handle the error if the sensor reading fails
		  printf("Failed to read from INA219 sensor\n");
	  }

	  // Altitude and GPS ----------------------------------------------------------
	  read_pa1010d();

	  // Output GPS data to UART or other debug interfaces
	  printf("Time: %02d:%02d:%02d\n", gps_time_hr, gps_time_min, gps_time_sec);
	  printf("Latitude: %.6f %c\n", gps_latitude, gps_lat_dir);
	  printf("Longitude: %.6f %c\n", gps_longitude, gps_long_dir);
	  printf("Altitude: %.2f meters\n", gps_altitude);
	  printf("Sats: %d\n", gps_sats);

	  // Magnetic Field ----------------------------------------------------------
	  uint8_t status = read_mmc5603();
	  if (status == HAL_OK) {
		  // Process and output the magnetic field data
		  printf("Magnetic Field - X: %d, Y: %d, Z: %d\n", mag_x, mag_y, mag_z);
	  } else {
		  // Handle error (e.g., print an error message)
		  printf("Error reading MMC5603 data\n");
	  }

	  // Cameras ----------------------------------------------------------

	  HAL_Delay(1000);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

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
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

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
  huart1.Init.BaudRate = 115200;
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
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

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

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */