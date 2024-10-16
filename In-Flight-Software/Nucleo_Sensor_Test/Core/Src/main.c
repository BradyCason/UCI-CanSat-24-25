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
#include <stdlib.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

typedef uint8_t bool;
#define false 0
#define true 1

// MMC5603 (Magnometer)
#define MMC5603_ADDRESS (0x30 << 1)  // MMC5603 I2C address
#define MMC5603_REG_CONTROL    0x1A          // Control Register 1
#define MMC5603_REG_CONTROL_2  0x1C          // Control Register 2
#define MMC5603_CONTINUOUS_MODE 0x80         // Enable continuous measurement mode
#define MMC5603_ODR_SETTING     0x08         // Set Output Data Rate (ODR) to a reasonable default

// MPL3115A2 (Temp/ Pressure)
#define MPL3115A2_ADDRESS  (0x60 << 1)  // 7-bit address, shifted left for HAL
#define MPL3115A2_WHO_AM_I 0x0C
#define MPL3115A2_CTRL_REG1 0x26
#define MPL3115A2_OUT_P_MSB 0x01

// MPU6050 (Accel/ Tilt)
#define MPU6050_ADDRESS (0x68 << 1)

// PA1010D (GPS)
#define PA1010D_ADDRESS (0x10 << 1)
#define PA_BFR_SIZE 255

//INA219 (Voltage)
#define INA219_ADDRESS (0x40 << 1)

#define PI 3.141592

#define ALTITUDE_OFFSET 0;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c2;

UART_HandleTypeDef huart3;

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USB_OTG_FS_PCD_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Variables TEAM_ID, MISSION_TIME, PACKET_COUNT, MODE, STATE, ALTITUDE,
TEMPERATURE, PRESSURE, VOLTAGE, GYRO_R, GYRO_P, GYRO_Y, ACCEL_R,
ACCEL_P, ACCEL_Y, MAG_R, MAG_P, MAG_Y, AUTO_GYRO_ROTATION_RATE,
GPS_TIME, GPS_ALTITUDE, GPS_LATITUDE, GPS_LONGITUDE, GPS_SATS,
CMD_ECHO [,,OPTIONAL_DATA] */


// Variables
float altitude;
float temperature;
float pressure;
float voltage; // Not implemented yet
float gyro_x;
float gyro_y;
float gyro_z;
float accel_x;
float accel_y;
float accel_z;
float mag_x, mag_y, mag_z;
float auto_gyro_rotation_rate; // Not implemented yet
uint8_t gps_time_hr;
uint8_t gps_time_min;
uint8_t gps_time_sec;
float gps_altitude;
float gps_latitude;
float gps_longitude;
uint8_t gps_sats;

// PA1010D DATA
uint8_t PA1010D_RATE[] = "$PMTK220,500*2B\r\n";
uint8_t PA1010D_SAT[] = "$PMTK313,1*2E\r\n";
uint8_t PA1010D_INIT[] = "$PMTK225,0*2B\r\n";
uint8_t PA1010D_CFG[] = "$PMTK353,1,0,0,0,0*2A\r\n";
uint8_t PA1010D_MODE[] = "$PMTK314,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*29\r\n";
uint8_t PA1010D_SEPERATOR = '$';
HAL_StatusTypeDef pa_ret;
uint8_t pa_buf[PA_BFR_SIZE];
HAL_StatusTypeDef pa_init_ret[5];
uint8_t prev_time = 0;
uint8_t i2cret[128];
char parse_buf[255];
char gps_lat_dir;
char gps_long_dir;

// INA219 DATA (VOLTAGE/CURRENT)
HAL_StatusTypeDef ina_ret;
uint8_t ina_buf[8];
int16_t raw_shunt_voltage;
int16_t raw_bus_voltage;
int16_t raw_power;
int16_t raw_current;
float shunt_voltage;
float bus_voltage;
float power;
float current;

HAL_StatusTypeDef result;

uint8_t set_gps(char* buf, uint8_t order){
	char tmp[2];

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

float calculate_altitude(float pressure) {
	return 44330.77 * (1 - powf(pressure / 101.326, 0.1902632)) + ALTITUDE_OFFSET;
}

void read_MMC5603(void) {
    uint8_t mmc5603_buf[9];
    uint8_t first_reg = 0x00;
	int32_t raw_x, raw_y, raw_z;

	// Perform the I2C write (send the register address) then read 9 bytes of data
	if (HAL_I2C_Master_Transmit(&hi2c2, MMC5603_ADDRESS, &first_reg, 1, HAL_MAX_DELAY) != HAL_OK) {
		// Handle transmission error
		return;
	}

	HAL_Delay(10);

	// Read 9 bytes of data from the sensor
	if (HAL_I2C_Master_Receive(&hi2c2, MMC5603_ADDRESS, mmc5603_buf, 9, HAL_MAX_DELAY) != HAL_OK) {
		// Handle reception error
		return;
	}

	// Extract X, Y, Z values from the buffer
	raw_x = ((uint32_t)mmc5603_buf[0] << 12) | ((uint32_t)mmc5603_buf[1] << 4) | ((uint32_t)mmc5603_buf[6] >> 4);
	raw_y = ((uint32_t)mmc5603_buf[2] << 12) | ((uint32_t)mmc5603_buf[3] << 4) | ((uint32_t)mmc5603_buf[7] >> 4);
	raw_z = ((uint32_t)mmc5603_buf[4] << 12) | ((uint32_t)mmc5603_buf[5] << 4) | ((uint32_t)mmc5603_buf[8] >> 4);

	// Fix center offsets

	raw_x -= (1 << 19);
	raw_y -= (1 << 19);
	raw_z -= (1 << 19);

	// Scale to Gauss
	mag_x = (float)raw_x * 0.0000625;
	mag_y = (float)raw_y * 0.0000625;
	mag_z = (float)raw_z * 0.0000625;
}

void read_MPL3115A2(void)
{
    uint8_t mpl_data[5]; // Buffer to hold pressure and temperature data

    // Read 5 bytes from OUT_P_MSB (3 for pressure, 2 for temperature)
    HAL_I2C_Mem_Read(&hi2c2, MPL3115A2_ADDRESS, MPL3115A2_OUT_P_MSB, I2C_MEMADD_SIZE_8BIT, mpl_data, 9, HAL_MAX_DELAY);

    // Combine pressure bytes into a 20-bit integer
    uint32_t p_raw = ((uint32_t)mpl_data[0] << 16) | ((uint32_t)mpl_data[1] << 8) | (mpl_data[2]);
    p_raw >>= 4; // Pressure is stored in the upper 20 bits

    // Convert raw pressure to Pascals
    pressure = p_raw / 4.0 / 1000; // Pressure in KiloPascals

    // Combine temperature bytes into a 12-bit integer
    int16_t t_raw = ((int16_t)mpl_data[3] << 8) | (mpl_data[4]);
    t_raw >>= 4; // Temperature is stored in the upper 12 bits

    // Convert raw temperature to degrees Celsius
    temperature = t_raw / 16.0; // Temperature in Celsius

    altitude = calculate_altitude(pressure);
}

void read_MPU6050(void) {
	uint8_t imu_addr = 0x3B;
	uint8_t gyro_addr = 0x43;
	HAL_StatusTypeDef mpu_ret;
	uint8_t mpu_buf[6];
	int16_t raw_accel_x;
	int16_t raw_accel_y;
	int16_t raw_accel_z;
	int16_t raw_gyro_x = 0;
	int16_t raw_gyro_y = 0;
	int16_t raw_gyro_z = 0;

	mpu_ret = HAL_I2C_IsDeviceReady(&hi2c2, MPU6050_ADDRESS, 3, 5);
    if (mpu_ret == HAL_OK){
		mpu_ret = HAL_I2C_Master_Transmit(&hi2c2, MPU6050_ADDRESS, &imu_addr, 1, 100);
		if ( mpu_ret == HAL_OK ) {
			mpu_ret = HAL_I2C_Master_Receive(&hi2c2, MPU6050_ADDRESS, mpu_buf, 6, 100);
			if ( mpu_ret == HAL_OK ) {
				// shift first byte left, add second byte
				raw_accel_x = (int16_t)(mpu_buf[0] << 8 | mpu_buf[1]);
				raw_accel_y = (int16_t)(mpu_buf[2] << 8 | mpu_buf[3]);
				raw_accel_z = (int16_t)(mpu_buf[4] << 8 | mpu_buf[5]);

				// get float values in g
				accel_x = raw_accel_x/16384.0;
				accel_y = raw_accel_y/16384.0;
				accel_z = raw_accel_z/16384.0;
			}
		}

		mpu_ret = HAL_I2C_Master_Transmit(&hi2c2, MPU6050_ADDRESS, &gyro_addr, 1, 100);
		if ( mpu_ret == HAL_OK ) {
			mpu_ret = HAL_I2C_Master_Receive(&hi2c2, MPU6050_ADDRESS, mpu_buf, 6, 100);
			if ( mpu_ret == HAL_OK ) {
				// shift first byte left, add second byte
				raw_gyro_x = (int16_t)(mpu_buf[0] << 8 | mpu_buf [1]);
				raw_gyro_y = (int16_t)(mpu_buf[2] << 8 | mpu_buf [3]);
				raw_gyro_z = (int16_t)(mpu_buf[4] << 8 | mpu_buf [5]);

				// convert to deg/sec
				gyro_x = raw_gyro_x/131.0;
				gyro_y = raw_gyro_y/131.0;
				gyro_z = raw_gyro_z/131.0;
			}
		}
    }
}

void read_PA1010D(void)
{
	uint8_t pa1010d_i;
	uint8_t pa1010d_bytebuf;

	/* PA1010D (GPS) */
	for(pa1010d_i=0; pa1010d_i<255; pa1010d_i++){
		pa_ret = HAL_I2C_Master_Receive(&hi2c2, PA1010D_ADDRESS, &pa1010d_bytebuf, 1, 100);
		if (pa1010d_bytebuf == '$'){
			break;
		}
		pa_buf[pa1010d_i] = pa1010d_bytebuf;
	}
	parse_nmea(pa_buf);
}

void read_INA219(void) {
	/* INA219 (CURRENT/VOLTAGE) */
	uint8_t bus_add = 0x02; // need to use separate registers for everything

	ina_ret = HAL_I2C_IsDeviceReady(&hi2c2, INA219_ADDRESS, 3, 5);
	if (ina_ret == HAL_OK) {
		ina_ret = HAL_I2C_Master_Transmit(&hi2c2, INA219_ADDRESS, &bus_add, 1, 100);
		if (ina_ret == HAL_OK) {
			HAL_I2C_Master_Receive(&hi2c2, INA219_ADDRESS, ina_buf, 2, 10);

			//raw_shunt_voltage = abs((int16_t)(ina_buf[0] << 8 | ina_buf[1]));
			raw_bus_voltage = (int16_t)(ina_buf[0] << 8 | ina_buf [1]);
			//raw_power = (int16_t)(ina_buf[4] << 8 | ina_buf [5]);
			//raw_current = (int16_t)(ina_buf[6] << 8 | ina_buf [7]);

			//shunt_voltage = raw_shunt_voltage*10.0;
			bus_voltage = raw_bus_voltage/1600.0;
			//power = raw_power*20/32768.0;
			//current = raw_current/32768.0;

			voltage = bus_voltage;
		}

	}

}

void init_MMC5603(void) {
	uint8_t odr_value = 100;  // Example: Set ODR to 1000 Hz by writing 255
	uint8_t control_reg0 = 0b10000000;  // Set Cmm_freq_en and Take_meas_M
	uint8_t control_reg1 = 0b10000000;  // BW0=0, BW1=0 (6.6 ms)
	uint8_t control_reg2 = 0b00010000;  // Set Cmm_en to enable continuous mode

	// Configure Control Register 1
	HAL_I2C_Mem_Write(&hi2c2, MMC5603_ADDRESS, 0x1C, I2C_MEMADD_SIZE_8BIT, &control_reg1, 1, HAL_MAX_DELAY);
	HAL_Delay(20);
	uint8_t set_bit = 0b00001000;
	HAL_I2C_Mem_Write(&hi2c2, MMC5603_ADDRESS, 0x1B, I2C_MEMADD_SIZE_8BIT, &set_bit, 1, HAL_MAX_DELAY);
	HAL_Delay(1);
	uint8_t reset_bit = 0b00010000;
	HAL_I2C_Mem_Write(&hi2c2, MMC5603_ADDRESS, 0x1B, I2C_MEMADD_SIZE_8BIT, &reset_bit, 1, HAL_MAX_DELAY);
	HAL_Delay(1);

	// Set Output Data Rate
	HAL_I2C_Mem_Write(&hi2c2, MMC5603_ADDRESS, 0x1A, I2C_MEMADD_SIZE_8BIT, &odr_value, 1, HAL_MAX_DELAY);
	HAL_Delay(10);

	// Configure Control Register 0
	HAL_I2C_Mem_Write(&hi2c2, MMC5603_ADDRESS, 0x1B, I2C_MEMADD_SIZE_8BIT, &control_reg0, 1, HAL_MAX_DELAY);
	HAL_Delay(10);

	// Configure Control Register 2
	HAL_I2C_Mem_Write(&hi2c2, MMC5603_ADDRESS, 0x1D, I2C_MEMADD_SIZE_8BIT, &control_reg2, 1, HAL_MAX_DELAY);

	// Optionally: Add a delay to allow the sensor to stabilize
	HAL_Delay(10);
}

void init_MPL3115A2(void)
{
	// Check the WHO_AM_I register to verify sensor is connected
	uint8_t who_am_i = 0;
	HAL_I2C_Mem_Read(&hi2c2, MPL3115A2_ADDRESS, MPL3115A2_WHO_AM_I, I2C_MEMADD_SIZE_8BIT, &who_am_i, 1, HAL_MAX_DELAY);
	if (who_am_i == 0xC4)
	{
		// WHO_AM_I is correct, now configure the sensor
//		uint8_t data = 0xB9; // Altimeter mode
		uint8_t data = 0x39; // Barometer mode
		HAL_I2C_Mem_Write(&hi2c2, MPL3115A2_ADDRESS, MPL3115A2_CTRL_REG1, I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);
	}
	else
	{
		// Handle error
	}
}

void init_MPU6050(void)
{
	uint8_t mpu_config = 0x00;
	uint8_t mpu_set_sample_rate = 0x07;
	uint8_t mpu_set_fs_range = 0x00;

	// wake up sensor
	HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDRESS, 0x6B, 1,&mpu_config, 1, 1000);

	// set sample rate to 1kHz, config ranges
	HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDRESS, 0x19, 1, &mpu_set_sample_rate, 1, 1000);
	HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDRESS, 0x1B, 1, &mpu_set_fs_range, 1, 1000);
	HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDRESS, 0x1c, 1, &mpu_set_fs_range, 1, 1000);
}

void init_PA1010D(void)
{
	uint8_t pa1010d_bytebuf;

//	pa_init_ret[0] = HAL_I2C_Master_Transmit(&hi2c2, PA1010D_ADDRESS, PA1010D_RATE, strlen( (char *)PA1010D_RATE), 1000);
	pa_init_ret[1] = HAL_I2C_Master_Transmit(&hi2c2, PA1010D_ADDRESS, PA1010D_INIT, strlen( (char *)PA1010D_INIT), 1000);
	pa_init_ret[2] = HAL_I2C_Master_Transmit(&hi2c2, PA1010D_ADDRESS, PA1010D_SAT, strlen( (char *)PA1010D_SAT), 1000);
//	pa_init_ret[3] = HAL_I2C_Master_Transmit(&hi2c2, PA1010D_ADDRESS, PA1010D_CFG, strlen( (char *)PA1010D_CFG), 1000);
	pa_init_ret[4] = HAL_I2C_Master_Transmit(&hi2c2, PA1010D_ADDRESS, PA1010D_MODE, strlen( (char *)PA1010D_MODE), 1000);

	//Wait for stabilization
	for(int j=0; j<10; j++){
		for(int i=0; i<255; i++){
			HAL_I2C_Master_Receive(&hi2c2, PA1010D_ADDRESS, &pa1010d_bytebuf, 1, 10);
			if (pa1010d_bytebuf == '$'){
				break;
			}
			pa_buf[i] = pa1010d_bytebuf;
		}
		if (j>5){
			parse_nmea(pa_buf);
		}
		HAL_Delay(500);
	}
}

void init_INA219(void)
{
	uint8_t ina_config[2] = {0b00000001, 0b00011101};
	HAL_I2C_Mem_Write(&hi2c2, (uint16_t) INA219_ADDRESS, 0x05, 1, ina_config, 2, 1000);
}

void read_sensors(void)
{
	read_MPL3115A2(); // Temperature/ Pressure
	read_MMC5603(); // Magnetic Field
	read_MPU6050(); // Accel/ tilt
	read_PA1010D(); // GPS
	read_INA219(); // Voltage
}

void init_sensors()
{
	result = HAL_I2C_IsDeviceReady(&hi2c2, MPU6050_ADDRESS, 3, 5);

	init_MPL3115A2();
	init_MMC5603();
	init_MPU6050();
	init_PA1010D();
	init_INA219();
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
  MX_USART3_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */

  init_sensors();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {

	  read_sensors();

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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 384;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
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
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief USB_OTG_FS Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_OTG_FS_PCD_Init(void)
{

  /* USER CODE BEGIN USB_OTG_FS_Init 0 */

  /* USER CODE END USB_OTG_FS_Init 0 */

  /* USER CODE BEGIN USB_OTG_FS_Init 1 */

  /* USER CODE END USB_OTG_FS_Init 1 */
  hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
  hpcd_USB_OTG_FS.Init.dev_endpoints = 6;
  hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_OTG_FS.Init.Sof_enable = ENABLE;
  hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.battery_charging_enable = ENABLE;
  hpcd_USB_OTG_FS.Init.vbus_sensing_enable = ENABLE;
  hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_OTG_FS_Init 2 */

  /* USER CODE END USB_OTG_FS_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD1_Pin|LD3_Pin|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(USB_PowerSwitchOn_GPIO_Port, USB_PowerSwitchOn_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : USER_Btn_Pin */
  GPIO_InitStruct.Pin = USER_Btn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USER_Btn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD1_Pin LD3_Pin PB7 */
  GPIO_InitStruct.Pin = LD1_Pin|LD3_Pin|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = USB_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(USB_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_OverCurrent_Pin */
  GPIO_InitStruct.Pin = USB_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USB_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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
