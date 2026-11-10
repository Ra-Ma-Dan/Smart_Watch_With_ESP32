#include "my_uart.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
    
static esp_log_level_t to_esp_level(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return ESP_LOG_DEBUG; //
        case LOG_LEVEL_INFO:  return ESP_LOG_INFO;  //
        case LOG_LEVEL_WARN:  return ESP_LOG_WARN;  //
        case LOG_LEVEL_ERROR: return ESP_LOG_ERROR; //
        default:              return ESP_LOG_INFO;  //
    }
}

// ── Init ──────────────────────────────────────────────────────
void uart_driver_init(log_level_t level) {
    // Global master switch override assignment
    esp_log_level_set("*", to_esp_level(level));
    
    LOG_INFO(TAG_SYSTEM, "================================");
    LOG_INFO(TAG_SYSTEM, "   AH Smartwatch Firmware");
    LOG_INFO(TAG_SYSTEM, "   ESP32-S3 — Debug via USB-C");
    LOG_INFO(TAG_SYSTEM, "================================");
}

// ── Change log level at runtime ───────────────────────────────
// Useful to switch to DEBUG when investigating a problem
void uart_driver_set_level(log_level_t level) {
    esp_log_level_set("*", to_esp_level(level));
    LOG_INFO(TAG_SYSTEM, "Global hardware framework log level shifted to: %d", level);
}

// ── Print useful system info on startup ───────────────────────
void uart_driver_log_system_info(void) {
    esp_chip_info_t chip;
    esp_chip_info(&chip); //
    
    uint32_t flash_size = 0;
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        LOG_INFO(TAG_SYSTEM, "Flash Size: %lu MB", flash_size / (1024 * 1024));
    }
    
    LOG_INFO(TAG_SYSTEM, "Silicon Core Topology: %s [Cores: %d, Rev: %d]", "ESP32-S3", chip.cores, chip.revision);
    LOG_INFO(TAG_SYSTEM, "Free Active Application Heap Profile: %lu bytes", (unsigned long)esp_get_free_heap_size());
}

void uart_driver_log_full_system_info(void) {
    
    // Chip info
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    LOG_INFO(TAG_SYSTEM, "Chip:    ESP32-S3");
    LOG_INFO(TAG_SYSTEM, "Cores:   %d", chip.cores);
    LOG_INFO(TAG_SYSTEM, "Rev:     %d", chip.revision);
    
    // Flash size
    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);
    LOG_INFO(TAG_SYSTEM, "Flash:   %lu MB", flash_size / (1024 * 1024));
    
    // Free heap
    LOG_INFO(TAG_SYSTEM, "Heap:    %lu bytes free",
        esp_get_free_heap_size());
        
        // FreeRTOS tick rate
        LOG_INFO(TAG_SYSTEM, "Tick:    %d Hz", configTICK_RATE_HZ);
        
        LOG_INFO(TAG_SYSTEM, "");
    }