#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "lcd.h"
#include "queue.h"
#include "game.h"

#define GROUND_Y 200

// For debugging purposes
#define KEY_JUMP 'A'
#define KEY_CROUCH 'B'
#define KEY_RESTART 'C'

#define DINO_X 30
#define DINO_W 30
#define DINO_H_STAND 40
#define DINO_H_CROUCH 20
#define GRAVITY 1
#define JUMP_VEL_Y (-12)

#define CACTUS_W 14
#define CACTUS_H 34
#define CROW_W 24
#define CROW_H 14
#define CROW_Y (GROUND_Y - 44)

#define SPEED_MIN 4
#define SPEED_MAX 10

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

static bool jump_pend;
static bool restart_pend;
static bool crouch_pend;

static void process_keys(void)
{
    uint32_t events;
    while (key_available())
    {   
        uint16_t events = key_pop();
        bool pressed = (events & (1u << 8)) != 0;
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
            crouch_pend = pressed;
        }
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

static void erase_rect(Rect *to_erase)
{
    if (to_erase->valid)
    {
        fill_rect(to_erase->x0, to_erase->y0, to_erase->x1, to_erase->y1, WHITE);
        to_erase->valid = false;
    }
}

static Rect dinosaur_rect(void)
{
    Rect rect = {DINO_X, dinosaur.y, DINO_X + DINO_W, GROUND_Y, true};
    return rect;
}

static Rect obstacle_rect(void)
{
    Rect rect = {obstacle.x, obstacle.y, obstacle.x + obstacle.w, obstacle.y + obstacle.h, true};
    return rect;
}

static void draw_frame(void)
{
    erase_rect(&dinosaur_prev);
    erase_rect(&obstacle_prev);

    Rect new_obstacle = obstacle_rect();
    fill_rect(new_obstacle.x0, new_obstacle.y0, new_obstacle.x1, new_obstacle.y1, BLACK);
    obstacle_prev = new_obstacle;

    Rect new_dinosaur = dinosaur_rect();
    fill_rect(new_dinosaur.x0, new_dinosaur.y0, new_dinosaur.x1, new_dinosaur.y1, BLACK);
    dinosaur_prev = new_dinosaur;
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
    if (dinosaur.on_ground && jump && !crouch)
    {
        dinosaur.vel_y = JUMP_VEL_Y;
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

static void update_obstacle(void)
{
    obstacle.x -= speed;

    if (obstacle.x + obstacle.w < 0)
    {
        add_obstacle(screen_w + 40 + rand() % 100);
    }

    if (!obstacle.scored && obstacle.x + obstacle.w < DINO_X)
    {
        obstacle.scored = true;
        score += 1;
        old_score = true;

        speed = SPEED_MIN + (int16_t)(score / 10);
        speed = speed > SPEED_MAX ? SPEED_MAX : speed;
    }
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

    score = 0;
    speed = SPEED_MIN;
    add_obstacle(screen_w + 60);

    jump_pend = false;
    restart_pend = false;

    state = PLAYING;
    draw_score();
    old_score = false;
}

void game_init(void)
{
    screen_w = (int16_t)lcddev.width;
    screen_h = (int16_t)lcddev.height;

    srand(time_us_32());
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

    update_obstacle();
    draw_frame();

    if (old_score)
    {
        draw_score();
        old_score = false;
    }

    if (collides())
    {
        state = GAME_OVER;
        game_over();
    }
}