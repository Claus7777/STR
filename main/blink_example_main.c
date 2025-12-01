#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/timers.h>
#include <driver/gpio.h>
#include <stdio.h>
#include "esp_timer.h"


#define LED_PIN GPIO_NUM_2
#define BUZZER_PIN GPIO_NUM_4
#define BUTTON_UP_PIN GPIO_NUM_18
#define BUTTON_DOWN_PIN GPIO_NUM_19

// Constantes
#define MIN_BPM 40
#define MAX_BPM 240
#define DEFAULT_BPM 120
uint64_t beat_count = 0;

QueueHandle_t bpm_queue;
QueueHandle_t bpm_display_queue;
QueueHandle_t bpm_metronome_queue;

typedef struct {
    int bpm;
    bool update_display;
} metronome_state_t;

void metronome_task(void *pvParameter) {
    metronome_state_t state = {DEFAULT_BPM, false};
    int interval_ms = 60000 / state.bpm; 

    uint64_t start_time_us = esp_timer_get_time();
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while(1) {

        metronome_state_t new_state;
        if(xQueueReceive(bpm_metronome_queue, &new_state, 0) == pdTRUE) {
            state.bpm = new_state.bpm;
            interval_ms = 60000 / state.bpm;
            printf("BPM atualiado para: %d\n", state.bpm);
        }
        
        uint64_t current_time_us = esp_timer_get_time();
        uint64_t elapsed_us = current_time_us - start_time_us;
        double elapsed_ms = elapsed_us / 1000.0;
        
        
        printf("Batida: %.2f ms | Intervalo: %d ms | BPM: %d\n", 
              elapsed_ms, interval_ms, state.bpm);
        

        gpio_set_level(LED_PIN, 1);
        gpio_set_level(BUZZER_PIN, 1);
        

        vTaskDelay(pdMS_TO_TICKS(50));
        
        gpio_set_level(LED_PIN, 0);
        gpio_set_level(BUZZER_PIN, 0);
        

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(interval_ms));
    }
}


void button_task(void *pvParameter) {
    metronome_state_t state = {DEFAULT_BPM, true};
    bool last_up_state = false;
    bool last_down_state = false;
    uint64_t start_time_us = esp_timer_get_time();
    
    while(1) {
        bool up_pressed = (gpio_get_level(BUTTON_UP_PIN) == 0);
        bool down_pressed = (gpio_get_level(BUTTON_DOWN_PIN) == 0);
        uint64_t current_time_us = esp_timer_get_time();
        uint64_t elapsed_us = current_time_us - start_time_us;
        double elapsed_ms = elapsed_us / 1000.0;
        
        if(up_pressed && !last_up_state) {
            state.bpm += 5;
            if(state.bpm > MAX_BPM) state.bpm = MAX_BPM;
            state.update_display = true;

            xQueueSend(bpm_display_queue, &state, portMAX_DELAY);
            xQueueSend(bpm_metronome_queue, &state, portMAX_DELAY);
            printf("Apertado! %.2f\n", elapsed_ms);
        }
        
        if(down_pressed && !last_down_state) {
            state.bpm -= 5;
            if(state.bpm < MIN_BPM) state.bpm = MIN_BPM;
            state.update_display = true;

            xQueueSend(bpm_display_queue, &state, portMAX_DELAY);
            xQueueSend(bpm_metronome_queue, &state, portMAX_DELAY);

            printf("Apertado! %.2f\n", elapsed_ms);
        }

        
        last_up_state = up_pressed;
        last_down_state = down_pressed;
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


void display_task(void *pvParameter) {
    metronome_state_t state = {DEFAULT_BPM, false};
    
    while(1) {
        if(xQueueReceive(bpm_display_queue, &state, portMAX_DELAY) == pdTRUE) {
            if(state.update_display) {
                printf("\rMetronomo: %d BPM    ", state.bpm);
                fflush(stdout);

                UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
                printf(" | Stack Livre (Display): %u \n", uxHighWaterMark * sizeof(StackType_t));
            }
        }
    }
}


void setup_gpio() {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    io_conf.pin_bit_mask = (1ULL << BUZZER_PIN);
    gpio_config(&io_conf);
    

    io_conf.pin_bit_mask = (1ULL << BUTTON_UP_PIN) | (1ULL << BUTTON_DOWN_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
}

void app_main() {
    setup_gpio();
    
    bpm_queue = xQueueCreate(10, sizeof(metronome_state_t));
    bpm_display_queue = xQueueCreate(10, sizeof(metronome_state_t));
    bpm_metronome_queue = xQueueCreate(10, sizeof(metronome_state_t));
    
    xTaskCreate(metronome_task, "metronome", 2048, NULL, 3, NULL);
    xTaskCreate(button_task, "buttons", 2048, NULL, 2, NULL);
    xTaskCreate(display_task, "display", 2048, NULL, 1, NULL);
    
    printf("Metronomo ESP32 Iniciado!\n");
    printf("Use o botão verde para aumentar e o vermelho para diminuir o BPM (%d-%d)\n", MIN_BPM, MAX_BPM);
}