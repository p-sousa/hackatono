#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/types.h>

#include "main.h"

// Thread stack size
#define TEMP_THREAD_STACK_SIZE 1024
#define DT_DRV_COMPAT pressure

struct i2c_dt_spec i2c = I2C_DT_SPEC_GET(DT_NODELABEL(pressure_sensor));

enum direction {
  DOWN,
  UP,
};

void temp_thread(void) {
  uint8_t sensor_data[3];

  if (!device_is_ready(i2c.bus)) {
    printk("Motor controller I2C bus not ready.\n");
    return;
  }

  int16_t pressure;
  // bool direction = UP;
  while (1) {

    //   if (pressure > 8000) {
    //     direction = DOWN;
    //   } else if (pressure < -8000) {
    //     direction = UP;
    //   }

    //   if (direction == UP) {
    //     pressure += 100;
    //   } else {
    //     pressure -= 100;
    //   }

    // read from the sensor
    k_sleep(K_MSEC(10));

    i2c_read_dt(&i2c, sensor_data, sizeof(sensor_data));
    pressure = (((sensor_data[0] << 8) | sensor_data[1]) - 8192) * -3;
    bt_send_fsr_notification(pressure);

    printk("Pressure: %d\n", pressure);
  }
}

// main ACC_thread
K_THREAD_DEFINE(temp_id, TEMP_THREAD_STACK_SIZE, temp_thread, NULL, NULL, NULL,
                6, 0, 0);
