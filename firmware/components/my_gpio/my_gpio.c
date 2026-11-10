#include "my_gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MY GPIO";

// ── Stored callbacks ─────────────────────────────────────────
static gpio_callback_t sos_callback     = NULL;
static gpio_callback_t touch_callback   = NULL;
static gpio_callback_t bmi270_callback  = NULL;
static gpio_callback_t axp2101_callback = NULL;

// ── SOS hold detection task ──────────────────────────────────
// Runs in background, checks if SOS button held for 3 seconds

static void sos_hold_task(void *arg) {
    while (1) {
        // Wait for button press (pin goes LOW when pressed)
        if (gpio_get_level(PIN_SOS_BUTTON) == 0) {
            vTaskDelay(pdMS_TO_TICKS(SOS_HOLD_MS));

            // Still held after 3 seconds?
            if (gpio_get_level(PIN_SOS_BUTTON) == 0) {
                if (sos_callback) {
                    sos_callback();   // fire the SOS event
                    ESP_LOGI(TAG, "SOS triggered");
                }

                // Wait for release before checking again
                while (gpio_get_level(PIN_SOS_BUTTON) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));   // check every 50ms
    }
}

// ── Interrupt handlers ───────────────────────────────────────
// These are ISRs — keep them short, just fire the callback

static void IRAM_ATTR touch_isr_handler(void *arg) {
    if (touch_callback) touch_callback();
}

static void IRAM_ATTR bmi270_isr_handler(void *arg) {
    if (bmi270_callback) bmi270_callback();
}

static void IRAM_ATTR axp2101_isr_handler(void *arg) {
    if (axp2101_callback) axp2101_callback();
}

// ── Init ─────────────────────────────────────────────────────

void gpio_driver_init(void) {

    // ── Output pins ──────────────────────────────────────────
    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << PIN_DISPLAY_DC)  |
                        (1ULL << PIN_DISPLAY_RST) |
                        (1ULL << PIN_DISPLAY_CS),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };
    gpio_config(&out_conf);

    // Start display CS HIGH (not selected) and RST HIGH (not in reset)
    gpio_set_level(PIN_DISPLAY_CS,  1);
    gpio_set_level(PIN_DISPLAY_RST, 1);
    gpio_set_level(PIN_DISPLAY_DC,  0);

    ESP_LOGI(TAG, "Output pins configured");

    // ── SOS button ───────────────────────────────────────────
    gpio_config_t sos_conf = {
        .pin_bit_mask = (1ULL << PIN_SOS_BUTTON),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,    // HIGH when not pressed
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE      // handled by task not ISR
    };
    gpio_config(&sos_conf);

    // Start background task to watch for 3 second hold
    xTaskCreate(sos_hold_task, "sos_task", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "SOS button configured");

    // ── Interrupt input pins ─────────────────────────────────
    gpio_config_t int_conf = {
        .pin_bit_mask = (1ULL << PIN_TOUCH_INT)   |
                        (1ULL << PIN_BMI270_INT)  |
                        (1ULL << PIN_AXP2101_IRQ),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE        // trigger on falling edge
    };
    gpio_config(&int_conf);

    // Install interrupt service and attach handlers
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_TOUCH_INT,   touch_isr_handler,   NULL);
    gpio_isr_handler_add(PIN_BMI270_INT,  bmi270_isr_handler,  NULL);
    gpio_isr_handler_add(PIN_AXP2101_IRQ, axp2101_isr_handler, NULL);

    ESP_LOGI(TAG, "Interrupt pins configured");
}

// ── Output helpers ────────────────────────────────────────────

void gpio_driver_set_high(gpio_num_t pin) {
    gpio_set_level(pin, 1);
}

void gpio_driver_set_low(gpio_num_t pin) {
    gpio_set_level(pin, 0);
}

// ── Input helper ─────────────────────────────────────────────

int gpio_driver_read(gpio_num_t pin) {
    return gpio_get_level(pin);
}

// ── Callback registration ─────────────────────────────────────

void gpio_driver_register_sos_callback(gpio_callback_t cb) {
    sos_callback = cb;
}

void gpio_driver_register_touch_callback(gpio_callback_t cb) {
    touch_callback = cb;
}

void gpio_driver_register_bmi270_callback(gpio_callback_t cb) {
    bmi270_callback = cb;
}

void gpio_driver_register_axp2101_callback(gpio_callback_t cb) {
    axp2101_callback = cb;
}