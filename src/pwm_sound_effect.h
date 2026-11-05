#ifndef PWM_SOUND_EFFECT_H
#define PWM_SOUND_EFFECT_H

#include <stdint.h>

// Initialize PWM on GPIO 21 for audio output
void sound_init(void);

// Play a tone at given frequency (Hz) for given duration (ms)
// Pass freq=0 to stop sound immediately
void sound_play_tone(uint32_t freq_hz, uint32_t duration_ms);

// Stop any currently playing sound
void sound_stop(void);

// Game event sound effects
void sound_jump(void);       // Short rising beep when dinosaur jumps
void sound_die(void);        // Descending tones on game over
void sound_milestone(void);  // Cheerful beep every 100 points
void sound_crouch(void);     // Short falling beep when crouching

#endif // PWM_SOUND_EFFECT_H