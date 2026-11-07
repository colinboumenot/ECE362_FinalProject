#include "sprites.h"

void sprite_build(Sprite *s, Picture *frames, uint16_t *px, int frame_count, int h, int w, const char *art)
{
    int area = w * h;
    int row_len = w + 1;
    int frame_len = h * row_len;
    
    // For each frame convert characters into WHITE or BLACK
    for (int f = 0; f < frame_count; f++)
    {
        const char *frame_art = art + f * frame_len;
        uint16_t *frame_px = px + f * area;

        for (int r = 0; r < h; r++)
        {
            for (int c = 0; c < w; c++)
            {
                frame_px[r * w + c] = (frame_art[r * row_len + c] == 'x') ? BLACK : WHITE;
            }
        }
        
        // LCD Picture properties
        frames[f].width = w;
        frames[f].height = h;
        frames[f].bytes_per_pixel = 2;
        frames[f].pixel_data = (unsigned char *)frame_px;
    }

    // Sprite Properties
    s->frames = frames;
    s->frame_count = frame_count;
    s->w = w;
    s->h = h;
}

