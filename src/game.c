#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#define DINO_W 40
#define DINO_W_CROUCH 56
#define DINO_H_STAND 44
#define DINO_H_CROUCH 26
#define GRAVITY 1
#define FAST_FALL_GRAVITY 3 // If holding down crouch
#define JUMP_VEL_Y (-14)

#define DINO_FRAME_RUN_COUNT 3
#define DINO_FRAME_DEAD 3
#define DINO_FRAME_COUNT 4
#define DINO_CROUCH_FRAMES 2
#define ANIMATION_TICKS 4
#define CROW_TICKS 8

#define MAX_OBSTACLES 8
#define SPAWN_GAP_MAX 260
#define SPAWN_GAP_FRAMES_MIN 20
#define SPAWN_GAP_FRAMES_RANGE 30
#define CACTUS_GROUP_GAP 4
#define BIRD_CHANCE 4
#define LARGE_CACTUS_CHANCE 3
#define BIRD_BAND_COUNT 3


// Based on real dinosaur game
#define POINTS_PER_SECOND 9 
#define SPEED_RAMP_MS 120000
#define SPEED_MIN 6
#define SPEED_MAX 13

#define JOY_THRESHOLD_HIGH 3500
#define JOY_THRESHOLD_LOW 1500
#define HAS_CONTROLLER true

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
    int16_t w;
    bool on_ground;
} Dinosaur;

typedef struct {
    Picture frame[2];
    uint8_t frame_count;
    int16_t w, h;
} ObstacleSprite;

typedef struct {
    bool active;
    ObstacleSprite *sprite;
    int16_t x, y;
    uint8_t frame;
    Rect prev;
} Obstacle;

static GameState state;
static Dinosaur dinosaur;
static Rect dinosaur_prev;

static ObstacleSprite bird_sprite;
static ObstacleSprite cactus_small_sprite;
static ObstacleSprite cactus_large_sprite;
static Obstacle obstacles[MAX_OBSTACLES];
static int16_t spawn_gap;
static uint8_t bird_anim_tick;
static uint16_t *clip_px;

static uint32_t score;
static int16_t speed;
static bool old_score;
static uint64_t score_acc;
static int16_t screen_w;
static int16_t screen_h;
static uint32_t start_time_us;
static uint32_t last_tick_us;

static uint8_t anim_frame;
static uint8_t anim_tick;

static bool jump_pend;
static bool restart_pend;
static bool crouch_pend;
static bool key_crouch; 

extern int adc_fifo_out;
uint8_t startup_buffer;

int16_t bird_band_bottom[BIRD_BAND_COUNT] = {GROUND_Y - 80, GROUND_Y - 32, GROUND_Y - 8};

uint16_t dino_stand_px[DINO_FRAME_COUNT][DINO_W * DINO_H_STAND];
Picture dino_stand_pic[DINO_FRAME_COUNT];

uint16_t dino_crouch_px[DINO_CROUCH_FRAMES][DINO_W_CROUCH * DINO_H_CROUCH];
Picture dino_crouch_pic[DINO_CROUCH_FRAMES];

char dino_art[DINO_FRAME_COUNT][DINO_H_STAND][DINO_W + 1] =
{
    {
        "......................xxxxxxxxxxxxxxxx..",
        "......................xxxxxxxxxxxxxxxx..",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxx..xxxxxxxxxxxxxx",
        "....................xxxx..xxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxx..........",
        "....................xxxxxxxxxx..........",
        "....................xxxxxxxxxxxxxxxx....",
        "....................xxxxxxxxxxxxxxxx....",
        "xx................xxxxxxxxxx............",
        "xx................xxxxxxxxxx............",
        "xx.............xxxxxxxxxxxxx............",
        "xx.............xxxxxxxxxxxxx............",
        "xxxx........xxxxxxxxxxxxxxxxxxxx........",
        "xxxx........xxxxxxxxxxxxxxxxxxxx........",
        "xxxxxx....xxxxxxxxxxxxxxxxxx..xx........",
        "xxxxxx....xxxxxxxxxxxxxxxxxx..xx........",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "..xxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "..xxxxxxxxxxxxxxxxxxxxxxxx..............",
        "....xxxxxxxxxxxxxxxxxxxxxx..............",
        "....xxxxxxxxxxxxxxxxxxxxxx..............",
        "......xxxxxxxxxxxxxxxxxx................",
        "......xxxxxxxxxxxxxxxxxx................",
        "........xxxxxxxxxxxxxx..................",
        "........xxxxxxxxxxxxxx..................",
        "..........xxxxxx..xxxx..................",
        "..........xxxxxx..xxxx..................",
        "..........xxxx......xx..................",
        "..........xxxx......xx..................",
        "..........xx........xx..................",
        "..........xx........xx..................",
        "..........xxxx......xxxx................",
        "..........xxxx......xxxx................"
    },
    {
        "......................xxxxxxxxxxxxxxxx..",
        "......................xxxxxxxxxxxxxxxx..",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxx..xxxxxxxxxxxxxx",
        "....................xxxx..xxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxx..........",
        "....................xxxxxxxxxx..........",
        "....................xxxxxxxxxxxxxxxx....",
        "....................xxxxxxxxxxxxxxxx....",
        "xx................xxxxxxxxxx............",
        "xx................xxxxxxxxxx............",
        "xx.............xxxxxxxxxxxxx............",
        "xx.............xxxxxxxxxxxxx............",
        "xxxx........xxxxxxxxxxxxxxxxxxxx........",
        "xxxx........xxxxxxxxxxxxxxxxxxxx........",
        "xxxxxx....xxxxxxxxxxxxxxxxxx..xx........",
        "xxxxxx....xxxxxxxxxxxxxxxxxx..xx........",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "..xxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "..xxxxxxxxxxxxxxxxxxxxxxxx..............",
        "....xxxxxxxxxxxxxxxxxxxxxx..............",
        "....xxxxxxxxxxxxxxxxxxxxxx..............",
        "......xxxxxxxxxxxxxxxxxx................",
        "......xxxxxxxxxxxxxxxxxx................",
        "........xxxxxxxxxxxxxx..................",
        "........xxxxxxxxxxxxxx..................",
        "..........xxxxxx....xxxxx...............",
        "..........xxxxxx....xxxxx...............",
        "..........xxxx..........................",
        "..........xxxx..........................",
        "..........xx............................",
        "..........xx............................",
        "..........xxxx..........................",
        "..........xxxx.........................."
    },
    {
        "......................xxxxxxxxxxxxxxxx..",
        "......................xxxxxxxxxxxxxxxx..",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxx..xxxxxxxxxxxxxx",
        "....................xxxx..xxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxx..........",
        "....................xxxxxxxxxx..........",
        "....................xxxxxxxxxxxxxxxx....",
        "....................xxxxxxxxxxxxxxxx....",
        "xx................xxxxxxxxxx............",
        "xx................xxxxxxxxxx............",
        "xx.............xxxxxxxxxxxxx............",
        "xx.............xxxxxxxxxxxxx............",
        "xxxx........xxxxxxxxxxxxxxxxxxxx........",
        "xxxx........xxxxxxxxxxxxxxxxxxxx........",
        "xxxxxx....xxxxxxxxxxxxxxxxxx..xx........",
        "xxxxxx....xxxxxxxxxxxxxxxxxx..xx........",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "..xxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "..xxxxxxxxxxxxxxxxxxxxxxxx..............",
        "....xxxxxxxxxxxxxxxxxxxxxx..............",
        "....xxxxxxxxxxxxxxxxxxxxxx..............",
        "......xxxxxxxxxxxxxxxxxx................",
        "......xxxxxxxxxxxxxxxxxx................",
        "........xxxxxxxxxxxxxx..................",
        "........xxxxxxxxxxxxxx..................",
        "..........xxxx....xxxx..................",
        "..........xxxx....xxxx..................",
        "............xxxx....xx..................",
        "............xxxx....xx..................",
        "....................xx..................",
        "....................xx..................",
        "....................xxxx................",
        "....................xxxx................"
    },
    {
        "......................xxxxxxxxxxxxxxxx..",
        "......................xxxxxxxxxxxxxxxx..",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxx....xxxxxxxxxxxx",
        "....................xxxx.xx.xxxxxxxxxxxx",
        "....................xxxx.xx.xxxxxxxxxxxx",
        "....................xxxx....xxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxxxxxx",
        "....................xxxxxxxxxxxxxxxx....",
        "....................xxxxxxxxxxxxxxxx....",
        "xx................xxxxxxxxxx............",
        "xx................xxxxxxxxxx............",
        "xx.............xxxxxxxxxxxxx............",
        "xx.............xxxxxxxxxxxxx............",
        "xxxx........xxxxxxxxxxxxxxxxxxxx........",
        "xxxx........xxxxxxxxxxxxxxxxxxxx........",
        "xxxxxx....xxxxxxxxxxxxxxxxxx..xx........",
        "xxxxxx....xxxxxxxxxxxxxxxxxx..xx........",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "..xxxxxxxxxxxxxxxxxxxxxxxxxx............",
        "..xxxxxxxxxxxxxxxxxxxxxxxx..............",
        "....xxxxxxxxxxxxxxxxxxxxxx..............",
        "....xxxxxxxxxxxxxxxxxxxxxx..............",
        "......xxxxxxxxxxxxxxxxxx................",
        "......xxxxxxxxxxxxxxxxxx................",
        "........xxxxxxxxxxxxxx..................",
        "........xxxxxxxxxxxxxx..................",
        "..........xxxxxx..xxxx..................",
        "..........xxxxxx..xxxx..................",
        "..........xxxx......xx..................",
        "..........xxxx......xx..................",
        "..........xx........xx..................",
        "..........xx........xx..................",
        "..........xxxx......xxxx................",
        "..........xxxx......xxxx................",

    }
};

char dino_crouch_art[DINO_CROUCH_FRAMES][DINO_H_CROUCH][DINO_W_CROUCH + 1] =
{
    {
        "xx.....................................................",
        "xx...................................xxxxxxxxxxxxxxxx..",
        "xxxxxx........xxxxxxxxxxxxxxxxx......xxxxxxxxxxxxxxxx..",
        "xxxxxx........xxxxxxxxxxxxxxxxx....xxxxxxxxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx..xxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx..xxxxxxxxxxxxxx",
        "..xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "..xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "....xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "....xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "......xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "......xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "........xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx..........",
        "........xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx..........",
        "..........xxxxxxxxxxxxxxxxxxxxxxx....xxxxxxxxxxxxxx....",
        "..........xxxxxxxxxxxxxxxxxxxxxxx....xxxxxxxxxxxxxx....",
        "............xxxxxxxxxxxxxxxxxxxxx......................",
        "............xxxxxxxxxxxx.....xx........................",
        "...........xx....xxxxxx......xx........................",
        "...........xx....xxxxxx......xxxx......................",
        "...........xxxx..xxxx........xxxx......................",
        "...........xxxx..xxxx..................................",
        ".................xx....................................",
        ".................xx....................................",
        ".................xxxx..................................",
        ".................xxxx..................................",
    },
    {
        "xx.....................................................",
        "xx...................................xxxxxxxxxxxxxxxx..",
        "xxxxxx........xxxxxxxxxxxxxxxxx......xxxxxxxxxxxxxxxx..",
        "xxxxxx........xxxxxxxxxxxxxxxxx....xxxxxxxxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx..xxxxxxxxxxxxxx",
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx..xxxxxxxxxxxxxx",
        "..xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "..xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "....xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "....xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "......xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "......xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
        "........xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx..........",
        "........xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx..........",
        "..........xxxxxxxxxxxxxxxxxxxxxxx....xxxxxxxxxxxxxx....",
        "..........xxxxxxxxxxxxxxxxxxxxxxx....xxxxxxxxxxxxxx....",
        "............xxxxxxxxxxxxxxxxxxxxx......................",
        "............xxxxxxxxxxxx.....xx........................",
        "...........xxxxxx....xxxxx...xx........................",
        "...........xxxxxx....xxxxx...xxxx......................",
        "...........xxxx..............xxxx......................",
        "...........xxxx........................................",
        "...........xx..........................................",
        "...........xx..........................................",
        "...........xxxx........................................",
        "...........xxxx........................................",
    }
};

char *bird_frame_0[] = 
{
    "........xxxx..............................",
    "........xxxx..............................",
    "......xxxxxx..............................",
    "......xxxxxx..............................",
    "....xxxxxxxxxx............................",
    "....xxxxxxxxxx............................",
    "..xxxxxxxxxxxx............................",
    "..xxxxxxxxxxxx............................",
    "xxxxxxxxxxxxxxxxxxxxxxxxxxx...............",
    "xxxxxxxxxxxxxxxxxxxxxxxxxxx...............",
    "............xxxxxxxxxxxxxxxxx.............",
    "............xxxxxxxxxxxxxxxxx.............",
    "..............xxxxxxxxxxxxxxxxxxxxxxxxxxxx",
    "..............xxxxxxxxxxxxxxxxxxxxxxxxxxxx",
    "................xxxxxxxxxxxxxxxxxxxx......",
    "................xxxxxxxxxxxxxxxxxxxx......",
    "................xxxxxxxxxxxxxxxxxxxxxxxx..",
    "................xxxxxxxxxxxxxxxxxxxxxxxx..",
    "................xxxxxxxx..................",
    "................xxxxxxxx..................",
    "................xxxxxx....................",
    "................xxxxxx....................",
    "................xxxx......................",
    "................xxxx......................",
    "................xxxx......................",
    "................xx........................",
    "................xx........................",
    NULL
};

char *bird_frame_1[] = 
{
    "..............xx..........................",
    "..............xx..........................",
    "..............xxxx........................",
    "..............xxxx........................",
    "..............xxxxxx......................",
    "..............xxxxxx......................",
    "........xxxx....xxxxxx....................",
    "........xxxx....xxxxxx....................",
    "......xxxxxx....xxxxxxxx..................",
    "......xxxxxx....xxxxxxxx..................",
    "....xxxxxxxxxx..xxxxxxxxxx................",
    "....xxxxxxxxxx..xxxxxxxxxx................",
    "..xxxxxxxxxxxx..xxxxxxxxxxxx..............",
    "..xxxxxxxxxxxx..xxxxxxxxxxxx..............",
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxx..............",
    "xxxxxxxxxxxxxxxxxxxxxxxxxxxx..............",
    "............xxxxxxxxxxxxxxxxxx............",
    "............xxxxxxxxxxxxxxxxxx............",
    "..............xxxxxxxxxxxxxxxxxxxxxxxxxxxx",
    "..............xxxxxxxxxxxxxxxxxxxxxxxxxxxx",
    "................xxxxxxxxxxxxxxxxxxxx......",
    "................xxxxxxxxxxxxxxxxxxxx......",
    "..................xxxxxxxxxxxxxxxxxxxxxx..",
    "..................xxxxxxxxxxxxxxxxxxxxxx..",
    "....................xxxxxxxxxxxxxx........",
    "....................xxxxxxxxxxxxxx........",
    "......................xxxxxxxxxxxx........",
    NULL
};

char *cactus_large_art[] =
{
    ".........xxxxx.........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx....xxx.",
    "........xxxxxxx...xxxxx",
    ".xxx....xxxxxxx...xxxxx",
    "xxxxx...xxxxxxx...xxxxx",
    "xxxxx...xxxxxxx...xxxxx",
    "xxxxx...xxxxxxx...xxxxx",
    "xxxxx...xxxxxxx...xxxxx",
    "xxxxx...xxxxxxx...xxxxx",
    "xxxxx...xxxxxxx...xxxxx",
    "xxxxx...xxxxxxx...xxxxx",
    "xxxxx...xxxxxxx...xxxxx",
    "xxxxx...xxxxxxx...xxxxx",
    "xxxxx...xxxxxxx...xxxxx",
    "xxxxx...xxxxxxx...xxxxx",
    "xxxxx...xxxxxxx...xxxxx",
    "xxxxx...xxxxxxx...xxxxx",
    "xxxxxxxxxxxxxxxxxxxxxx.",
    "xxxxxxxxxxxxxxxxxxxxx..",
    ".xxxxxxxxxxxxxxxxxxx...",
    "..xxxxxxxxxxxxxxxxx....",
    "...xxxxxxxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "........xxxxxxx........",
    "......xxxxxxxxx........",
    NULL
};

char *cactus_small_art[] = 
{
    "......xxx......",
    ".....xxxxx.....",
    ".....xxxxx.....",
    ".....xxxxx.....",
    ".xx..xxxxx...x.",
    "xxxx.xxxxx..xxx",
    "xxxx.xxxxx..xxx",
    "xxxx.xxxxx..xxx",
    "xxxx.xxxxx..xxx",
    "xxxx.xxxxx..xxx",
    "xxxx.xxxxx..xxx",
    "xxxx.xxxxx..xxx",
    "xxxxxxxxxx..xxx",
    "xxxxxxxxxx..xxx",
    ".xxxxxxxxx..xxx",
    "..xxxxxxxx..xxx",
    ".....xxxxx..xxx",
    ".....xxxxxxxxxx",
    ".....xxxxxxxxx.",
    ".....xxxxxxxx..",
    ".....xxxxx.....",
    ".....xxxxx.....",
    ".....xxxxx.....",
    ".....xxxxx.....",
    ".....xxxxx.....",
    ".....xxxxx.....",
    ".....xxxxx.....",
    ".....xxxxx.....",
    ".....xxxxx.....",
    ".....xxxxx.....",
    ".....xxxxx.....",
    ".....xxxxx.....",
    ".....xxxxx.....",
    NULL
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

    for (int f = 0; f < DINO_CROUCH_FRAMES; f++)
    {
        for (int r = 0; r < DINO_H_CROUCH; r++)
        {
            for (int c = 0; c < DINO_W_CROUCH; c++)
            {
                dino_crouch_px[f][r * DINO_W_CROUCH + c] = (dino_crouch_art[f][r][c] == 'x') ? BLACK : WHITE;
            }
        }
        dino_crouch_pic[f].width = DINO_W_CROUCH;
        dino_crouch_pic[f].height = DINO_H_CROUCH;
        dino_crouch_pic[f].bytes_per_pixel = 2;
        dino_crouch_pic[f].pixel_data = (unsigned char *)dino_crouch_px[f];
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

static int count_rows(char **rows)
{
    int count = 0;
    while (rows[count] != NULL)
    {
        count += 1;
    }
    return count;
}

static void build_obstacle_sprite(ObstacleSprite *sprite, char ***frames, uint8_t count)
{
    sprite->w = 0;
    sprite->h = 0;

    for (int f = 0; f < count; f++)
    {
        int rows = count_rows(frames[f]);
        if (rows > sprite->h) sprite->h = rows;

        for (int r = 0; r < rows; r++)
        {
            int len = (int)strlen(frames[f][r]);
            if (len > sprite->w) sprite->w = len;
        }
    }

    sprite->frame_count = count;

    for (int f = 0; f < count; f++)
    {
        int rows = count_rows(frames[f]);
        uint16_t *px = malloc(sprite->w * sprite->h * sizeof(uint16_t));

        for (int r = 0; r < sprite->h; r++)
        {
            const char *row = (r < rows) ? frames[f][r] : "";
            int len = (int)strlen(row);

            for (int c = 0; c < sprite->w; c++)
            {
                px[r * sprite->w + c] = (c < len && row[c] == 'x') ? BLACK : WHITE;
            }
        }

        sprite->frame[f].width = sprite->w;
        sprite->frame[f].height = sprite->h;
        sprite->frame[f].bytes_per_pixel = 2;
        sprite->frame[f].pixel_data = (unsigned char *)px;
    }
}

static void build_obstacle_sprites(void)
{
    char **bird_frames[2] = {bird_frame_0, bird_frame_1};
    char **small_frames[1] = {cactus_small_art};
    char **large_frames[1] = {cactus_large_art};

    build_obstacle_sprite(&bird_sprite, bird_frames, 2);
    build_obstacle_sprite(&cactus_small_sprite, small_frames, 1);
    build_obstacle_sprite(&cactus_large_sprite, large_frames, 1);

    ObstacleSprite *all[] = {&bird_sprite, &cactus_small_sprite, &cactus_large_sprite};
    int max_area = 0;
    for (int i = 0; i < 3; i++)
    {
        int area = all[i]->w * all[i]->h;
        max_area = area > max_area ? area : max_area;
    }
    clip_px = malloc(max_area * sizeof(uint16_t));
}

static Rect obstacle_rect(const Obstacle *obs)
{
    Rect rect = {obs->x, obs->y, obs->x + obs->sprite->w, obs->y + obs->sprite->h, true};
    return rect;
}

static Rect dinosaur_rect(void)
{
    Rect rect = {DINO_X, dinosaur.y, DINO_X + dinosaur.w, dinosaur.y + dinosaur.h, true};
    return rect;
}

static Obstacle *free_space(void)
{
    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        if (!obstacles[i].active)
        {
            return &obstacles[i];
        }
    }
    return NULL;
}

static void place_obstacle(Obstacle *obs, ObstacleSprite *sprite, int16_t x, int16_t y)
{
    obs->active = true;
    obs->sprite = sprite;
    obs->x = x;
    obs->y = y;
    obs->frame = 0;
    obs->prev.valid = false;
}

static void spawn_bird(void)
{
    Obstacle *obs = free_space();
    if (obs == NULL)
    {
        return;
    }

    int16_t band = bird_band_bottom[rand() % BIRD_BAND_COUNT];
    place_obstacle(obs, &bird_sprite, screen_w, band - bird_sprite.h);
}

static void spawn_cacti(void)
{
    bool large = (rand() % LARGE_CACTUS_CHANCE == 0);
    ObstacleSprite *sprite = large ? &cactus_large_sprite : &cactus_small_sprite;
    int count = large ? 1 + rand() % 2 : 1 + rand() % 3;
    int16_t x = screen_w;

    for (int i = 0; i < count; i++)
    {
        Obstacle *obs = free_space();
        if (obs == NULL)
        {
            break;
        }

        place_obstacle(obs, sprite, x, GROUND_Y - sprite->h);
        x += sprite->w + CACTUS_GROUP_GAP;
    }
}

static void spawn_group(void)
{
    if (rand() % BIRD_CHANCE == 0)
    {
        spawn_bird();
    }
    else
    {
        spawn_cacti();
    }
}

static void update_obstacles(void)
{
    bool flap = false;
    bird_anim_tick += 1;
    if (bird_anim_tick >= CROW_TICKS)
    {
        bird_anim_tick = 0;
        flap = true;
    }

    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        Obstacle *obs = &obstacles[i];
        if (!obs->active)
        {
            continue;
        }

        obs->x -= speed;

        if (flap && obs->sprite == &bird_sprite)
        {
            obs->frame = (obs->frame + 1) % bird_sprite.frame_count;
        }

        if (obs->x + obs->sprite->w <= 0)
        {
            if (obs->prev.valid)
            {
                fill_rect(obs->prev.x0, obs->prev.y0, obs->prev.x1, obs->prev.y1, WHITE);
            }
            obs->active = false;
        }
    }

    int16_t rightmost = -1;
    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        Obstacle *obs = &obstacles[i];
        if (obs->active && obs->x + obs->sprite->w > rightmost)
        {
            rightmost = obs->x + obs->sprite->w;
        }
    }

    if (rightmost < 0 || rightmost <= screen_w - spawn_gap)
    {
        spawn_group();
        spawn_gap = speed * (SPAWN_GAP_FRAMES_MIN + rand() % SPAWN_GAP_FRAMES_RANGE);
        if (spawn_gap > SPAWN_GAP_MAX)
        {
            spawn_gap = SPAWN_GAP_MAX;
        }
    }
}

static const Picture *dinosaur_picture(void)
{
    if (dinosaur.on_ground && dinosaur.h == DINO_H_CROUCH)
    {
        return &dino_crouch_pic[anim_frame % DINO_CROUCH_FRAMES];
    }
    return &dino_stand_pic[dinosaur.on_ground ? anim_frame : 0];
}

static void draw_dinosaur(void)
{
    LCD_DrawPicture(DINO_X, dinosaur.y, dinosaur_picture());
}

static void draw_obstacle(const Obstacle *obs)
{
    const ObstacleSprite *sprite = obs->sprite;
    int16_t start_x0 = (obs->x < 0) ? -obs->x : 0;
    int16_t start_x1 = (obs->x + sprite->w > screen_w) ? screen_w - obs->x : sprite->w;
    int16_t visible_window = start_x1 - start_x0;

    if (visible_window <= 0)
    {
        return;
    }

    if (visible_window == sprite->w)
    {
        LCD_DrawPicture(obs->x, obs->y, &sprite->frame[obs->frame]);
        return;
    }

    const uint16_t *start = (const uint16_t *)sprite->frame[obs->frame].pixel_data;
    for (int r = 0; r < sprite->h; r++)
    {
        for (int c = 0; c < visible_window; c++)
        {
            clip_px[r * visible_window + c] = start[r * sprite->w + start_x0 + c];
        }
    }

    Picture visible = {visible_window, sprite->h, 2, (unsigned char *)clip_px};
    LCD_DrawPicture(obs->x + start_x0, obs->y, &visible);
}

static void draw_frame(void)
{
    Rect obs_new[MAX_OBSTACLES];

    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        Obstacle *obs = &obstacles[i];
        if (!obs->active)
        {
            continue;
        }
        obs_new[i] = obstacle_rect(obs);
        erase_old(obs->prev, obs_new[i]);
    }

    Rect dino_new = dinosaur_rect();
    erase_old(dinosaur_prev, dino_new);

    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        Obstacle *obs = &obstacles[i];
        if (!obs->active)
        {
            continue;
        }
        draw_obstacle(obs);
        obs->prev = obs_new[i];
        obs->prev.valid = true;
    }

    draw_dinosaur();
    dinosaur_prev = dino_new;
}

static bool pixel_is_black(const Picture *pic, int x, int y)
{
    const uint16_t *px = (const uint16_t *)pic->pixel_data;
    return px[y * pic->width + x] == BLACK;
}

static bool pixels_collide(const Obstacle *obs)
{
    const Picture *dino_pic = dinosaur_picture();
    const Picture *obs_pic = &obs->sprite->frame[obs->frame];

    Rect dino = dinosaur_rect();
    Rect obs_curr = obstacle_rect(obs);

    
    int x0 = dino.x0 > obs_curr.x0 ? dino.x0 : obs_curr.x0;
    int x1 = dino.x1 < obs_curr.x1 ? dino.x1 : obs_curr.x1;
    int y0 = dino.y0 > obs_curr.y0 ? dino.y0 : obs_curr.y0;
    int y1 = dino.y1 < obs_curr.y1 ? dino.y1 : obs_curr.y1;

    for (int y = y0; y < y1; y++)
    {
        for (int x = x0; x < x1; x++)
        {
            if (pixel_is_black(dino_pic, x- dino.x0, y - dino.y0) && pixel_is_black(obs_pic, x - obs_curr.x0, y - obs_curr.y0))
            {
                return true;
            }
        }
    }
    return false;
}

static bool collides(void)
{
    Rect dino = dinosaur_rect();

    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        if (!obstacles[i].active)
        {
            continue;
        }

        Rect obs = obstacle_rect(&obstacles[i]);
        bool overlap = dino.x0 < obs.x1 && dino.x1 > obs.x0 && dino.y0 < obs.y1 && dino.y1 > obs.y0;

        if (overlap && pixels_collide(&obstacles[i]))
        {
            return true;
        }
    }

    return false;
}

static void draw_dead(void)
{
    int16_t dead_y = dinosaur.on_ground ? (GROUND_Y - DINO_H_STAND) : dinosaur.y;
    int16_t top = dead_y < dinosaur.y ? dead_y : dinosaur.y;

    fill_rect(DINO_X, top, DINO_X + DINO_W_CROUCH, GROUND_Y, WHITE);

    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        if (obstacles[i].active)
        {
            draw_obstacle(&obstacles[i]);
        }
    }

    LCD_DrawPicture(DINO_X, dead_y, &dino_stand_pic[DINO_FRAME_DEAD]);
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
    fill_rect(110, 70, 300, 130, WHITE);
    LCD_DrawString(150, 80, BLACK, WHITE, "GAME OVER", 16, 0);
    LCD_DrawString(130, 104, BLACK, WHITE, "Press C to Restart", 16, 0);
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
        if (crouch)
        {
            dinosaur.h = DINO_H_CROUCH;
            dinosaur.w = DINO_W_CROUCH;
        }
        else
        {
            dinosaur.h = DINO_H_STAND;
            dinosaur.w = DINO_W;
        }
        dinosaur.y = GROUND_Y - dinosaur.h;
    }
    else
    {
        dinosaur.h = DINO_H_STAND;
        dinosaur.w = DINO_W;
        dinosaur.vel_y += crouch ? FAST_FALL_GRAVITY : GRAVITY;
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

static void update_score(void)
{
    uint32_t current_time = time_us_32();

    uint32_t dt_us = current_time - last_tick_us;
    score_acc += (uint64_t)speed * dt_us * 15;
    last_tick_us = current_time;

    uint32_t new_score = (uint32_t)(score_acc / 10000000ULL);

    if (new_score != score)
    {
        score = new_score;
        old_score = true;
    }

    uint32_t elapsed_ms = (current_time - start_time_us) / 1000;
    int16_t new_speed = SPEED_MIN + (int16_t)((elapsed_ms * (SPEED_MAX - SPEED_MIN)) / SPEED_RAMP_MS) ;
    speed = new_speed > SPEED_MAX ? SPEED_MAX : new_speed;
}

static void free_obstacle_sprite(ObstacleSprite *sprite)
{
    for (int f = 0; f < sprite->frame_count; f++)
    {
        free(sprite->frame[f].pixel_data);
        sprite->frame[f].pixel_data = NULL;
    }
    sprite->frame_count = 0;
}

void game_shutdown(void)
{
    free_obstacle_sprite(&bird_sprite);
    free_obstacle_sprite(&cactus_small_sprite);
    free_obstacle_sprite(&cactus_large_sprite);
    free(clip_px);
    clip_px = NULL;
}

static void game_reset(void)
{
    LCD_Clear(WHITE);
    fill_rect(0, GROUND_Y, screen_w, GROUND_Y + 1, BLACK);

    dinosaur.h = DINO_H_STAND;
    dinosaur.w = DINO_W;
    dinosaur.y = GROUND_Y - DINO_H_STAND;
    dinosaur.vel_y = 0;
    dinosaur.on_ground = true;

    dinosaur_prev.valid = false;

    anim_frame = 0;
    anim_tick = 0;

    score = 0;
    speed = SPEED_MIN;
    start_time_us = time_us_32();
    last_tick_us = time_us_32();
    score_acc = 0;

    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        obstacles[i].active = false;
        obstacles[i].prev.valid = false;
    }
    spawn_gap = SPEED_MIN * SPAWN_GAP_FRAMES_MIN;
    bird_anim_tick = 0;

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
    game_shutdown();
    
    screen_w = (int16_t)lcddev.width;
    screen_h = (int16_t)lcddev.height;

    build_sprites();
    build_obstacle_sprites();

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
    update_obstacles();
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
        draw_dead();
        game_over();
    }
}

//Return score to I2C and seven segment display to track high score
uint32_t get_score(void) {
    return score;
}