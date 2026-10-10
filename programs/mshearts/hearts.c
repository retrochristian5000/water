/* Clean-room Hearts / Black Lady rules engine, with no Win32 dependencies.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include "hearts.h"
#include <string.h>
#define HEART_SUIT 2
#define TWO_CLUBS 4
#define QUEEN_SPADES 47

int hearts_rank(int card) { return card / 4 ? card / 4 + 1 : 14; }
int hearts_points(int card) { return card == QUEEN_SPADES ? 13 : card % 4 == HEART_SUIT; }
static int compare(int a, int b)
{
    if (a % 4 != b % 4) return a % 4 - b % 4;
    return hearts_rank(a) - hearts_rank(b);
}
static void sort_hand(struct hearts_game *g, int p)
{
    unsigned i, j;
    for (i = 1; i < g->count[p]; ++i)
    {
        uint8_t c = g->hand[p][i];
        for (j = i; j && compare(c, g->hand[p][j - 1]) < 0; --j)
            g->hand[p][j] = g->hand[p][j - 1];
        g->hand[p][j] = c;
    }
}
static uint32_t random32(struct hearts_game *g)
{
    uint32_t x = g->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g->rng = x;
}
static int owns(const struct hearts_game *g, int p, int card)
{
    unsigned i;
    for (i = 0; i < g->count[p]; ++i)
        if (g->hand[p][i] == card) return 1;
    return 0;
}
static void take(struct hearts_game *g, int p, int card)
{
    unsigned i;
    for (i = 0; i < g->count[p]; ++i)
        if (g->hand[p][i] == card)
        {
            --g->count[p];
            if (i < g->count[p])
                memmove(g->hand[p] + i, g->hand[p] + i + 1, g->count[p] - i);
            return;
        }
}
static void start_tricks(struct hearts_game *g)
{
    int p;
    g->phase = HEARTS_PLAY;
    g->trick_count = g->completed = 0;
    g->broken = 0;
    g->last_winner = -1;
    g->last_points = 0;
    for (p = 0; p < 4; ++p)
    {
        g->points[p] = 0;
        g->trick[p] = g->last[p] = 255;
        if (owns(g, p, TWO_CLUBS)) g->leader = g->turn = p;
    }
}
static void deal(struct hearts_game *g)
{
    uint8_t deck[52];
    unsigned i, p;
    for (i = 0; i < 52; ++i) deck[i] = i;
    for (i = 51; i; --i)
    {
        unsigned j = random32(g) % (i + 1);
        uint8_t c = deck[i];
        deck[i] = deck[j];
        deck[j] = c;
    }
    memset(g->count, 0, sizeof(g->count));
    for (i = 0; i < 52; ++i)
    {
        p = i % 4;
        g->hand[p][g->count[p]++] = deck[i];
    }
    for (p = 0; p < 4; ++p) sort_hand(g, p);
    if (g->round % 4 == 3) start_tricks(g);
    else
    {
        g->phase = HEARTS_PASS;
        g->completed = g->trick_count = 0;
        g->last_winner = -1;
    }
}
void hearts_new(struct hearts_game *g, uint32_t seed)
{
    memset(g, 0, sizeof(*g));
    g->rng = seed ? seed : 0x5eed1234u;
    deal(g);
}
void hearts_next(struct hearts_game *g)
{
    if (g->phase != HEARTS_HAND_DONE) return;
    ++g->round;
    deal(g);
}
static int pass_value(int card)
{
    int rank = hearts_rank(card);
    if (card == QUEEN_SPADES) return 1000;
    if (card % 4 == 3 && rank >= 13) return 450 + rank;
    if (card % 4 == HEART_SUIT) return 150 + rank * 3;
    return rank * 7;
}
int hearts_pass(struct hearts_game *g, const uint8_t cards[3])
{
    uint8_t moved[4][3];
    int p, i, j, delta;
    if (g->phase != HEARTS_PASS || !cards) return 0;
    for (i = 0; i < 3; ++i)
    {
        if (cards[i] >= 52 || !owns(g, 0, cards[i])) return 0;
        for (j = 0; j < i; ++j) if (cards[i] == cards[j]) return 0;
        moved[0][i] = cards[i];
    }
    for (p = 1; p < 4; ++p)
    {
        int used[13] = {0};
        for (i = 0; i < 3; ++i)
        {
            int best = -1, value = -1;
            unsigned k;
            for (k = 0; k < g->count[p]; ++k)
            {
                int v;
                if (used[k]) continue;
                v = pass_value(g->hand[p][k]);
                if (v <= value) continue;
                best = k;
                value = v;
            }
            used[best] = 1;
            moved[p][i] = g->hand[p][best];
        }
    }
    for (p = 0; p < 4; ++p)
        for (i = 0; i < 3; ++i) take(g, p, moved[p][i]);
    delta = g->round % 4 == 0 ? 1 : g->round % 4 == 1 ? 3 : 2;
    for (p = 0; p < 4; ++p)
        for (i = 0; i < 3; ++i)
            g->hand[(p + delta) % 4][g->count[(p + delta) % 4]++] = moved[p][i];
    for (p = 0; p < 4; ++p) sort_hand(g, p);
    start_tricks(g);
    return 1;
}
int hearts_legal(const struct hearts_game *g, int player, int card)
{
    unsigned i;
    int suit, led, has_suit = 0, can_discard_nonpoint = 0;
    if (g->phase != HEARTS_PLAY || player < 0 || player >= 4 || player != g->turn ||
        card < 0 || card >= 52 || !owns(g, player, card)) return 0;
    if (!g->completed && !g->trick_count) return card == TWO_CLUBS;
    suit = card % 4;
    if (!g->trick_count)
    {
        if (suit != HEART_SUIT || g->broken) return 1;
        for (i = 0; i < g->count[player]; ++i)
            if (g->hand[player][i] % 4 != HEART_SUIT) return 0;
        return 1;
    }
    led = g->trick[g->leader] % 4;
    for (i = 0; i < g->count[player]; ++i)
    {
        int other = g->hand[player][i];
        if (other % 4 == led) has_suit = 1;
        if (other % 4 != led && !hearts_points(other)) can_discard_nonpoint = 1;
    }
    if (has_suit) return suit == led;
    if (!g->completed && hearts_points(card) && can_discard_nonpoint) return 0;
    return 1;
}
static void finish_hand(struct hearts_game *g)
{
    int p, shooter = -1;
    for (p = 0; p < 4; ++p)
        if (g->points[p] == 26) shooter = p;
    for (p = 0; p < 4; ++p)
        g->score[p] += shooter >= 0 ? (p == shooter ? 0 : 26) : g->points[p];
    g->phase = HEARTS_HAND_DONE;
    for (p = 0; p < 4; ++p)
        if (g->score[p] >= 100) g->phase = HEARTS_GAME_DONE;
}
int hearts_play(struct hearts_game *g, int card)
{
    int p = g->turn, winner, led, high, points = 0, i;
    if (!hearts_legal(g, p, card)) return 0;
    take(g, p, card);
    g->trick[p] = card;
    if (card % 4 == HEART_SUIT) g->broken = 1;
    if (++g->trick_count < 4)
    {
        g->turn = (p + 1) % 4;
        return 1;
    }
    winner = g->leader;
    led = g->trick[winner] % 4;
    high = hearts_rank(g->trick[winner]);
    for (i = 0; i < 4; ++i)
    {
        int c = g->trick[i];
        points += hearts_points(c);
        g->last[i] = c;
        if (c % 4 == led && hearts_rank(c) > high)
        {
            high = hearts_rank(c);
            winner = i;
        }
        g->trick[i] = 255;
    }
    g->last_winner = winner;
    g->last_points = points;
    g->points[winner] += points;
    ++g->completed;
    g->trick_count = 0;
    g->turn = g->leader = winner;
    if (g->completed == 13) finish_hand(g);
    return 1;
}
int hearts_ai_choose(const struct hearts_game *g, int player)
{
    int best = -1, value = -100000, high = 0, led = -1;
    unsigned i;
    if (g->phase != HEARTS_PLAY || g->turn != player) return -1;
    if (g->trick_count)
    {
        int p;
        led = g->trick[g->leader] % 4;
        for (p = 0; p < 4; ++p)
            if (g->trick[p] != 255 && g->trick[p] % 4 == led &&
                hearts_rank(g->trick[p]) > high) high = hearts_rank(g->trick[p]);
    }
    for (i = 0; i < g->count[player]; ++i)
    {
        int card = g->hand[player][i], rank = hearts_rank(card), v;
        if (!hearts_legal(g, player, card)) continue;
        if (led < 0) v = -rank * 10 - (card % 4 == HEART_SUIT ? 30 : 0);
        else if (card % 4 != led) v = hearts_points(card) ? 1500 + hearts_points(card) * 10 + rank : rank;
        else if (rank < high) v = 800 + rank * 5;
        else v = -rank * 5;
        if (v > value) { value = v; best = card; }
    }
    return best;
}
int hearts_winner(const struct hearts_game *g)
{
    int p, best = 0;
    if (g->phase != HEARTS_GAME_DONE) return -1;
    for (p = 1; p < 4; ++p)
        if (g->score[p] < g->score[best]) best = p;
    return best;
}
