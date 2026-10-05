/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>

/* USB-C CC1/CC2 sense pins; QMK drives both low at boot, so do the same. */
#define ELORA_CC1_PIN 28
#define ELORA_CC2_PIN 29

static int splitkb_elora_rev1_init(void) {
    const struct device *gpio = DEVICE_DT_GET(DT_NODELABEL(gpio0));

    if (!device_is_ready(gpio)) {
        return -ENODEV;
    }

    gpio_pin_configure(gpio, ELORA_CC1_PIN, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio, ELORA_CC2_PIN, GPIO_OUTPUT_LOW);

    return 0;
}

SYS_INIT(splitkb_elora_rev1_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
