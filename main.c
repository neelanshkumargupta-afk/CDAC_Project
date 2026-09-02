/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32 Node (USART2 - 115200 Baud Rate) + Servo Motor
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
/* USER CODE END Includes */

/* Handles -------------------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint8_t  pir_state        = 0;
uint32_t mq135_adc_val    = 0;
uint16_t estimatedPpm     = 0;

float    temperature      = 0.0f;
float    humidity         = 0.0f;
uint8_t  dht_status       = 0;
uint8_t  dht_data[5]      = {0};

uint8_t  motor_active     = 0;
uint32_t motor_start_time = 0;

char     txBuffer[100];
uint8_t  rxSingleByte;
char     rxBuffer[50];
uint8_t  rxIndex          = 0;
/* USER CODE END PV */

/* Prototypes ----------------------------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);

void delay_us(uint16_t us);
void DHT22_SetPin_Output(void);
void DHT22_SetPin_Input(void);
uint8_t DHT22_Read(float *temp, float *hum);
void Set_Servo_Angle(uint8_t angle);

/* USER CODE BEGIN 0 */

/* Servo Motor Helper --------------------------------------------------------*/
// Generates a 50Hz PWM pulse mapping 0-180 degrees to 500-2500 microseconds.
void Set_Servo_Angle(uint8_t angle) {
    if(angle > 180) angle = 180; // Bound to 180 max
    // Formula maps 0 deg = 500us and 180 deg = 2500us
    uint32_t pulse_width = 500 + ((uint32_t)angle * 2000) / 180;
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse_width);
}

/* Microsecond Delay via TIM3 ------------------------------------------------*/
void delay_us(uint16_t us) {
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    while (__HAL_TIM_GET_COUNTER(&htim3) < us);
}

/* DHT22 GPIO Configuration Helpers ------------------------------------------*/
void DHT22_SetPin_Output(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void DHT22_SetPin_Input(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/* DHT22 Reading Logic with Safe Timeout Handling ---------------------------*/
uint8_t DHT22_Read(float *temp, float *hum) {
    uint8_t i, j;
    uint32_t t, refPulse, threshold;

    DHT22_SetPin_Output();
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
    delay_us(1200);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
    delay_us(30);
    DHT22_SetPin_Input();

    __disable_irq();

    t = __HAL_TIM_GET_COUNTER(&htim3);
    while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)) {
        if (__HAL_TIM_GET_COUNTER(&htim3) - t > 200) { __enable_irq(); return 0; }
    }

    t = __HAL_TIM_GET_COUNTER(&htim3);
    while (!HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)) {
        if (__HAL_TIM_GET_COUNTER(&htim3) - t > 500) { __enable_irq(); return 0; }
    }

    t = __HAL_TIM_GET_COUNTER(&htim3);
    while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)) {
        if (__HAL_TIM_GET_COUNTER(&htim3) - t > 500) { __enable_irq(); return 0; }
    }
    refPulse  = __HAL_TIM_GET_COUNTER(&htim3) - t;
    threshold = refPulse / 2;

    for (j = 0; j < 5; j++) {
        dht_data[j] = 0;
        for (i = 0; i < 8; i++) {
            t = __HAL_TIM_GET_COUNTER(&htim3);
            while (!HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)) {
                if (__HAL_TIM_GET_COUNTER(&htim3) - t > 2000) { __enable_irq(); return 0; }
            }

            t = __HAL_TIM_GET_COUNTER(&htim3);
            while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1)) {
                if (__HAL_TIM_GET_COUNTER(&htim3) - t > 2000) { __enable_irq(); return 0; }
            }
            uint32_t highTime = __HAL_TIM_GET_COUNTER(&htim3) - t;

            if (highTime > threshold) {
                dht_data[j] |= (1 << (7 - i));
            }
        }
    }

    __enable_irq();

    if (dht_data[4] == ((dht_data[0] + dht_data[1] + dht_data[2] + dht_data[3]) & 0xFF)) {
        int16_t raw_hum  = (dht_data[0] << 8) | dht_data[1];
        int16_t raw_temp = ((dht_data[2] & 0x7F) << 8) | dht_data[3];

        *hum  = raw_hum / 10.0f;
        *temp = raw_temp / 10.0f;

        if (dht_data[2] & 0x80) *temp *= -1.0f;
        return 1;
    }
    return 0;
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_ADC1_Init();
  MX_USART2_UART_Init();

  HAL_TIM_Base_Start(&htim3);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

  // Set Servo to 0 degrees initially
  Set_Servo_Angle(0);

  uint32_t last_sensor_read_time = 0; // Added to track the 1-second interval

  while (1)
  {
    /* ==================================================================== */
    /* 1. SENSOR READING & TRANSMISSION (Executes only once every 1000 ms)  */
    /* ==================================================================== */
    if (HAL_GetTick() - last_sensor_read_time >= 1000)
    {
        last_sensor_read_time = HAL_GetTick();

        /* Motion Acquisition (PA0) */
        pir_state = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) ? 1 : 0;

        /* Air Quality ADC Acquisition (PC0) */
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
            mq135_adc_val = HAL_ADC_GetValue(&hadc1);
            estimatedPpm  = (uint16_t)((mq135_adc_val / 4095.0f) * 3500.0f);
        }
        HAL_ADC_Stop(&hadc1);

        /* Temp/Humidity Acquisition (PA1) */
        dht_status = DHT22_Read(&temperature, &humidity);

        /* Formatting and UART Transmission via USART2 (PA2 TX) */
        int   temp_int = (int)temperature;
        float temp_abs_frac = fabsf(temperature) - fabsf((float)temp_int);
        int   temp_dec = (int)(temp_abs_frac * 10.0f + 0.5f);

        if (temp_dec >= 10) {
            temp_dec = 0;
            temp_int += (temperature < 0.0f) ? -1 : 1;
        }

        const char* sign = (temperature < 0.0f) ? "-" : "";

        memset(txBuffer, 0, sizeof(txBuffer));
        snprintf(txBuffer, sizeof(txBuffer), "PIR:%d,CO2:%u,TEMP:%s%d.%d\r\n",
                 pir_state, estimatedPpm, sign, abs(temp_int), temp_dec);

        HAL_UART_Transmit(&huart2, (uint8_t*)txBuffer, (uint16_t)strlen(txBuffer), 200);
    }


    /* ==================================================================== */
        /* 2. CONTINUOUS UART COMMAND RECEPTION (Runs constantly, no delays)    */
        /* ==================================================================== */
        if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) {
            __HAL_UART_CLEAR_OREFLAG(&huart2); // Clear Overrun Error if data piled up
        }

        // Check if the hardware flag indicates a byte is physically waiting
        if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {

            // Since we know a byte is here, a 1ms timeout is safe and won't block
            if (HAL_UART_Receive(&huart2, &rxSingleByte, 1, 1) == HAL_OK) {
                if (rxSingleByte != '\n' && rxSingleByte != '\r') {
                    // Prevent buffer overflow
                    if (rxIndex < sizeof(rxBuffer) - 1) {
                        rxBuffer[rxIndex++] = rxSingleByte;
                    }
                } else if (rxIndex > 0) {
                    rxBuffer[rxIndex] = '\0';
                    rxIndex = 0;

                    // Evaluate the completed string
                    if (strstr(rxBuffer, "ROTATE_MOTOR") != NULL) {
                        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
                        motor_active = 1;
                        motor_start_time = HAL_GetTick();

                        Set_Servo_Angle(120);
                    }
                }
            }
        }


    /* ==================================================================== */
    /* 3. NON-BLOCKING MOTOR CUTOFF TIMER                                   */
    /* ==================================================================== */
    if (motor_active && (HAL_GetTick() - motor_start_time >= 3000)) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
        motor_active = 0;

        // Return servo to 0 degrees when cutoff finishes
        Set_Servo_Angle(0);
    }
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* TIM2 Initialization for Servo PWM */
static void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  __HAL_RCC_TIM2_CLK_ENABLE();

  htim2.Instance = TIM2;
  /*
     SYSCLK is 168MHz, APB1 is 42MHz, meaning TIM2 clock runs at 84MHz (42 * 2).
     Prescaler = (84 - 1) gives 1MHz timer tick (1 tick = 1 microsecond).
     Period (ARR) = (20000 - 1) gives a 20ms period (50Hz), perfect for servos.
  */
  htim2.Init.Prescaler = 84 - 1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 20000 - 1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) Error_Handler();

  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) Error_Handler();

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) Error_Handler();

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 500; /* Initial pulse: 0 degrees */
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) Error_Handler();

  /* Configure PA5 for Alternate Function 1 (TIM2_CH1) */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}
/* END TIM2 Initialization */

static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.Channel = ADC_CHANNEL_10;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_TIM3_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 83;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200; /* Configured to 115200 */
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
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* PC0 (MQ135 ADC Analog Pin) Setup */
  GPIO_InitStruct.Pin  = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* Motor Pin PB0 Output Setup */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* PIR Digital Pin PA0 Input Setup */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USART2 Alternate Function Setup (PA2 TX / PA3 RX) */
  GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
