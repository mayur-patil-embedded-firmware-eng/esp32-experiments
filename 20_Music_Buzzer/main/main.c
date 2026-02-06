#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/ledc.h"

/*
Happy Birthday Song Buzzer
*/

/* ================= CONFIG ================= */
#define BUZZER_GPIO   18

#define LEDC_TIMER    LEDC_TIMER_0
#define LEDC_MODE     LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL  LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_10_BIT
#define LEDC_DUTY     512   // 50%

/* ================= NOTES (Hz) ================= */
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784

/* ================= HAPPY BIRTHDAY ================= */
static const int melody[] = {
    NOTE_G4, NOTE_G4, NOTE_A4, NOTE_G4, NOTE_C5, NOTE_B4,
    NOTE_G4, NOTE_G4, NOTE_A4, NOTE_G4, NOTE_D5, NOTE_C5,
    NOTE_G4, NOTE_G4, NOTE_G5, NOTE_E5, NOTE_C5, NOTE_B4, NOTE_A4,
    NOTE_F5, NOTE_F5, NOTE_E5, NOTE_C5, NOTE_D5, NOTE_C5
};

static const int note_duration[] = {
    400, 200, 600, 600, 600, 1200,
    400, 200, 600, 600, 600, 1200,
    400, 200, 600, 600, 600, 600, 1200,
    400, 200, 600, 600, 600, 1200
};

static volatile int play_enable = 0;

/* ================= BUZZER ================= */

static void buzzer_init(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz          = 1000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t channel_conf = {
        .gpio_num   = BUZZER_GPIO,
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&channel_conf);
}

static void buzzer_stop(void)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

static void play_tone(int freq, int duration_ms)
{
    if (!play_enable) return;

    ledc_set_freq(LEDC_MODE, LEDC_TIMER, freq);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    buzzer_stop();
    vTaskDelay(pdMS_TO_TICKS(50));
}

/* ================= TASKS ================= */

static void music_task(void *arg)
{
    const int notes = sizeof(melody) / sizeof(melody[0]);

    while (1) {
        if (play_enable) {
            for (int i = 0; i < notes && play_enable; i++) {
                play_tone(melody[i], note_duration[i]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void console_task(void *arg)
{
    int ch;

    printf("\nType:\n");
    printf("  1 → START music\n");
    printf("  0 → STOP music\n\n");

    while (1) {
        ch = getchar();   // ESP-IDF console (UART0 safe)

        if (ch == '1') {
            play_enable = 1;
            printf("Music START\n");
        } 
        else if (ch == '0') {
            play_enable = 0;
            buzzer_stop();
            printf("Music STOP\n");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ================= MAIN ================= */

void app_main(void)
{
    buzzer_init();
    xTaskCreate(music_task, "music_task", 4096, NULL, 5, NULL);
    xTaskCreate(console_task, "console_task", 2048, NULL, 5, NULL);
}
