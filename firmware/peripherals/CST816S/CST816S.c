#include "CST8165.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// ── Stored callback ───────────────────────────────────────────
static cst816s_touch_cb_t touch_callback = NULL;

// ── Touch event queue ─────────────────────────────────────────
// ISR puts events here, task reads them
// This keeps I2C reads out of the ISR context

static QueueHandle_t touch_queue = NULL;

// ── Gesture name lookup ───────────────────────────────────────

const char *cst816s_gesture_name(cst816s_gesture_t gesture) {
    switch (gesture) {
        case CST_GESTURE_SWIPE_UP:    return "Swipe Up";
        case CST_GESTURE_SWIPE_DOWN:  return "Swipe Down";
        case CST_GESTURE_SWIPE_LEFT:  return "Swipe Left";
        case CST_GESTURE_SWIPE_RIGHT: return "Swipe Right";
        case CST_GESTURE_SINGLE_TAP:  return "Single Tap";
        case CST_GESTURE_DOUBLE_TAP:  return "Double Tap";
        case CST_GESTURE_LONG_PRESS:  return "Long Press";
        default:                      return "None";
    }
}

// ── Touch processing task ─────────────────────────────────────
// Waits for signal from ISR then reads I2C
// I2C reads cannot happen inside ISR so we use a task

static void touch_task(void *arg) {
    uint8_t signal;

    while (1) {
        // Block here until ISR sends a signal
        if (xQueueReceive(touch_queue,
                          &signal,
                          portMAX_DELAY) == pdTRUE) {

            cst816s_touch_t touch = {0};
            esp_err_t ret = cst816s_read(&touch);

            if (ret == ESP_OK && touch.touched) {
                LOG_INFO(TAG_TOUCH,
                         "%-12s  X=%3d  Y=%3d",
                         cst816s_gesture_name(touch.gesture),
                         touch.x, touch.y);

                if (touch_callback) {
                    touch_callback(touch);
                }
            }
        }
    }
}

// ── Init ──────────────────────────────────────────────────────

esp_err_t cst816s_init(void) {

    // Brief reset pulse via power cycling handled by AXP2101
    // CST816S needs a clean power-on to initialize correctly
    vTaskDelay(pdMS_TO_TICKS(50));

    // Read chip ID
    uint8_t id = 0;
    esp_err_t ret = i2c_driver_read_byte(CST816S_ADDR,
                                          CST_REG_CHIP_ID,
                                          &id);
    if (ret != ESP_OK) {
        LOG_ERROR(TAG_TOUCH, "CST816S not found on I2C bus");
        return ret;
    }

    if (id != CST816S_CHIP_ID) {
        LOG_ERROR(TAG_TOUCH,
                  "Wrong chip ID 0x%02X expected 0x%02X",
                  id, CST816S_CHIP_ID);
        return ESP_ERR_NOT_FOUND;
    }

    // Read firmware version
    uint8_t fw = 0;
    i2c_driver_read_byte(CST816S_ADDR, CST_REG_FIRMWARE, &fw);
    LOG_INFO(TAG_TOUCH,
             "CST816S found — ID 0x%02X  FW 0x%02X",
             id, fw);

    // Configure IRQ pulse width
    // 10 = pulse stays low for 10ms giving ESP32 time to wake
    i2c_driver_write_byte(CST816S_ADDR,
                           CST_REG_IRQ_PULSE, 10);

    // Create touch event queue
    touch_queue = xQueueCreate(10, sizeof(uint8_t));
    if (touch_queue == NULL) {
        LOG_ERROR(TAG_TOUCH, "Failed to create touch queue");
        return ESP_ERR_NO_MEM;
    }

    // Start touch processing task
    xTaskCreate(touch_task,
                "touch_task",
                2048, NULL, 6, NULL);   // high priority

    LOG_INFO(TAG_TOUCH, "CST816S initialized");
    return ESP_OK;
}

// ── Read touch data ───────────────────────────────────────────

esp_err_t cst816s_read(cst816s_touch_t *touch) {

    // Read 6 bytes starting from gesture register
    uint8_t buf[6] = {0};
    esp_err_t ret = i2c_driver_read(CST816S_ADDR,
                                     CST_REG_GESTURE,
                                     buf, 6);
    if (ret != ESP_OK) return ret;

    touch->gesture = (cst816s_gesture_t)buf[0];
    touch->fingers = buf[1];

    // X is upper 4 bits of buf[2] combined with buf[3]
    touch->x = ((uint16_t)(buf[2] & 0x0F) << 8) | buf[3];

    // Y is upper 4 bits of buf[4] combined with buf[5]
    touch->y = ((uint16_t)(buf[4] & 0x0F) << 8) | buf[5];

    // Clamp to display bounds
    if (touch->x > 239) touch->x = 239;
    if (touch->y > 239) touch->y = 239;

    touch->touched = (touch->fingers > 0);

    return ESP_OK;
}

// ── Handle interrupt ──────────────────────────────────────────
// Called from GPIO ISR — must be fast
// Just signals the touch task, no I2C here

void cst816s_handle_interrupt(void) {
    uint8_t signal = 1;
    BaseType_t woken = pdFALSE;

    // Send to queue from ISR context
    xQueueSendFromISR(touch_queue, &signal, &woken);

    // Yield to higher priority task if it was waiting
    if (woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

// ── Register callback ─────────────────────────────────────────

void cst816s_register_callback(cst816s_touch_cb_t cb) {
    touch_callback = cb;
}

// ── Sleep ─────────────────────────────────────────────────────

esp_err_t cst816s_sleep(void) {
    LOG_INFO(TAG_TOUCH, "CST816S sleep");
    return i2c_driver_write_byte(CST816S_ADDR,
                                  CST_REG_SLEEP, 0x03);
}