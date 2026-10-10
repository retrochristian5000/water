/* Water Hearts rules. Card index uses CARDS.DLL: face*4+suit;
 * ace face 0, two..king faces 1..12; clubs/diamonds/hearts/spades 0..3.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef WATER_MSHEARTS_H
#define WATER_MSHEARTS_H
#include <stdint.h>
enum hearts_phase { HEARTS_PASS, HEARTS_PLAY, HEARTS_HAND_DONE, HEARTS_GAME_DONE };
struct hearts_game {
    uint8_t hand[4][13], trick[4], last[4];
    unsigned count[4], score[4], points[4], round, completed, trick_count;
    uint32_t rng;
    int turn, leader, last_winner, last_points, broken, phase;
};
int hearts_rank(int card);
int hearts_points(int card);
void hearts_new(struct hearts_game *g, uint32_t seed);
void hearts_next(struct hearts_game *g);
int hearts_pass(struct hearts_game *g, const uint8_t cards[3]);
int hearts_legal(const struct hearts_game *g, int player, int card);
int hearts_play(struct hearts_game *g, int card);
int hearts_ai_choose(const struct hearts_game *g, int player);
int hearts_winner(const struct hearts_game *g);
#endif
