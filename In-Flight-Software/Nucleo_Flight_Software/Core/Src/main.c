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

#define RX_BFR_SIZE 50
#define TX_BFR_SIZE 255
#define TEAM_ID "2057"

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

uint8_t GS_MAC_ADDR[8] = {0x00,0x13,0xA2,0x00,0x7A,0xD1,0x90,0x20};

// XBee
uint8_t rx_packet[RX_BFR_SIZE];
uint8_t tx_data[TX_BFR_SIZE-18];
uint8_t tx_packet[TX_BFR_SIZE];
uint8_t tx_count;

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c2;

UART_HandleTypeDef huart2;
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
static void MX_USART2_UART_Init(void);
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
int8_t mission_time_hr;
int8_t mission_time_min;
int8_t mission_time_sec;
int16_t packet_count = 0;
char mode = 'F';
char state[16] = "PRE-LAUNCH";
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
char cmd_echo[64] = "N/A";

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

// Commands
char sim_command[14];
char simp_command[15];
char set_time_command[13];
char cal_alt_command[14];
char bcn_on_command[16];
char bcn_off_command[17];
char tel_on_command[15];
char tel_off_command[16];

// State Variables
float prev_alt = 0;
int descending_count = 0;

// Other Variables
int8_t altitude_offset = 0;
int beacon_status = 0;
int telemetry_status = 0;
bool sim_enabled = false;
char rx_data[255];
HAL_StatusTypeDef uart_received;

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
	return 44330.77 * (1 - powf(pressure / 101.326, 0.1902632)) + altitude_offset;
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

void init_sensors(void)
{
	init_MPL3115A2();
	init_MMC5603();
	init_MPU6050();
	init_PA1010D();
	init_INA219();
}

void init_commands(void)
{
	snprintf(sim_command, sizeof(sim_command), "CMD,%s,SIM,", TEAM_ID);
	snprintf(simp_command, sizeof(simp_command), "CMD,%s,SIMP,", TEAM_ID);
	snprintf(set_time_command, sizeof(set_time_command), "CMD,%s,ST,", TEAM_ID);
	snprintf(cal_alt_command, sizeof(cal_alt_command), "CMD,%s,CAL,", TEAM_ID);
	snprintf(bcn_on_command, sizeof(bcn_on_command), "CMD,%s,BCN,ON", TEAM_ID);
	snprintf(bcn_off_command, sizeof(bcn_off_command), "CMD,%s,BCN,OFF", TEAM_ID);
	snprintf(tel_on_command, sizeof(tel_on_command), "CMD,%s,CX,ON", TEAM_ID);\
	snprintf(tel_off_command, sizeof(tel_off_command), "CMD,%s,CX,OFF", TEAM_ID);
}

uint8_t calculate_checksum(const char *data) {
    uint32_t sum = 0;  // Use a larger type for summing
    size_t len = strlen(data);  // Get the length of the string

    // Iterate over each byte in the string and sum their values
    for (size_t i = 0; i < len; i++) {
        sum += (uint8_t)data[i];  // Cast char to uint8_t and add to sum
    }

    // Return the result modulo 256 (0x100)
    return (uint8_t)(sum % 256);
}

void create_telemetry(uint8_t *ret, uint8_t part){
	char tel_buf[TX_BFR_SIZE-18] = {0};	//Preload buffer

	packet_count += 1;
	/* Variables TEAM_ID, MISSION_TIME, PACKET_COUNT, MODE, STATE, ALTITUDE,
	TEMPERATURE, PRESSURE, VOLTAGE, GYRO_R, GYRO_P, GYRO_Y, ACCEL_R,
	ACCEL_P, ACCEL_Y, MAG_R, MAG_P, MAG_Y, AUTO_GYRO_ROTATION_RATE,
	GPS_TIME, GPS_ALTITUDE, GPS_LATITUDE, GPS_LONGITUDE, GPS_SATS,
	CMD_ECHO [,,OPTIONAL_DATA] */
	snprintf(tel_buf, TX_BFR_SIZE,
			"@1=%s,%02d:%02d:%02d,%d,%c,%s,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%d,%02d:%02d:%02d,%.1f,%.4f,%.4f,%d,%s",
			TEAM_ID,
			mission_time_hr, mission_time_min, mission_time_sec,
			packet_count,
			mode,
			state,
			altitude,
			temperature,
			pressure,
			voltage,
			gyro_x,
			gyro_y,
			gyro_z,
			accel_x,
			accel_y,
			accel_z,
			mag_x,
			mag_y,
			mag_z,
			auto_gyro_rotation_rate,
			gps_time_hr, gps_time_min, gps_time_sec,
			gps_altitude,
			gps_latitude,
			gps_longitude,
			gps_sats,
			cmd_echo
			);
	tx_count = strlen(tel_buf);

	memset(ret, '/0', sizeof(ret));
	memcpy(ret, tel_buf, TX_BFR_SIZE-18);
}

uint16_t transmit_packet(	uint8_t *T_packet,		//w
							uint8_t *T_data,		//r
							uint8_t *dest_add,		//r
							uint16_t datanum)		//r
{
	uint16_t i,length; 	//the length of data for checksum

	/*Write Packet Length to Packet*/
	uint16_t packet_length=datanum+2;				// 1 byte for start delimiter and 1 for checksum
	length=packet_length-4;							// length for checksum
	T_packet[0]=0x7E;

  	/*Write RF Data to Packet*/
	for(i=0;i<datanum;i++)
	{
		T_packet[i+1]=T_data[i];
	}

	/*Calculate Checksum*/

	T_packet[packet_length-1]= calculate_checksum(T_data);
	return packet_length;
}

void read_transmit_telemetry (){
	read_sensors();

	if (gps_time_sec != prev_time){

		//
		if (prev_alt > altitude) {
			descending_count++;
		}
		else {
			descending_count = 0;
		}

		// Handle Mission Time
		mission_time_sec++;
		if ( mission_time_sec >= 60 ){
			mission_time_sec -= 60;
			mission_time_min += 1;
		}
		if ( mission_time_min >= 60 ){
			mission_time_min -= 60;
			mission_time_hr += 1;
		}
		if ( mission_time_hr >= 24 ){
			mission_time_hr -= 24;
		}


		create_telemetry(tx_data, 0);
		transmit_packet(tx_packet, tx_data, GS_MAC_ADDR, tx_count);
		HAL_UART_Transmit(&huart2, tx_packet, sizeof(tx_packet), 100);

		prev_time = gps_time_sec;
		prev_alt = altitude;

	}
}

void set_cmd_echo(const char *cmd)
{
	memset(cmd_echo, '\0', sizeof(cmd_echo));
	strncpy(cmd_echo, cmd, strlen(cmd));
}

void handle_command() {

	// SIM command
	if (strncmp(rx_data, sim_command, strlen(sim_command)) == 0) {

		// disable
		if (rx_data[13] == 'D'){
			set_cmd_echo("SIMDISABLE");
			mode = 'F';
			sim_enabled = false;
		}

		// enable
		if (rx_data[13] == 'E'){
			set_cmd_echo("SIMENABLE");
			sim_enabled = true;
		}

		// activate
		if (rx_data[13] == 'A' && sim_enabled == true){
			mode = 'S';
			set_cmd_echo("SIMACTIVATE");
		}

	}

	// SIMP command
	else if (strncmp(rx_data, simp_command, strlen(simp_command)) == 0) {
		if (mode == 'S') {
			char pressure_str[16];

			strncpy(pressure_str, &rx_data[14], 5);

			//memset(pressure_str, '\0', sizeof(pressure_str));
			int i = 0;
			while (pressure_str[8-i] == '\0') {
				i++;
			}

			//pressure_str[8-i] = "\0";

			pressure = atof(pressure_str)/1000;
			char temp[12] = "SIMP";
			strcat(temp, pressure_str);
			set_cmd_echo(temp);
			memset(pressure_str, '\0', sizeof(pressure_str));
		}
		sim_enabled = false;
	}

	// set time command
	else if (strncmp(rx_data, set_time_command, strlen(set_time_command)) == 0) {
		if (rx_data[12]=='G') {
			mission_time_hr = (int16_t)gps_time_hr;
			mission_time_min = (int16_t)gps_time_min;
			mission_time_sec = (int16_t)gps_time_sec;
			set_cmd_echo("STGPS");
		}
		else {
			char temp[3];
			memset(temp, 0, sizeof(temp));
			temp[0] = rx_data[12];
			temp[1] = rx_data[13];
			mission_time_hr = atoi(temp);
			memset(temp, 0, sizeof(temp));
			temp[0] = rx_data[15];
			temp[1] = rx_data[16];
			mission_time_min = atoi(temp);
			memset(temp, 0, sizeof(temp));
			temp[0] = rx_data[18];
			temp[1] = rx_data[19];
			mission_time_sec = atoi(temp);
			memset(cmd_echo, '\0', sizeof(cmd_echo));
			snprintf(cmd_echo, 11, "ST%02d:%02d:%02", mission_time_hr, mission_time_min, mission_time_sec);


		}

	}

	// Calibrate Altitude
	else if (strncmp(rx_data, "CMD,2057,CAL", strlen("CMD,2057,CAL")) == 0) {
		altitude_offset = altitude;
		set_cmd_echo("CAL");
		if (strncmp(state, "PRE-LAUNCH", strlen("PRE-LAUNCH")) == 0) {
			memset(state, 0, sizeof(state));
			strncpy(state, "LAUNCH-READY", strlen("LAUNCH-READY"));
		}

		sim_enabled = false;
	}

	// Beacon On
	else if (strncmp(rx_data, bcn_on_command, strlen(bcn_on_command)) == 0) {
		beacon_status = 1;
		set_cmd_echo("BCNON");
		sim_enabled = false;
	}

	// Beacon Off
	else if (strncmp(rx_data, bcn_off_command, strlen(bcn_off_command)) == 0) {
		beacon_status = 0;
		set_cmd_echo("BCNOFF");
		sim_enabled = false;
	}

	// Telemetry On
	else if (strncmp(rx_data, tel_on_command, strlen(tel_on_command)) == 0) {
		telemetry_status = 1;
		set_cmd_echo("CXON");
		sim_enabled = false;
	}

	// Telemetry Off
	else if (strncmp(rx_data, tel_off_command, strlen(tel_off_command)) == 0) {
		telemetry_status = 0;
		set_cmd_echo("CXOFF");
		sim_enabled = false;
	}


}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size){
	int i = 0;
	while (rx_data[i+15] != 0) {
		rx_data[i] = rx_data[i+15];
		i++;
	}
	rx_data[i-1] = 0;
	for (; i < 255; i++) {
		rx_data[i] = 0;
	}

	handle_command();
	memset(rx_data, 0, sizeof(rx_data));

	uart_received = HAL_UARTEx_ReceiveToIdle_IT(&huart2, rx_data, RX_BFR_SIZE);

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
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  uart_received = HAL_UARTEx_ReceiveToIdle_IT(&huart2, rx_data, RX_BFR_SIZE);

  init_sensors();
  init_commands();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  read_sensors();
	  // Control Telemetry
	  if (telemetry_status == 1) {
		  read_transmit_telemetry();
	  }

	  // Control Beacon
	  if (beacon_status == 1) {
		  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
	  }

	  else {
		  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
	  }

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
  HAL_GPIO_WritePin(GPIOB, LD1_Pin|LD3_Pin|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(USB_PowerSwitchOn_GPIO_Port, USB_PowerSwitchOn_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : USER_Btn_Pin */
  GPIO_InitStruct.Pin = USER_Btn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USER_Btn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD1_Pin LD3_Pin LD2_Pin */
  GPIO_InitStruct.Pin = LD1_Pin|LD3_Pin|LD2_Pin;
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