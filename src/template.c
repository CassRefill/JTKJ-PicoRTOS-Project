#include <stdio.h>
#include <string.h>
#include <FreeRTOS.h>
#include <hardware/gpio.h>
#include <hardware/i2c.h>
#include <pico/stdlib.h>
#include <queue.h>
#include <task.h>

#include <tkjhat/sdk.h>
#include <pico/binary_info.h>

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

void sw1_task(void *pvParameters);
void sw2_task(void *pvParameters);
void rgb_task(void *pvParameters);
void buzzer_task(void *pvParameters);
void imu_task(void *pvParameters);

//Add here necessary states
enum state { IDLE=1, READ_POS=2, END_MSG=3, PLAY_MSG=4 };
enum state programState = IDLE;

// Array to store RGB values
uint16_t RGB[] = {0, 255, 0};

// Array to store message
char msg[500];

void sw1_task(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        sw1_pressed = gpio_get(SW1_PIN);
        if(sw1_pressed)
        {
            programState = READ_POS;
        }
        vTaskDelay(10);
    }
    while (2) {
        sw1_pressed = gpio_get(SW1_PIN);
        if(sw1_pressed)
        {
            programState = END_MSG;
        }
        vTaskDelay(10);
    }
    while (3) {
        sw1_pressed = gpio_get(SW1_PIN);
        if(sw1_pressed)
        {
            memset(msg, 0, sizeof(msg));
            programState = READ_POS;
        }
        vTaskDelay(10);
    }
}

void sw2_task(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        sw2_pressed = gpio_get(SW2_PIN);
        if (sw2_pressed)
        {
            programState = PLAY_MSG;
        }
        vTaskDelay(10);
    }
    while (2) {
        sw2_pressed = gpio_get(SW2_PIN);
        if (sw2_pressed)
        {
            SAVE == true;
        }
        vTaskDelay(2000);
        if (sw2_pressed)
        {
            SEND_SPACE == true;
        }
    }
    while (3) {
        sw2_pressed = gpio_get(SW2_PIN);
        if (sw2_pressed)
        {
            programState == PLAY_MSG;
        }
        vTaskDelay(2000);
        if (sw2_pressed)
        {
            programState == IDLE;
        }
    }
}

void rgb_task(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        rgb_led_write(RGB[0], RGB[1], RGB[2]);
        vTaskDelay(500);
    }
    while (2)
    {
        RGB[0], RGB[1] = 255, 0;
        rgb_led_write(RGB[0], RGB[1], RGB[2]);
        vTaskDelay(500);
    }
    while (3)
    {
        RGB[2], RGB[0] = 255, 0;
        rgb_led_write(RGB[0], RGB[1], RGB[2]);
        vTaskDelay(500);
    }
}

void buzzer_task(void *pvParameters) {
    (void)pvParameters;

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
                break;
            }
        }
        vTaskDelay(250);
    }
}

void imu_task(void *pvParameters) {
    (void)pvParameters;

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

    // Initialize SW1 
    init_sw1();

    // Initialize SW2
    init_sw2();

    // Initialize RGB
    init_rgb_led();

    // Initialize Buzzer
    init_buzzer();
   
    // Initialize I2C
    init_i2c_default();

    // Create tasks
    
    xTaskCreate(sw2_task, "SW2Task", 256, NULL, 1, NULL);
    xTaskCreate(rgb_task, "RGBTask", 256, NULL, 1, NULL);
    xTaskCreate(buzzer_task, "BuzzerTask", 256, NULL, 4, NULL);
    xTaskCreate(imu_task, "IMUTask", 256, NULL, 1, NULL);
    xTaskCreate(sw1_task, "SW1Task", 64, NULL, 1, NULL);
    
    //Initialize IMU
    if (init_ICM42670() == 0) {
        printf("ICM-42670P initialized successfully!\n");
        int _gyro = ICM42670_startGyro(ICM42670_GYRO_ODR_DEFAULT, ICM42670_GYRO_FSR_DEFAULT);
        printf ("Gyro return:  %d\n", _gyro);
        int _accel = ICM42670_startAccel(ICM42670_ACCEL_ODR_DEFAULT, ICM42670_ACCEL_FSR_DEFAULT);
        printf ("Accel return:  %d\n", _accel);

        /*
        if (ICM42670_startAccel(ICM42670_ACCEL_ODR_DEFAULT, ICM42670_ACCEL_FSR_DEFAULT) != 0){
            printf("Wrong values to init the accelerometer in ICM-42670P.\n");
        }
        if (ICM42670_startGyro(ICM42670_GYRO_ODR_DEFAULT, ICM42670_GYRO_FSR_DEFAULT) != 0){
            printf("Wrong values to init the gyroscope in ICM-42670P.\n");
        };
        ICM42670_enable_accel_gyro_ln_mode();
        */
       int _enablegyro = ICM42670_enable_accel_gyro_ln_mode();
       printf ("Enable gyro: %d\n",_enablegyro);
    } else {
        printf("Failed to initialize ICM-42670P.\n");
    }
    
    // Start the FreeRTOS scheduler
    vTaskStartScheduler();

    return 0;
}

