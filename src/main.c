/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/bluetooth/services/hrs.h>
#include <zephyr/bluetooth/uuid.h>

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_HRS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL),
                  BT_UUID_16_ENCODE(BT_UUID_DIS_VAL))};

bool fsr_notification_on = false;

/**
 * @brief UUID value for the Bloom Pod Force data characteristic
 * UUID: f93f1b1a-b069-437d-adb8-11e58aa15a6b
 */
#define BLOOM_POD_FORCE_BT_UUID_VAL                                            \
  BT_UUID_128_ENCODE(0xf93f1b1a, 0xb069, 0x437d, 0xadb8, 0x11e58aa15a6b)

/**
 * @brief UUID for the Bloom Pod Force data characteristic
 * This is the BLE force characteristic that will be available on the final
 * product
 */
#define BLOOM_POD_FORCE_BT_UUID BT_UUID_DECLARE_128(BLOOM_POD_FORCE_BT_UUID_VAL)

#define BLOOM_POD_SENSORS_BT_UUID_PRIMARY_VAL                                  \
  BT_UUID_128_ENCODE(0xf93f1b19, 0xb069, 0x437d, 0xadb8, 0x11e58aa15a6b)

/**
 * @brief UUID for the Bloom Pod Sensors service
 * This is the service that will be available on the final product
 */
#define BLOOM_POD_SENSORS_BT_UUID_PRIMARY                                      \
  BT_UUID_DECLARE_128(BLOOM_POD_SENSORS_BT_UUID_PRIMARY_VAL)

/**
 * @brief UUID value for the Bloom Pod Force data characteristic
 * UUID: f93f1b1a-b069-437d-adb8-11e58aa15a6b
 */
#define BLOOM_POD_MOTOR_BT_UUID_VAL                                            \
  BT_UUID_128_ENCODE(0xf93f1b1c, 0xb069, 0x437d, 0xadb8, 0x11e58aa15a6b)

/**
 * @brief UUID for the Bloom Pod Force data characteristic
 * This is the BLE force characteristic that will be available on the final
 * product
 */
#define BLOOM_POD_MOTOR_BT_UUID BT_UUID_DECLARE_128(BLOOM_POD_MOTOR_BT_UUID_VAL)

/** @brief Callback function that detects fsr notifications have been
 * enabled.
 */
static void fsr_notification_ccc_cfg_changed(const struct bt_gatt_attr *attr,
                                             uint16_t value) {
  ARG_UNUSED(attr);
  fsr_notification_on = (value == BT_GATT_CCC_NOTIFY);
  if (fsr_notification_on) {
    // This characteristic is relevant for inactivity. Track the subscription
    printk("Pressure notifications enabled\n");
  } else {
    // This characteristic is relevant for inactivity. Track the unsubscription
    printk("Pressure notifications disabled\n");
  }
}

static ssize_t commands_write_cb(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len, uint16_t offset,
                                 uint8_t flags) {
  ARG_UNUSED(conn);

  printk("Received command: %s\n", (char *)buf);

  return 0;
}

/** @brief Callback function that detects fsr notifications have bee

/* Device Primary/Secondary services declaration */
BT_GATT_SERVICE_DEFINE(
    primary_svc_pod_sensors,
    BT_GATT_PRIMARY_SERVICE(BLOOM_POD_SENSORS_BT_UUID_PRIMARY),
    BT_GATT_CHARACTERISTIC(BLOOM_POD_FORCE_BT_UUID, BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(fsr_notification_ccc_cfg_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(BLOOM_POD_MOTOR_BT_UUID, BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE, NULL, commands_write_cb, NULL),

);

static void connected(struct bt_conn *conn, uint8_t err) {
  if (err) {
    printk("Connection failed (err 0x%02x)\n", err);
  } else {
    printk("Connected\n");
  }
}

// define a struct for the force sensor data
struct force_sensor_data {
  uint32_t fsr;
};

int bt_send_fsr_notification(uint32_t fsr) {
  int err = 0;
  if (fsr_notification_on == false) {
    return err;
  }

  // Send the encoded message
  err = bt_gatt_notify(NULL, &primary_svc_pod_sensors.attrs[2], &fsr,
                       sizeof(struct force_sensor_data));

  return err;
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
  printk("Disconnected (reason 0x%02x)\n", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void bt_ready(void) {
  int err;

  printk("Bluetooth initialized\n");

  err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
  if (err) {
    printk("Advertising failed to start (err %d)\n", err);
    return;
  }

  printk("Advertising successfully started\n");
}

static void auth_cancel(struct bt_conn *conn) {
  char addr[BT_ADDR_LE_STR_LEN];

  bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

  printk("Pairing cancelled: %s\n", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
    .cancel = auth_cancel,
};

static void bas_notify(void) {
  uint8_t battery_level = bt_bas_get_battery_level();

  battery_level--;

  if (!battery_level) {
    battery_level = 100U;
  }

  bt_bas_set_battery_level(battery_level);
}

int main(void) {
  int err;

  err = bt_enable(NULL);
  if (err) {
    printk("Bluetooth init failed (err %d)\n", err);
    return 0;
  }

  bt_ready();

  bt_conn_auth_cb_register(&auth_cb_display);

  /* Implement notification. At the moment there is no suitable way
   * of starting delayed work so we do it here
   */

  uint8_t test_pressure = 10;
  while (1) {
    test_pressure++;
    k_sleep(K_SECONDS(1));
    bt_send_fsr_notification(test_pressure);

    /* Battery level simulation */
    bas_notify();
  }
  return 0;
}
