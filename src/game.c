#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "lcd.h"
#include "queue.h"
#include "game.h"
#include "pwm_sound_effect.h"

#define GROUND_Y 200

// For debugging purposes
#define KEY_JUMP '*'
#define KEY_CROUCH '0'
#define KEY_RESTART 'C'

#define DINO_X 30
#define DINO_W 20
#define DINO_H_STAND 22
#define DINO_H_CROUCH 12
#define GRAVITY 1
#define JUMP_VEL_Y (-12)

#define DINO_FRAME_RUN_COUNT 3
#define DINO_FRAME_DEAD 3
#define DINO_FRAME_COUNT 4
#define ANIMATION_TICKS 4

#define CACTUS_W 14
#define CACTUS_H 34
#define CROW_W 24
#define CROW_H 14
#define CROW_Y (GROUND_Y - 34)

#define SPEED_MIN 4
#define SPEED_MAX 10

#define POINTS_PER_SECOND 9 // Based on real dinosaur game
#define MS_PER_SPEED_STEP 10000

#define JOY_THRESHOLD_HIGH 3500
#define JOY_THRESHOLD_LOW 200
#define HAS_CONTROLLER false

typedef enum {
    PLAYING,
    GAME_OVER
} GameState;

typedef struct {
    int16_t x0, y0, x1, y1;
    bool valid;
} Rect;

typedef struct {
    int16_t y;
    int16_t vel_y;
    int16_t h;
    bool on_ground;
} Dinosaur;

typedef struct {
    int16_t x, y, w, h;
    bool scored;
} Obstacle;

static GameState state;
static Dinosaur dinosaur;
static Obstacle obstacle;
static Rect dinosaur_prev, obstacle_prev;
static uint32_t score;
static int16_t speed;
static bool old_score;
static int16_t screen_w;
static int16_t screen_h;
static uint32_t start_time_us;

static uint8_t anim_frame;
static uint8_t anim_tick;

static bool jump_pend;
static bool restart_pend;
static bool crouch_pend;
static bool key_crouch; 

extern int adc_fifo_out;
uint8_t startup_buffer;

uint16_t dino_stand_px[DINO_FRAME_COUNT][DINO_W * DINO_H_STAND];
Picture dino_stand_pic[DINO_FRAME_COUNT];

Picture dino_crouch_pic[DINO_FRAME_RUN_COUNT];

char dino_art[DINO_FRAME_COUNT][DINO_H_STAND][DINO_W + 1] =
{
    {
        "...........xxxxxxxx.",
        "..........xxxxxxxxxx",
        "..........xx.xxxxxxx",
        "..........xxxxxxxxxx",
        "..........xxxxxxxxxx",
        "..........xxxxxxxxxx",
        "..........xxxxx.....",
        "..........xxxxxxxx..",
        "x........xxxxx......",
        "x.......xxxxxx......",
        "xx....xxxxxxxxxx....",
        "xxx..xxxxxxxxx.x....",
        "xxxxxxxxxxxxxx......",
        "xxxxxxxxxxxxxx......",
        ".xxxxxxxxxxxx.......",
        "..xxxxxxxxxxx.......",
        "...xxxxxxxxx........",
        "....xxxxxxx.........",
        ".....xxx.xx.........",
        ".....xx...x.........",
        ".....x....x.........",
        ".....xx...xx........"
    },
    {
        "...........xxxxxxxx.",
        "..........xxxxxxxxxx",
        "..........xx.xxxxxxx",
        "..........xxxxxxxxxx",
        "..........xxxxxxxxxx",
        "..........xxxxxxxxxx",
        "..........xxxxx.....",
        "..........xxxxxxxx..",
        "x........xxxxx......",
        "x.......xxxxxx......",
        "xx....xxxxxxxxxx....",
        "xxx..xxxxxxxxx.x....",
        "xxxxxxxxxxxxxx......",
        "xxxxxxxxxxxxxx......",
        ".xxxxxxxxxxxx.......",
        "..xxxxxxxxxxx.......",
        "...xxxxxxxxx........",
        "....xxxxxxx.........",
        ".....xxx..xxx.......",
        ".....xx.............",
        ".....x..............",
        ".....xx............."
    },
    {
        "...........xxxxxxxx.",
        "..........xxxxxxxxxx",
        "..........xx.xxxxxxx",
        "..........xxxxxxxxxx",
        "..........xxxxxxxxxx",
        "..........xxxxxxxxxx",
        "..........xxxxx.....",
        "..........xxxxxxxx..",
        "x........xxxxx......",
        "x.......xxxxxx......",
        "xx....xxxxxxxxxx....",
        "xxx..xxxxxxxxx.x....",
        "xxxxxxxxxxxxxx......",
        "xxxxxxxxxxxxxx......",
        ".xxxxxxxxxxxx.......",
        "..xxxxxxxxxxx.......",
        "...xxxxxxxxx........",
        "....xxxxxxx.........",
        ".....xx..xx.........",
        "......xx..x.........",
        "..........x.........",
        "..........xx........"
    },
    {
        "...........xxxxxxxx.",
        "..........xx...xxxxx",
        "..........xx.x.xxxxx",
        "..........xx...xxxxx",
        "..........xxxxxxxxxx",
        "..........xxxxxxxxxx",
        "..........xxxxx.....",
        "..........xxxxxxxx..",
        "x........xxxxx......",
        "x.......xxxxxx......",
        "xx....xxxxxxxxxx....",
        "xxx..xxxxxxxxx.x....",
        "xxxxxxxxxxxxxx......",
        "xxxxxxxxxxxxxx......",
        ".xxxxxxxxxxxx.......",
        "..xxxxxxxxxxx.......",
        "...xxxxxxxxx........",
        "....xxxxxxx.........",
        ".....xxx.xx.........",
        ".....xx...x.........",
        ".....x....x.........",
        ".....xx...xx........"
    }
};

static void build_sprites(void)
{
    for (int f = 0; f < DINO_FRAME_COUNT; f++)
    {
        for (int r = 0; r < DINO_H_STAND; r++)
        {
            for (int c = 0; c < DINO_W; c++)
            {
                dino_stand_px[f][r * DINO_W + c] = (dino_art[f][r][c] == 'x') ? BLACK : WHITE;
            }
        }
        dino_stand_pic[f].width = DINO_W;
        dino_stand_pic[f].height = DINO_H_STAND;
        dino_stand_pic[f].bytes_per_pixel = 2;
        dino_stand_pic[f].pixel_data = (unsigned char *)dino_stand_px[f];
    }

    for (int f = 0; f < DINO_FRAME_RUN_COUNT; f++)
    {
        int first_row = DINO_H_STAND - DINO_H_CROUCH;

        dino_crouch_pic[f].width = DINO_W;
        dino_crouch_pic[f].height = DINO_H_CROUCH;
        dino_crouch_pic[f].bytes_per_pixel = 2;
        dino_crouch_pic[f].pixel_data = (unsigned char *)&dino_stand_px[f][first_row * DINO_W];
    }
}

static void process_keys(void)
{
    while (key_available())
    {   
        uint16_t events = key_pop();
        bool pressed = (events & (1u << 8)) != 0;
        printf("%s %c\n",
                   (events & (1u << 8)) ? "Pressed" : "Released",
                   (char)(events & 0xFF));
        char key = (char)(events & 0xFF);

        if (key == KEY_JUMP && pressed)
        {
            jump_pend = true;
        }
        else if (key == KEY_RESTART && pressed)
        {
            restart_pend = true;
        }
        else if (key == KEY_CROUCH)
        {
            key_crouch = pressed;
        }
    }

    if (HAS_CONTROLLER)
    {
        if (startup_buffer > 0) startup_buffer--;
        else 
        {
            bool joy_jump = (adc_fifo_out > JOY_THRESHOLD_HIGH);
            bool joy_crouch = (adc_fifo_out < JOY_THRESHOLD_LOW);

            if (joy_jump)
            {
                jump_pend = true;
            }

            crouch_pend = key_crouch || joy_crouch;
        }

    }
    else
    {
        crouch_pend = key_crouch;
    }
}

static void fill_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    x0 = x0 < 0 ? 0 : x0;
    x1 = x1 > screen_w ? screen_w : x1;

    if (x0 >= x1 || y0 >= y1)
    {
        return;
    }

    LCD_DrawFillRectangle(x0, y0, x1 - 1, y1 - 1, color);
}

static void erase_old(Rect old, Rect new)
{
    if (!old.valid)
    {
        return;
    }

    if (old.y0 < new.y0) fill_rect(old.x0, old.y0, old.x1, new.y0, WHITE);
    if (old.y1 > new.y1) fill_rect(old.x0, new.y1, old.x1, old.y1, WHITE);
    if (old.x0 < new.x0) fill_rect(old.x0, old.y0, new.x0, old.y1, WHITE);
    if (old.x1 > new.x1) fill_rect(new.x1, old.y0, old.x1, old.y1, WHITE);
}

// static void erase_rect(Rect *to_erase)
// {
//     if (to_erase->valid)
//     {
//         fill_rect(to_erase->x0, to_erase->y0, to_erase->x1, to_erase->y1, WHITE);
//         to_erase->valid = false;
//     }
// }

static Rect dinosaur_rect(void)
{
    Rect rect = {DINO_X, dinosaur.y, DINO_X + DINO_W, dinosaur.y + dinosaur.h, true};
    return rect;
}

static Rect obstacle_rect(void)
{
    Rect rect = {obstacle.x, obstacle.y, obstacle.x + obstacle.w, obstacle.y + obstacle.h, true};
    return rect;
}

static void draw_dinosaur(void)
{
    uint8_t frame = dinosaur.on_ground ? anim_frame : 0;

    if (dinosaur.h == DINO_H_CROUCH)
    {
        LCD_DrawPicture(DINO_X, dinosaur.y, &dino_crouch_pic[frame]);
    }
    else
    {
        LCD_DrawPicture(DINO_X, dinosaur.y, &dino_stand_pic[frame]);
    }
}
static void draw_frame(void)
{
    Rect obs_new = obstacle_rect();
    fill_rect(obs_new.x0, obs_new.y0, obs_new.x1, obs_new.y1, BLACK);
    erase_old(obstacle_prev, obs_new);
    obstacle_prev = obs_new;

    Rect dino_new = dinosaur_rect();
    draw_dinosaur();
    erase_old(dinosaur_prev, dino_new);
    dinosaur_prev = dino_new;
}

static void draw_score(void)
{
    char buffer[16];
    fill_rect(0, 0, screen_w, 16, WHITE);
    snprintf(buffer, sizeof(buffer), "Score %lu", (unsigned long)score);
    LCD_DrawString(8, 4, BLACK, WHITE, buffer, 16, 0);
}

static void game_over(void)
{
    fill_rect(60, 90, 260, 140, WHITE);
    LCD_DrawString(124, 100, BLACK, WHITE, "GAME OVER", 16, 0);
    LCD_DrawString(88, 120, BLACK, WHITE, "Press C to Restart", 16, 0);
}

static void add_obstacle(int16_t x)
{
    obstacle.x = x;
    obstacle.scored = false;
    if (rand() % 5 == 0) // 1 in 5 obstacles are crows
    {
        obstacle.w = CROW_W;
        obstacle.h = CROW_H;
        obstacle.y = CROW_Y;
    }
    else
    {
        obstacle.w = CACTUS_W;
        obstacle.h = CACTUS_H;
        obstacle.y = GROUND_Y - CACTUS_H;
    }
}

static void update_dinosaur(bool jump, bool crouch)
{
    if (dinosaur.on_ground && jump)
    {
        dinosaur.vel_y = JUMP_VEL_Y;
        sound_jump();
        dinosaur.on_ground = false;
    }

    if (dinosaur.on_ground)
    {
        dinosaur.h = crouch ? DINO_H_CROUCH : DINO_H_STAND;
        dinosaur.y = GROUND_Y - dinosaur.h;
    }
    else
    {
        dinosaur.h = DINO_H_STAND;
        dinosaur.vel_y += GRAVITY;
        dinosaur.y += dinosaur.vel_y;

        if (dinosaur.y + dinosaur.h >= GROUND_Y)
        {
            dinosaur.y = GROUND_Y - dinosaur.h;
            dinosaur.vel_y = 0;
            dinosaur.on_ground = true;
        }
    }
}

static void update_animation(void)
{
    if (!dinosaur.on_ground)
    {
        return;
    }

    anim_tick += 1;
    if (anim_tick >= ANIMATION_TICKS)
    {
        anim_tick = 0;
        anim_frame = (anim_frame + 1) % DINO_FRAME_RUN_COUNT;
    }
}

static void update_obstacle(void)
{
    obstacle.x -= speed;

    if (obstacle.x + obstacle.w < 0)
    {
        add_obstacle(screen_w + 40 + rand() % 100);
    }
}

static void update_score(void)
{
    uint32_t elapsed_time = (time_us_32() - start_time_us) / 1000;
    uint32_t new_score = (elapsed_time * POINTS_PER_SECOND) / 1000;

    if (new_score != score)
    {
        score = new_score;
        old_score = true;
    }

    int16_t new_speed = SPEED_MIN + (int16_t)(elapsed_time / MS_PER_SPEED_STEP);
    speed = new_speed > SPEED_MAX ? SPEED_MAX : new_speed;
}
static bool collides(void)
{
    Rect dino = dinosaur_rect();
    Rect obs = obstacle_rect();
    return dino.x0 < obs.x1 && dino.x1 > obs.x0 && dino.y0 < obs.y1 && dino.y1 > obs.y0;
}

static void game_reset(void)
{
    LCD_Clear(WHITE);
    fill_rect(0, GROUND_Y, screen_w, GROUND_Y + 1, BLACK);

    dinosaur.h = DINO_H_STAND;
    dinosaur.y = GROUND_Y - DINO_H_STAND;
    dinosaur.vel_y = 0;
    dinosaur.on_ground = true;

    dinosaur_prev.valid = false;
    obstacle_prev.valid = false;

    anim_frame = 0;
    anim_tick = 0;

    score = 0;
    speed = SPEED_MIN;
    start_time_us = time_us_32();
    add_obstacle(screen_w + 60);

    jump_pend = false;
    restart_pend = false;
    crouch_pend = false;
    key_crouch = false;
    startup_buffer = 10;

    state = PLAYING;
    draw_score();
    old_score = false;
}

void game_init(void)
{
    screen_w = (int16_t)lcddev.width;
    screen_h = (int16_t)lcddev.height;

    build_sprites();

    srand(time_us_32());
    sound_init();
    game_reset();
}

void game_tick(void)
{
    process_keys();

    if (state == GAME_OVER)
    {
        if (restart_pend)
        {
            game_reset();
        }
        jump_pend = false;
        restart_pend = false;
        return;
    }

    update_dinosaur(jump_pend, crouch_pend);
    jump_pend = false;

    update_animation();
    update_obstacle();
    update_score();
    draw_frame();

    if (old_score)
    {
        draw_score();
        old_score = false;
    }

    if (collides())
    {
        state = GAME_OVER;
        sound_jump();

        LCD_DrawPicture(DINO_X, GROUND_Y - DINO_H_STAND, &dino_stand_pic[DINO_FRAME_DEAD]);
        game_over();
    }
}

//Return score to I2C and seven segment display to track high score
uint32_t get_score(void) {
    return score;
}