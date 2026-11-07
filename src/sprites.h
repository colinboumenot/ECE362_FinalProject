#ifndef SPRITES_H
#define SPRITES_H

#include <stdint.h>
#include "lcd.h"

#define DINO_W 40
#define DINO_W_CROUCH 56
#define BIRD_W 42
#define CACTUS_W_LARGE 23
#define CACTUS_W_SMALL 15

#define DINO_H_STAND 44
#define DINO_H_CROUCH 26
#define BIRD_H 27
#define CACTUS_H_LARGE 46
#define CACTUS_H_SMALL 33

#define DINO_FRAME_COUNT 4
#define DINO_CROUCH_FRAMES 2
#define BIRD_FRAMES 2
#define CACTUS_FRAMES 1

// Sprite is a collection of frames
typedef struct {
    Picture *frames;
    uint8_t frame_count;
    int16_t w, h;
} Sprite;

// Converts 'x' '.' character art into pixels and a sprite
// s - Sprite to fill in
// frames - Picture array
// px - Pixel buffer
// frame_count - Number of frames for sprite
// h, w - Height and width of each frame (Must be the same for each frame)
// art - Character art array
void sprite_build(Sprite *s, Picture *frames, uint16_t *px, int frame_count, int h, int w, const char *art);

#endif
