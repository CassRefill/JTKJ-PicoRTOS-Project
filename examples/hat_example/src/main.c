
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

/*#if   CFG_TUSB_OS == OPT_OS_FREERTOS
# pragma message("TinyUSB: FreeRTOS")
#elif CFG_TUSB_OS == OPT_OS_PICO
# pragma message("TinyUSB: OS PICO")
#elif CFG_TUSB_OS == OPT_OS_NONE
# pragma message("TinyUSB: sin RTOS")
#endif*/

// Flag to indicate button press
volatile bool sw1_pressed = false;
volatile bool sw2_pressed = false;

void sw1_task(void *pvParameters);
void sw2_task(void *pvParameters);
void rgb_task(void *pvParameters);
void buzzer_task(void *pvParameters);
void imu_task(void *pvParameters);

void sw2_task(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        sw2_pressed = gpio_get(SW2_PIN);
        vTaskDelay(10);
    }
}

void rgb_task(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        rgb_led_write(20, 30, 255);
        vTaskDelay(500);
        rgb_led_write(255, 30, 10);
        vTaskDelay(500);
        rgb_led_write(50, 255, 10);
        vTaskDelay(500);
    }
}

void buzzer_task(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        if (sw2_pressed) {
            buzzer_play_tone(440, 500);
        }
        vTaskDelay(10);
    }
}

void imu_task(void *pvParameters) {
    (void)pvParameters;
    
    float ax, ay, az, gx, gy, gz, t;

    while (1)
    {
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {
            float temp_c = (float)t / 128.0f;
            printf("Accel: X=%d, Y=%d, Z=%d | Gyro: X=%d, Y=%d, Z=%d | Temp: %.2f°C\n", ax, ay, az, gx, gy, gz, temp_c);

        } else {
            printf("Failed to read imu data\n");
        }
        vTaskDelay(pdMS_TO_TICKS(60));
    }

}

/**
 * @brief FreeRTOS task for monitoring switch SW1 state.
 *
 * This task continuously reads the digital input connected to SW1
 * and stores its state in the global variable @ref sw1_pressed.
 *
 * @param[in] pvParameters
 *        Pointer to task parameters. Not used in this implementation,
 *        should be passed as NULL when creating the task.
 * @see gpio_get()
 * @see vTaskDelay()
 */
void sw1_task(void *pvParameters) {
    (void)pvParameters;

    while (1) {
        
        sw1_pressed = gpio_get(SW1_PIN);
        vTaskDelay(10);
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

    // Test SW1 
    init_sw1();

    //Test SW2
    init_sw2();

    //Test RGB
    init_rgb_led();

    // Initialize Buzzer
    init_buzzer();
   
    // Initialize I2C
    init_i2c_default();

    //Initialize the microphone
    //Microhpone test in test_microphone.c

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
    float ax, ay, az, gx, gy, gz, t;

    while(true){
        
        if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {
            
            printf("Accel: X=%f, Y=%f, Z=%f | Gyro: X=%f, Y=%f, Z=%f| Temp: %2.2f°C\n", ax, ay, az, gx, gy, gz, t);

        } else {
            printf("Failed to read imu data\n");
        }
        sleep_ms(1000);

    }// end infinite loop
    

    // Create tasks
    
    xTaskCreate(sw2_task, "SW2Task", 256, NULL, 1, NULL);
    xTaskCreate(rgb_task, "RGBTask", 256, NULL, 1, NULL);
    xTaskCreate(buzzer_task, "BuzzerTask", 256, NULL, 4, NULL);
    xTaskCreate(imu_task, "IMUTask", 256, NULL, 1, NULL);
    xTaskCreate(sw1_task, "SW1Task", 64, NULL, 1, NULL);
    
    // Start the FreeRTOS scheduler
    vTaskStartScheduler();

    return 0;
}

