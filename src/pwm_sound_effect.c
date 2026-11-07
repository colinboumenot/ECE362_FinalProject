#include "pwm_sound_effect.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

// GPIO 21 = PWM slice 2, channel B
// Free pin: not used by LCD (27,29,30,32,33), I2C (4,5),
//           keypad (2-9), 7-seg (10-20), or ADC/joystick (26-28)
#define AUDIO_PIN 21

// ─── Low-level helpers ───────────────────────────────────────────────────────

// Set PWM output frequency.  Pass freq_hz = 0 to silence output.
static void set_freq(uint32_t freq_hz) {
    uint slice = pwm_gpio_to_slice_num(AUDIO_PIN);

    if (freq_hz == 0) {
        pwm_set_enabled(slice, false);
        pwm_set_gpio_level(AUDIO_PIN, 0);   // duty = 0 → silent
        return;
    }

    // We want:  f_sys / (clkdiv * (wrap + 1)) = freq_hz
    // Fix wrap = 999 and solve for clkdiv (float div supported by SDK).
    uint32_t f_sys = clock_get_hz(clk_sys);  // typically 125 000 000
    uint32_t wrap  = 999;
    float    div   = (float)f_sys / ((float)freq_hz * (wrap + 1));

    // Clamp to hardware limits (1.0 – 255.9375)
    if (div < 1.0f)   div = 1.0f;
    if (div > 255.9f) div = 255.9f;

    pwm_set_clkdiv(slice, div);
    pwm_set_wrap(slice, wrap);
    pwm_set_gpio_level(AUDIO_PIN, wrap / 2);  // 50 % duty cycle
    pwm_set_enabled(slice, true);
}

// ─── Public API ──────────────────────────────────────────────────────────────

void sound_init(void) {
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);

    uint slice = pwm_gpio_to_slice_num(AUDIO_PIN);
    pwm_config cfg = pwm_get_default_config();
    pwm_init(slice, &cfg, true);       // start enabled, output silent
    pwm_set_gpio_level(AUDIO_PIN, 0);  // duty = 0 until first tone
}

void sound_play_tone(uint32_t freq_hz, uint32_t duration_ms) {
    set_freq(freq_hz);
    if (duration_ms > 0) {
        sleep_ms(duration_ms);
        set_freq(0);
    }
}

void sound_stop(void) {
    set_freq(0);
}

// ─── Game sound effects ──────────────────────────────────────────────────────

// Jump: quick rising two-note chirp (440 Hz → 880 Hz)
void sound_jump(void) {
    sound_play_tone(440, 60);
    sound_play_tone(880, 60);
}

// Die: three descending tones
void sound_die(void) {
    sound_play_tone(600, 120);
    sound_play_tone(400, 120);
    sound_play_tone(200, 200);
}

// Milestone (every 100 pts): short happy jingle
void sound_milestone(void) {
    sound_play_tone(523, 80);   // C5
    sound_play_tone(659, 80);   // E5
    sound_play_tone(784, 120);  // G5
}

// Crouch: quick downward blip
void sound_crouch(void) {
    sound_play_tone(600, 40);
    sound_play_tone(400, 40);
}