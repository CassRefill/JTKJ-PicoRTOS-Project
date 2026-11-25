
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
enum state { IDLE=1, READ_POS=2, END_MSG=3, PLAY_MSG=4 };
enum state programState = IDLE;

// Array to store RGB values
uint16_t RGB[] = {0, 255, 0};

// Array to store message
char msg[500];

void initial_task(void *pvParameters) {
    (void)pvParameters;

    init_sw1();
    init_sw2();
    init_rgb_led();

    while (1) {
        sw1_pressed = gpio_get(SW1_PIN);
        sw2_pressed = gpio_get(SW2_PIN);

        rgb_led_write(RGB[0], RGB[1], RGB[2]);

        if(sw1_pressed)
        {
            programState = READ_POS;
        }
        else if (sw2_pressed)
        {
            programState = PLAY_MSG;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void write_task(void *pvParameters) {
    (void)pvParameters;

    init_sw1();
    init_sw2();
    init_rgb_led();

    while (2) {
        RGB[0], RGB[1] = 255, 0;
        rgb_led_write(RGB[0], RGB[1], RGB[2]);

        sw1_pressed = gpio_get(SW1_PIN);
        sw2_pressed = gpio_get(SW2_PIN);
        
        if(sw1_pressed)
        {
            programState = END_MSG;
        }
        else if(sw2_pressed)
        {
            vTaskDelay(pdMS_TO_TICKS(750));
            if(!sw2_pressed)
            {
                SAVE = true;
            }
            else
            {
                SEND_SPACE = true;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void send_task(void *pvParameters) {
    (void)pvParameters;

    init_sw1();
    init_sw2();
    init_rgb_led();

    while (3)
    {
        RGB[2], RGB[0] = 255, 0;
        rgb_led_write(RGB[0], RGB[1], RGB[2]);
        
        sw1_pressed = gpio_get(SW1_PIN);
        sw2_pressed = gpio_get(SW2_PIN);

        if(sw1_pressed)
        {
            memset(msg, 0, sizeof(msg));
            programState = READ_POS;
        }
        else if(sw2_pressed)
        {
            vTaskDelay(pdMS_TO_TICKS(750));
            if(!sw2_pressed)
            {
                programState = PLAY_MSG;
            }
            else
            {
                programState = IDLE;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void buzzer_task(void *pvParameters) {
    (void)pvParameters;

    init_buzzer();

    while (4) {
        for(int j = 0; j < sizeof(msg); ++j)
        {
            init_rgb_led();

            if(msg[j] == '-')
            {
                buzzer_play_tone(440, 500);
                stop_rgb_led();
            }
            if(msg[j] == '.')
            {
                buzzer_play_tone(440, 250);
                stop_rgb_led();
            }
            if(msg[j] == ' ')
            {
                stop_rgb_led();
                buzzer_play_tone(0, 500);
            }
            else
            {
                buzzer_turn_off();
                programState = END_MSG;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void imu_task(void *pvParameters) {
    (void)pvParameters;

    init_ICM42670();

    ICM42670_start_with_default_values();

    short i = 0;
    
    float ax, ay, az, gx, gy, gz, t;

    while (2)
    {
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {

            if(gx + 10 >= 90 && gx - 10 <= 90 && SAVE == true)
            {
                msg[i] = '-';
                SAVE == false;
            }
            else if(gx + 10 >= 0 && gx - 10 <= 0 && SAVE == true)
            {
                msg[i] = '.';
                SAVE == false;
            }
            else if(SAVE == false && SEND_SPACE == true)
            {
                msg[i] = ' ';
                SEND_SPACE == false;
            }

        } else {
            printf("Failed to read imu data\n");
        }
        ++i;
        vTaskDelay(pdMS_TO_TICKS(60));
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
   
    // Initialize I2C
    init_i2c_default();

    // Create tasks
    TaskHandle_t hIN, hWR, hSE, hBuzz, hIMU = NULL;

    xTaskCreate(initial_task, "InitialTask", DEFAULT_STACK_SIZE, NULL, 1, &hIN);
    xTaskCreate(write_task, "WriteTask", DEFAULT_STACK_SIZE, NULL, 1, &hWR);
    xTaskCreate(send_task, "SendTask", DEFAULT_STACK_SIZE, NULL, 3, &hSE);
    xTaskCreate(buzzer_task, "BuzzerTask", DEFAULT_STACK_SIZE, NULL, 4, &hBuzz);
    xTaskCreate(imu_task, "IMUTask", DEFAULT_STACK_SIZE, NULL, 2, &hIMU);
    
    // Start the FreeRTOS scheduler
    vTaskStartScheduler();

    return 0;
}