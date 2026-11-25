
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

#if CFG_TUSB_OS != OPT_OS_FREERTOS
# error "This should be using FREERTOS but the CFG_TUSB_OS is not OPT_OS_FREERTOS"
#endif

// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE 2048 

// Flag to indicate button press
volatile bool sw1_pressed = false;
volatile bool sw2_pressed = false;
// Indicate whether to save a character or send a space for State 2 Button 2 press
volatile bool SAVE = false;
volatile bool SEND_SPACE = false;

void buzzer_task(void *pvParameters);
void imu_task(void *pvParameters);
void initial_task(void *pvParameters);
void write_task(void *pvParameters);
void send_task(void *pvParameters);

//Add here necessary states
enum state { IDLE=1, READ_POS, END_MSG, PLAY_MSG };
enum state programState = IDLE;
enum state lastState;

// Array to store RGB values
uint16_t RGB[] = {0, 255, 0};

// Array to store message
char msg[500];

volatile short i = 0;

void initial_task(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        if(programState == IDLE)
        {  
            lastState = IDLE;

            RGB[0] = 0; RGB[1] = 255; RGB[2] = 0;

            sw1_pressed = gpio_get(SW1_PIN);
            sw2_pressed = gpio_get(SW2_PIN);

            rgb_led_write(RGB[0], RGB[1], RGB[2]);

            if(sw1_pressed)
            {
                while(gpio_get(SW1_PIN))
                {
                    vTaskDelay(pdMS_TO_TICKS(60));
                }
                memset(msg, 0, sizeof(msg));
                i = 0;
                programState = READ_POS;
            }
            else if (sw2_pressed)
            {
                while(gpio_get(SW2_PIN))
                {
                    vTaskDelay(pdMS_TO_TICKS(60));
                }
                programState = PLAY_MSG;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void write_task(void *pvParameters) {
    (void)pvParameters;
    
    while (1) {
        if(programState == READ_POS)
        {
            RGB[0] = 255; RGB[1] = 0, RGB[2] = 0;
            rgb_led_write(RGB[0], RGB[1], RGB[2]);

            sw1_pressed = gpio_get(SW1_PIN);
            sw2_pressed = gpio_get(SW2_PIN);
        
            if(sw1_pressed)
            {
                while(gpio_get(SW1_PIN))
                {
                    vTaskDelay(pdMS_TO_TICKS(60));
                }
                programState = END_MSG;
            }
            else if(sw2_pressed)
            {
                vTaskDelay(pdMS_TO_TICKS(1200));

                sw2_pressed = gpio_get(SW2_PIN);

                if(!sw2_pressed)
                {
                    SAVE = true;
                }
                else
                {
                    SEND_SPACE = true;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void send_task(void *pvParameters) {
    (void)pvParameters;

    while (1)
    {
        if(programState == END_MSG)
        {
            lastState = END_MSG;

            RGB[2] = 255; RGB[0] = 0;
            rgb_led_write(RGB[0], RGB[1], RGB[2]);
        
            sw1_pressed = gpio_get(SW1_PIN);
            sw2_pressed = gpio_get(SW2_PIN);

            if(sw1_pressed)
            {
                memset(msg, 0, sizeof(msg));
                i = 0;
                while(gpio_get(SW1_PIN))
                {
                    vTaskDelay(pdMS_TO_TICKS(60));
                }
                printf("\n\n\n\n\n\n\n\n\n");
                programState = READ_POS;
            }
            else if(sw2_pressed)
            {
                vTaskDelay(pdMS_TO_TICKS(1200));

                sw2_pressed = gpio_get(SW2_PIN);
                if(!sw2_pressed)
                {
                    printf("\n\n\n\n\n\n\n\n\n");
                    programState = PLAY_MSG;
                }
                else
                {
                    while(gpio_get(SW2_PIN))
                    {
                        vTaskDelay(pdMS_TO_TICKS(60));
                    }
                    printf("%s", msg);
                    programState = IDLE;
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void buzzer_task(void *pvParameters) {
    (void)pvParameters;

    rgb_led_write(RGB[0], RGB[1], RGB[2]);

    while (1) {
        if(programState == PLAY_MSG)
        {
            printf("\nPlaying message...\n");

            for(int j = 0; j < sizeof(msg) - 1; ++j)
            {
                vTaskDelay(pdMS_TO_TICKS(250));

                rgb_led_write(RGB[0], RGB[1], RGB[2]);

                if(msg[j] == '-')
                {
                    buzzer_play_tone(400, 500);
                    rgb_led_write(0, 0, 0);
                }
                else if(msg[j] == '.')
                {
                    buzzer_play_tone(400, 250);
                    rgb_led_write(0, 0, 0);
                }
                else if(msg[j] == ' ')
                {
                    rgb_led_write(0, 0, 0);
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
                else
                {
                    buzzer_turn_off();
                    programState = lastState;
                    break;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void imu_task(void *pvParameters) {
    (void)pvParameters;

    init_ICM42670();

    ICM42670_start_with_default_values();
    
    float ax, ay, az, gx, gy, gz, t;

    while (1)
    {
        if(programState == READ_POS)
        {
            if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {

                if(az >= -0.25f && az <= 0.25f && SAVE == true)
                {
                    msg[i] = '-';
                    printf("-");
                    ++i;
                }
                else if((az >= 0.75f || az <= -0.75f) && SAVE == true)
                {
                    msg[i] = '.';
                    printf(".");
                    ++i;
                }
                else if(SAVE == false && SEND_SPACE == true)
                {
                    msg[i] = ' ';
                    printf(" ");
                    ++i;
                    SEND_SPACE = false;
                }
            } 
            else {
                printf("Failed to read imu data\n");
            }

            SAVE = false;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

}

int main() {
    stdio_init_all();
    // Uncomment this lines if you want to wait till the serial monitor is connected
    while (!stdio_usb_connected()){
        sleep_ms(10);
    }
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    // initializing RGB led
    init_rgb_led();

    // initializing buzzer
    init_buzzer();

    // initializing buttons
    init_sw1();
    init_sw2();

    // Initialize I2C
    init_i2c_default();

    // Create tasks
    TaskHandle_t hIN, hWR, hSE, hBuzz, hIMU = NULL;

    BaseType_t initRes = xTaskCreate(initial_task, "InitialTask", DEFAULT_STACK_SIZE, NULL, 4, &hIN);
    if(initRes != pdPASS)
    {
        printf("Initial Task creation failed\n");
        return 0;
    }
    initRes = xTaskCreate(write_task, "WriteTask", DEFAULT_STACK_SIZE, NULL, 3, &hWR);
    if(initRes != pdPASS)
    {
        printf("Write Task creation failed\n");
        return 0;
    }
    initRes = xTaskCreate(send_task, "SendTask", DEFAULT_STACK_SIZE, NULL, 3, &hSE);
    if(initRes != pdPASS)
    {
        printf("Send Task creation failed\n");
        return 0;
    }
    initRes = xTaskCreate(buzzer_task, "BuzzerTask", DEFAULT_STACK_SIZE, NULL, 2, &hBuzz);
    if(initRes != pdPASS)
    {
        printf("Buzzer Task creation failed\n");
        return 0;
    }
    initRes = xTaskCreate(imu_task, "IMUTask", DEFAULT_STACK_SIZE, NULL, 3, &hIMU);
    if(initRes != pdPASS)
    {
        printf("IMU Task creation failed\n");
        return 0;
    }

    printf("\n\n\n\n\n\n\n\nHello!\n");

    // Start the FreeRTOS scheduler
    vTaskStartScheduler();

    return 0;
}