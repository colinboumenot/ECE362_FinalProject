
#ifndef SCORE_SS7_H
#define SCORE_SS7_H

#include <stdint.h>
#include <stdbool.h>

//Initialize seven-segment display and load score
void init_ss(void);

//Update high score if current score is greater
bool update_high_score(uint16_t current_score);

//Display a score
void display_high_score(uint16_t score);

#endif