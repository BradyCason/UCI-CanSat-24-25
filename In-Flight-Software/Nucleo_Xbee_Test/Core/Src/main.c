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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

// XBee
uint8_t rx_packet[RX_BFR_SIZE];
uint8_t tx_data[TX_BFR_SIZE-18];
uint8_t tx_data2[TX_BFR_SIZE-18];
uint8_t tx_packet[TX_BFR_SIZE];
uint8_t tx_packet2[TX_BFR_SIZE];
uint8_t tx_count;
uint8_t tx_count2;
bool tx_ready;
uint8_t tx_part;

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
static void MX_USART2_UART_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int telemetry_status = 1;
HAL_StatusTypeDef uart_received;

void read_transmit_telemetry (){
	read_sensors();

	if (gps_time_sec != prev_time){

		transmitting = 1;
		mission_time_sec++;

		if (prev_alt > altitude) {
			descending_count++;
		}
		else {
			descending_count = 0;
		}


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
		create_telemetry(tx_data2, 1);
		transmit_packet(tx_packet2, tx_data2, GS_MAC_ADDR, tx_count2);

		HAL_UART_Transmit(&huart1, tx_packet, sizeof(tx_packet), 100);
		HAL_Delay(50);
		HAL_UART_Transmit(&huart1, tx_packet2, sizeof(tx_packet2), 100);

		transmit_packet(tx_packet, tx_data, GS_MAC_ADDR2, tx_count);
		transmit_packet(tx_packet2, tx_data2, GS_MAC_ADDR2, tx_count2);
		HAL_Delay(50);
		HAL_UART_Transmit(&huart1, tx_packet, sizeof(tx_packet), 100);
		HAL_Delay(50);
		HAL_UART_Transmit(&huart1, tx_packet2, sizeof(tx_packet2), 100);

		prev_time = gps_time_sec;
		prev_alt = altitude;

		transmitting = 0;

	}
}

void handle_command() {
	// SIM command
	if (strncmp(rx_data, "CMD,2057,SIM,", strlen("CMD,2057,SIM,")) == 0) {

		// disable
		if (rx_data[13] == 'D'){
			memset(cmd_echo, '\0', sizeof(cmd_echo));
			strncpy(cmd_echo, "SIMDISABLE", strlen("SIMDISABLE"));
			mode = 'F';
			sim_enabled = false;
		}

		// enable
		if (rx_data[13] == 'E'){
			memset(cmd_echo, '\0', sizeof(cmd_echo));
			strncpy(cmd_echo, "SIMENABLE", strlen("SIMENABLE"));
			sim_enabled = true;
		}

		// activate
		if (rx_data[13] == 'A' && sim_enabled == true){
			mode = 'S';
			memset(cmd_echo, '\0', sizeof(cmd_echo));
			strncpy(cmd_echo, "SIMACTIVATE", strlen("SIMACTIVATE"));
		}

	}

	// SIMP command
	else if (strncmp(rx_data, "CMD,2057,SIMP,", strlen("CMD,2057,SIMP,")) == 0) {
		if (mode == 'S') {

			strncpy(pressure_str, &rx_data[14], 5);

			//memset(pressure_str, '\0', sizeof(pressure_str));
			int i = 0;
			int null_char_count = 0;
			while (pressure_str[8-i] == '\0') {
				i++;
			}

			//pressure_str[8-i] = "\0";

			pressure = atof(pressure_str)/1000;
			memset(cmd_echo, '\0', sizeof(cmd_echo));
			char temp[12] = "SIMP";
			strcat(temp, pressure_str);
			strncpy(cmd_echo, temp, strlen(temp));
			memset(pressure_str, '\0', sizeof(pressure_str));
		}
		sim_enabled = false;
	}

	// set time command
	else if (strncmp(rx_data, "CMD,2057,ST,", strlen("CMD,2057,ST,")) == 0) {
		if (rx_data[12]=='G') {
			mission_time_hr = (int16_t)gps_time_hr;
			mission_time_min = (int16_t)gps_time_min;
			mission_time_sec = (int16_t)gps_time_sec;
			memset(cmd_echo, '\0', sizeof(cmd_echo));
			strncpy(cmd_echo, "STGPS", strlen("STGPS"));
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

	else if (strncmp(rx_data, "CMD,2057,CAL", strlen("CMD,2057,CAL")) == 0) {
		base_altitude = altitude;
		memset(cmd_echo, '\0', sizeof(cmd_echo));
		strncpy(cmd_echo, "CAL", strlen("CAL"));
		if (strncmp(state, "PRE-LAUNCH", strlen("PRE-LAUNCH")) == 0) {
			memset(state, 0, sizeof(state));
			strncpy(state, "LAUNCH-READY", strlen("LAUNCH-READY"));
		}

		sim_enabled = false;
	}

	else if (strncmp(rx_data, "CMD,2057,BCN,ON", strlen("CMD,2057,BCN,ON")) == 0) {
		beacon_status = 1;
		memset(cmd_echo, '\0', sizeof(cmd_echo));
		strncpy(cmd_echo, "BCNON", strlen("BCNON"));
		sim_enabled = false;
	}
	else if (strncmp(rx_data, "CMD,2057,BCN,OFF", strlen("CMD,2057,BCN,OFF")) == 0) {
		beacon_status = 0;
		memset(cmd_echo, '\0', sizeof(cmd_echo));
		strncpy(cmd_echo, "BCNOFF", strlen("BCNOFF"));
		sim_enabled = false;
	}
	else if (strncmp(rx_data, "CMD,2057,CX,ON", strlen("CMD,2057,CX,ON")) == 0) {
		telemetry_status = 1;
		memset(cmd_echo, '\0', sizeof(cmd_echo));
		strncpy(cmd_echo, "CXON", strlen("CXON"));
		sim_enabled = false;
	}
	else if (strncmp(rx_data, "CMD,2057,CX,OFF", strlen("CMD,2057,CX,OFF")) == 0) {
		telemetry_status = 0;
		memset(cmd_echo, '\0', sizeof(cmd_echo));
		strncpy(cmd_echo, "CXOFF", strlen("CXOFF"));
		sim_enabled = false;
	}
	else if (strncmp(rx_data, "CMD,2057,OVERRIDE,1", strlen("CMD,2057,OVERRIDE,1")) == 0) {
		hs_deployed = 'P';
		memset(cmd_echo, '\0', sizeof(cmd_echo));
		strncpy(cmd_echo, "OVERRIDE1", strlen("OVERRIDE1"));
		sim_enabled = false;
	}
	else if (strncmp(rx_data, "CMD,2057,OVERRIDE,2", strlen("CMD,2057,OVERRIDE,2")) == 0) {
		pc_deployed = 'C';
		memset(cmd_echo, '\0', sizeof(cmd_echo));
		strncpy(cmd_echo, "OVERRIDE2", strlen("OVERRIDE2"));
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

	uart_received = HAL_UARTEx_ReceiveToIdle_IT(&huart1, rx_data, RX_BFR_SIZE);

}

char rxBuffer[100];
HAL_StatusTypeDef result;

void sendData(const char *data) {
    HAL_UART_Transmit(&huart2, (uint8_t*)data, strlen(data), HAL_MAX_DELAY);
}

void receiveData(char *buffer, uint16_t size) {
	result = HAL_UART_Receive(&huart2, (uint8_t*)buffer, size, HAL_MAX_DELAY);
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
  MX_USART2_UART_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */

  uart_received = HAL_UARTEx_ReceiveToIdle_IT(&huart1, rx_data, RX_BFR_SIZE);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  sendData("Hello, XBee!\r\n");
	  HAL_Delay(1000); // Delay between sends

	  receiveData(rxBuffer, sizeof(rxBuffer) - 1);
	  rxBuffer[sizeof(rxBuffer) - 1] = '\0';

	  if (telemetry_status == 1) {
	  		  read_transmit_telemetry();
	  	  }
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

  /*Configure GPIO pins : PG9 PG14 */
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : PB8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
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