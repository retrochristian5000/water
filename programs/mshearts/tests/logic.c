/* Portable rules regression tests; compile without Windows dependencies.
 * cc -std=c99 -Wall -Wextra -Werror -Iprograms/mshearts \
 *   programs/mshearts/hearts.c programs/mshearts/tests/logic.c -o hearts-test
 * ./hearts-test
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hearts.h"

static void test_legal(void)
{
    struct hearts_game g = {0};
    g.phase = HEARTS_PLAY;
    g.turn = g.leader = 0;
    g.count[0] = 3;
    g.hand[0][0] = 4; /* 2C */
    g.hand[0][1] = 6; /* 2H */
    g.hand[0][2] = 47; /* QS */
    assert(hearts_legal(&g, 0, 4));
    assert(!hearts_legal(&g, 0, 6));
    assert(!hearts_legal(&g, 0, 47));
    g.completed = 1;
    g.trick_count = 1;
    g.leader = 1;
    g.turn = 0;
    g.trick[1] = 9; /* 3D */
    assert(hearts_legal(&g, 0, 47)); /* no diamonds */
    g.hand[0][2] = 13; /* 4D */
    assert(hearts_legal(&g, 0, 13));
    assert(!hearts_legal(&g, 0, 6));
    assert(!hearts_legal(&g, 0, 4));
    g.trick_count = 0;
    g.leader = g.turn = 0;
    assert(!hearts_legal(&g, 0, 6)); /* hearts not broken */
    g.broken = 1;
    assert(hearts_legal(&g, 0, 6));
}

static void test_games(void)
{
    struct hearts_game g, copy;
    unsigned hands = 0, p, i;
    hearts_new(&g, 12345);
    hearts_new(&copy, 12345);
    assert(!memcmp(&g, &copy, sizeof(g)));
    assert(hearts_rank(0) == 14 && hearts_rank(4) == 2);
    assert(hearts_points(47) == 13 && hearts_points(6) == 1);
    assert(!hearts_points(5));

    while (g.phase != HEARTS_GAME_DONE && hands < 256)
    {
        unsigned deck[52] = {0}, sum = 0, moves = 0;
        for (p = 0; p < 4; ++p)
        {
            assert(g.count[p] == 13);
            for (i = 0; i < 13; ++i)
            {
                int card = g.hand[p][i];
                assert(card < 52);
                ++deck[card];
            }
        }
        for (i = 0; i < 52; ++i) assert(deck[i] == 1);
        if (g.phase == HEARTS_PASS)
        {
            uint8_t pass[3] = {g.hand[0][0], g.hand[0][1], g.hand[0][2]};
            uint8_t duplicate[3] = {pass[0], pass[0], pass[1]};
            assert(!hearts_pass(&g, duplicate));
            assert(hearts_pass(&g, pass));
        }
        assert(g.phase == HEARTS_PLAY);
        while (g.phase == HEARTS_PLAY)
        {
            int c = hearts_ai_choose(&g, g.turn);
            assert(c >= 0 && hearts_legal(&g, g.turn, c));
            assert(hearts_play(&g, c));
            assert(++moves <= 52);
        }
        assert(moves == 52 && g.completed == 13);
        for (i = 0; i < 4; ++i) { assert(!g.count[i]); sum += g.points[i]; }
        assert(sum == 26);
        ++hands;
        if (g.phase == HEARTS_HAND_DONE) hearts_next(&g);
    }
    assert(g.phase == HEARTS_GAME_DONE);
    assert(hearts_winner(&g) >= 0);
    printf("hearts rules: %u simulated hands passed\n", hands);
}
int main(void)
{
    test_legal();
    test_games();
    return 0;
}
