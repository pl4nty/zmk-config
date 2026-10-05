/*
 * Copyright (c) 2026 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_kscan_74hc165

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/kscan.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk_kscan_74hc165.h>

LOG_MODULE_REGISTER(kscan_74hc165, CONFIG_KSCAN_LOG_LEVEL);

#define MAX_BYTES 16

struct kscan_74hc165_config {
    struct spi_dt_spec spi;
    uint8_t num_bytes;
    bool active_low;
    uint16_t poll_period_ms;
    uint8_t debounce_press_ms;
    uint8_t debounce_release_ms;
};

struct kscan_74hc165_data {
    const struct device *dev;
    kscan_callback_t callback;
    struct k_work_delayable work;
    bool enabled;
    sys_slist_t listeners;
    uint8_t raw[MAX_BYTES];
    uint8_t state[MAX_BYTES];
    uint8_t ignored[MAX_BYTES];
    uint8_t elapsed_ms[MAX_BYTES * 8];
};

static int kscan_74hc165_read(const struct device *dev) {
    const struct kscan_74hc165_config *cfg = dev->config;
    struct kscan_74hc165_data *data = dev->data;

    const struct spi_buf rx_buf = {.buf = data->raw, .len = cfg->num_bytes};
    const struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};

    int err = spi_read_dt(&cfg->spi, &rx);
    if (err) {
        return err;
    }

    if (cfg->active_low) {
        for (int i = 0; i < cfg->num_bytes; i++) {
            data->raw[i] = ~data->raw[i];
        }
    }

    return 0;
}

static void kscan_74hc165_debounce(const struct device *dev) {
    const struct kscan_74hc165_config *cfg = dev->config;
    struct kscan_74hc165_data *data = dev->data;

    for (int row = 0; row < cfg->num_bytes; row++) {
        uint8_t changed = (data->raw[row] ^ data->state[row]) & ~data->ignored[row];

        for (int col = 0; col < 8; col++) {
            uint8_t *elapsed = &data->elapsed_ms[row * 8 + col];

            if (!(changed & BIT(col))) {
                *elapsed = 0;
                continue;
            }

            bool pressed = data->raw[row] & BIT(col);
            uint8_t threshold = pressed ? cfg->debounce_press_ms : cfg->debounce_release_ms;

            *elapsed = MIN(*elapsed + cfg->poll_period_ms, UINT8_MAX);
            if (*elapsed < threshold) {
                continue;
            }

            *elapsed = 0;
            WRITE_BIT(data->state[row], col, pressed);
            LOG_DBG("Row %d col %d %s", row, col, pressed ? "pressed" : "released");
            if (data->callback) {
                data->callback(dev, row, col, pressed);
            }
        }
    }
}

static void kscan_74hc165_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct kscan_74hc165_data *data = CONTAINER_OF(dwork, struct kscan_74hc165_data, work);
    const struct device *dev = data->dev;
    const struct kscan_74hc165_config *cfg = dev->config;

    int err = kscan_74hc165_read(dev);
    if (err) {
        LOG_ERR("SPI read failed (%d)", err);
    } else {
        kscan_74hc165_debounce(dev);

        struct kscan_74hc165_listener *listener;
        SYS_SLIST_FOR_EACH_CONTAINER(&data->listeners, listener, node) {
            listener->callback(listener, data->raw);
        }
    }

    if (data->enabled) {
        k_work_reschedule(&data->work, K_MSEC(cfg->poll_period_ms));
    }
}

int kscan_74hc165_add_listener(const struct device *dev, struct kscan_74hc165_listener *listener) {
    const struct kscan_74hc165_config *cfg = dev->config;
    struct kscan_74hc165_data *data = dev->data;

    for (int i = 0; i < listener->num_claimed_bits; i++) {
        uint16_t bit = listener->claimed_bits[i];
        if (bit >= cfg->num_bytes * 8) {
            return -EINVAL;
        }
        data->ignored[bit / 8] |= BIT(bit % 8);
    }

    sys_slist_append(&data->listeners, &listener->node);
    return 0;
}

static int kscan_74hc165_configure(const struct device *dev, kscan_callback_t callback) {
    struct kscan_74hc165_data *data = dev->data;

    if (!callback) {
        return -EINVAL;
    }

    data->callback = callback;
    return 0;
}

static int kscan_74hc165_enable(const struct device *dev) {
    struct kscan_74hc165_data *data = dev->data;

    data->enabled = true;
    k_work_reschedule(&data->work, K_NO_WAIT);
    return 0;
}

static int kscan_74hc165_disable(const struct device *dev) {
    struct kscan_74hc165_data *data = dev->data;

    data->enabled = false;
    k_work_cancel_delayable(&data->work);
    return 0;
}

static int kscan_74hc165_init(const struct device *dev) {
    const struct kscan_74hc165_config *cfg = dev->config;
    struct kscan_74hc165_data *data = dev->data;

    if (!spi_is_ready_dt(&cfg->spi)) {
        LOG_ERR("SPI bus %s not ready", cfg->spi.bus->name);
        return -ENODEV;
    }

    data->dev = dev;
    sys_slist_init(&data->listeners);
    k_work_init_delayable(&data->work, kscan_74hc165_work_handler);

    return 0;
}

static const struct kscan_driver_api kscan_74hc165_api = {
    .config = kscan_74hc165_configure,
    .enable_callback = kscan_74hc165_enable,
    .disable_callback = kscan_74hc165_disable,
};

#define KSCAN_74HC165_INIT(n)                                                                      \
    BUILD_ASSERT(DT_INST_PROP(n, num_bytes) <= MAX_BYTES, "num-bytes is too large");               \
    BUILD_ASSERT(DT_INST_PROP(n, debounce_press_ms) <= UINT8_MAX &&                                \
                     DT_INST_PROP(n, debounce_release_ms) <= UINT8_MAX,                            \
                 "debounce times must be at most 255 ms");                                         \
                                                                                                   \
    static struct kscan_74hc165_data kscan_74hc165_data_##n;                                       \
                                                                                                   \
    static const struct kscan_74hc165_config kscan_74hc165_config_##n = {                          \
        .spi = SPI_DT_SPEC_INST_GET(n, SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB,    \
                                    0),                                                            \
        .num_bytes = DT_INST_PROP(n, num_bytes),                                                   \
        .active_low = DT_INST_PROP(n, active_low),                                                 \
        .poll_period_ms = DT_INST_PROP(n, poll_period_ms),                                         \
        .debounce_press_ms = DT_INST_PROP(n, debounce_press_ms),                                   \
        .debounce_release_ms = DT_INST_PROP(n, debounce_release_ms),                               \
    };                                                                                             \
                                                                                                   \
    DEVICE_DT_INST_DEFINE(n, kscan_74hc165_init, NULL, &kscan_74hc165_data_##n,                    \
                          &kscan_74hc165_config_##n, POST_KERNEL, CONFIG_KSCAN_INIT_PRIORITY,      \
                          &kscan_74hc165_api);

DT_INST_FOREACH_STATUS_OKAY(KSCAN_74HC165_INIT)
