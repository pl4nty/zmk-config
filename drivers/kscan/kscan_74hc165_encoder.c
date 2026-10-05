/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_kscan_74hc165_encoder

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk_kscan_74hc165.h>

LOG_MODULE_REGISTER(kscan_74hc165_encoder, CONFIG_SENSOR_LOG_LEVEL);

#define FULL_ROTATION 360

struct encoder_74hc165_config {
    const struct device *kscan;
    uint16_t a_bit;
    uint16_t b_bit;
    int32_t steps;
};

struct encoder_74hc165_data {
    const struct device *dev;
    struct kscan_74hc165_listener listener;
    sensor_trigger_handler_t handler;
    const struct sensor_trigger *trigger;
    bool initialized;
    uint8_t ab_state;
    int32_t pulses;
};

static void encoder_74hc165_update(struct kscan_74hc165_listener *listener, const uint8_t *bits) {
    struct encoder_74hc165_data *data =
        CONTAINER_OF(listener, struct encoder_74hc165_data, listener);
    const struct encoder_74hc165_config *cfg = data->dev->config;

    uint8_t val = (kscan_74hc165_bit(bits, cfg->a_bit) << 1) | kscan_74hc165_bit(bits, cfg->b_bit);

    if (!data->initialized) {
        data->ab_state = val;
        data->initialized = true;
        return;
    }

    int8_t delta;
    switch (val | (data->ab_state << 2)) {
    case 0b0010:
    case 0b0100:
    case 0b1101:
    case 0b1011:
        delta = -1;
        break;
    case 0b0001:
    case 0b0111:
    case 0b1110:
    case 0b1000:
        delta = 1;
        break;
    default:
        delta = 0;
        break;
    }

    data->ab_state = val;

    if (delta == 0) {
        return;
    }

    data->pulses += delta;

    if (data->handler) {
        data->handler(data->dev, data->trigger);
    }
}

static int encoder_74hc165_sample_fetch(const struct device *dev, enum sensor_channel chan) {
    if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_ROTATION) {
        return -ENOTSUP;
    }

    /* Pulses are accumulated by the scan listener. */
    return 0;
}

static int encoder_74hc165_channel_get(const struct device *dev, enum sensor_channel chan,
                                       struct sensor_value *val) {
    const struct encoder_74hc165_config *cfg = dev->config;
    struct encoder_74hc165_data *data = dev->data;

    if (chan != SENSOR_CHAN_ROTATION) {
        return -ENOTSUP;
    }

    int32_t pulses = data->pulses;
    data->pulses = 0;

    val->val1 = (pulses * FULL_ROTATION) / cfg->steps;
    val->val2 = (pulses * FULL_ROTATION) % cfg->steps;
    if (val->val2 != 0) {
        val->val2 *= 1000000;
        val->val2 /= cfg->steps;
    }

    return 0;
}

static int encoder_74hc165_trigger_set(const struct device *dev, const struct sensor_trigger *trig,
                                       sensor_trigger_handler_t handler) {
    struct encoder_74hc165_data *data = dev->data;

    data->trigger = trig;
    data->handler = handler;
    return 0;
}

static const struct sensor_driver_api encoder_74hc165_api = {
    .trigger_set = encoder_74hc165_trigger_set,
    .sample_fetch = encoder_74hc165_sample_fetch,
    .channel_get = encoder_74hc165_channel_get,
};

static int encoder_74hc165_init(const struct device *dev) {
    const struct encoder_74hc165_config *cfg = dev->config;
    struct encoder_74hc165_data *data = dev->data;

    if (!device_is_ready(cfg->kscan)) {
        LOG_ERR("kscan device not ready");
        return -ENODEV;
    }

    data->dev = dev;
    data->listener.callback = encoder_74hc165_update;
    data->listener.claimed_bits[0] = cfg->a_bit;
    data->listener.claimed_bits[1] = cfg->b_bit;
    data->listener.num_claimed_bits = 2;

    return kscan_74hc165_add_listener(cfg->kscan, &data->listener);
}

#define ENCODER_74HC165_INIT(n)                                                                    \
    BUILD_ASSERT(DT_INST_PROP(n, steps) > 0, "steps must be positive");                            \
                                                                                                   \
    static struct encoder_74hc165_data encoder_74hc165_data_##n;                                   \
                                                                                                   \
    static const struct encoder_74hc165_config encoder_74hc165_config_##n = {                      \
        .kscan = DEVICE_DT_GET(DT_INST_PHANDLE(n, kscan)),                                         \
        .a_bit = DT_INST_PROP(n, a_bit),                                                           \
        .b_bit = DT_INST_PROP(n, b_bit),                                                           \
        .steps = DT_INST_PROP(n, steps),                                                           \
    };                                                                                             \
                                                                                                   \
    DEVICE_DT_INST_DEFINE(n, encoder_74hc165_init, NULL, &encoder_74hc165_data_##n,                \
                          &encoder_74hc165_config_##n, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,   \
                          &encoder_74hc165_api);

DT_INST_FOREACH_STATUS_OKAY(ENCODER_74HC165_INIT)
