/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zephyr/device.h>
#include <zephyr/sys/slist.h>

struct kscan_74hc165_listener;

/*
 * Called from the scan work item after every scan. `bits` holds the raw
 * (undebounced) inputs, normalised so that 1 means active.
 */
typedef void (*kscan_74hc165_listener_cb)(struct kscan_74hc165_listener *listener,
                                          const uint8_t *bits);

struct kscan_74hc165_listener {
    sys_snode_t node;
    kscan_74hc165_listener_cb callback;
    /* Bits consumed by the listener; these are not reported as keys. */
    uint16_t claimed_bits[2];
    uint8_t num_claimed_bits;
};

int kscan_74hc165_add_listener(const struct device *dev, struct kscan_74hc165_listener *listener);

static inline bool kscan_74hc165_bit(const uint8_t *bits, uint16_t bit) {
    return bits[bit / 8] & BIT(bit % 8);
}
