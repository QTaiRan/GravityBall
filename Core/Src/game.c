/**
  ******************************************************************************
  * @file    game.c
  * @brief   BALL BLASTER - MPU6050 tilt-controlled ball dodging game
  *
  *          Controls (4x4 keypad):
  *            2/4/6/8 : nudge ball up/left/right/down
  *            5       : confirm / pause
  *            16      : back to menu
  ******************************************************************************
  */
#include "game.h"
#include "ssd1315.h"
#include "mpu6050.h"
#include "keypad4x4.h"
#include "buzzer.h"
#include <string.h>

#define FIELD_X      0
#define FIELD_Y      8
#define FIELD_W      128
#define FIELD_H      56
#define BALL_R       2
#define MAX_OBS      5
#define DT           0.0333f
#define FRAME_MS     33u

typedef enum { ST_SPLASH, ST_MENU, ST_CALIB, ST_PLAYING, ST_PAUSE, ST_GAMEOVER } st_t;

typedef struct { float x, y, vx, vy; } ball_t;
typedef struct { float x, y, w, h, vx, vy; uint8_t kind; } obst_t;

/* ---------------- globals ---------------- */
static st_t state;
static ball_t ball;
static obst_t obs[MAX_OBS];
static uint8_t o_cnt;

static int lives;
static int score;
static int level;
static int high_score;
static uint8_t sound_on;

static float coin_x, coin_y;
static uint8_t coin_alive;

static uint32_t last_ms;
static uint32_t next_obst_ms;
static uint32_t invuln_ms;
static uint32_t splash_t;
static uint32_t over_t;

static int menu_idx;         /* 0 start, 1 calibrate, 2 difficulty, 3 sound */
static int difficulty;       /* 0 easy 1 normal 2 hard */
static uint32_t rng_state;
static uint8_t mpu_ok;
static float trail_x[4], trail_y[4];

static void draw_gameover(void);
static void draw_play(void);

static void rng_seed(void) { rng_state = HAL_GetTick() ^ 0x9E3779B9u; }
static uint32_t rng(void)
{
  rng_state = rng_state * 1664525u + 1013904223u;
  return rng_state >> 8;
}
static int32_t rng_range(int32_t lo, int32_t hi) { return lo + (int32_t)(rng() % (uint32_t)(hi - lo + 1)); }

/* ---------------- helpers ---------------- */
static void led_on(void)  { HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); }
static void led_off(void) { HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET); }

static void ball_reset(void)
{
  ball.x = FIELD_X + FIELD_W / 2.0f;
  ball.y = FIELD_Y + FIELD_H / 2.0f;
  ball.vx = ball.vy = 0.0f;
}

static void coin_spawn(void)
{
  uint8_t try_i;
  coin_alive = 1;
  for (try_i = 0; try_i < 10; try_i++)
  {
    coin_x = (float)rng_range(FIELD_X + 4, FIELD_X + FIELD_W - 4);
    coin_y = (float)rng_range(FIELD_Y + 4, FIELD_Y + FIELD_H - 4);
    {
      float dx = coin_x - ball.x, dy = coin_y - ball.y;
      if (dx * dx + dy * dy > 100.0f) break;
    }
  }
}

static void obst_clear(void) { o_cnt = 0; }

static void obst_spawn(void)
{
  int kind = rng_range(0, 2);
  float spd = 22.0f + (float)level * 9.0f + (float)difficulty * 6.0f;
  float w = (float)rng_range(7, 13);
  float h = (float)rng_range(5, 9);
  uint8_t try_i;
  float x, y, sx, sy, dx, dy;
  int placed = 0;
  if (spd > 68.0f) spd = 68.0f;

  for (try_i = 0; try_i < 6; try_i++)
  {
    switch (kind)
    {
      case 0: /* fall from top: check distance from entering edge */
        x = (float)rng_range(FIELD_X + 4, FIELD_X + FIELD_W - (int)w - 4);
        y = FIELD_Y - h - 2.0f;
        sx = x + w / 2.0f; sy = y + h;
        break;
      case 1: /* from left */
        x = FIELD_X - w - 2.0f;
        y = (float)rng_range(FIELD_Y + 2, FIELD_Y + FIELD_H - (int)h - 2);
        sx = x + w; sy = y + h / 2.0f;
        break;
      default: /* from right */
        x = FIELD_X + FIELD_W + 2.0f;
        y = (float)rng_range(FIELD_Y + 2, FIELD_Y + FIELD_H - (int)h - 2);
        sx = x; sy = y + h / 2.0f;
        break;
    }
    dx = sx - ball.x; dy = sy - ball.y;
    if (dx * dx + dy * dy > 900.0f) { placed = 1; break; }
  }
  if (!placed) return; /* ball too close at every try: skip, spawn later */

  if (o_cnt >= MAX_OBS) return;
  obs[o_cnt].x = x; obs[o_cnt].y = y;
  obs[o_cnt].w = w; obs[o_cnt].h = h;
  obs[o_cnt].vx = (kind == 1) ? spd : (kind == 2) ? -spd : 0.0f;
  obs[o_cnt].vy = (kind == 0) ? spd : 0.0f;
  obs[o_cnt].kind = (uint8_t)kind;
  o_cnt++;
}

static void game_start(void)
{
  ball_reset();
  obst_clear();
  lives = (difficulty == 2) ? 1 : 3;
  score = 0;
  level = 1;
  coin_spawn();
  invuln_ms = 0;
  next_obst_ms = HAL_GetTick() + 2500;
  state = ST_PLAYING;
  last_ms = HAL_GetTick();
  if (sound_on) sfx_ready();
  led_on();
}

/* point-to-segment distance squared */
static float seg_dist2(float px, float py, float x0, float y0, float x1, float y1)
{
  float dx = x1 - x0, dy = y1 - y0;
  float len2 = dx * dx + dy * dy;
  float t, cx, cy, ddx, ddy;

  if (len2 == 0.0f)
  {
    ddx = px - x0; ddy = py - y0;
    return ddx * ddx + ddy * ddy;
  }
  t = ((px - x0) * dx + (py - y0) * dy) / len2;
  if (t < 0.0f) t = 0.0f;
  else if (t > 1.0f) t = 1.0f;
  cx = x0 + t * dx; cy = y0 + t * dy;
  ddx = px - cx; ddy = py - cy;
  return ddx * ddx + ddy * ddy;
}

/* does the ball touch this obstacle? (collision = visual, integer coords) */
static int ball_hits_obst(const obst_t *o)
{
  float cx = (float)(int)ball.x, cy = (float)(int)ball.y;
  float r2 = (float)(BALL_R * BALL_R);
  float x0 = o->x, y0 = o->y;
  float x1 = o->x + o->w - 1.0f, y1 = o->y + o->h - 1.0f;
  int ix, iy;

  if (o->kind == 0)
  {
    /* solid box: nearest point = clamp(ball, box) */
    float nx2, ny2, dx, dy;
    nx2 = cx; if (nx2 < x0) nx2 = x0; else if (nx2 > x1) nx2 = x1;
    ny2 = cy; if (ny2 < y0) ny2 = y0; else if (ny2 > y1) ny2 = y1;
    dx = cx - nx2; dy = cy - ny2;
    return (dx * dx + dy * dy) <= r2;
  }
  if (o->kind == 1)
  {
    /* hollow 1px frame: check 4 segments */
    if (seg_dist2(cx, cy, x0, y0, x1, y0) <= r2) return 1;
    if (seg_dist2(cx, cy, x0, y1, x1, y1) <= r2) return 1;
    if (seg_dist2(cx, cy, x0, y0, x0, y1) <= r2) return 1;
    if (seg_dist2(cx, cy, x1, y0, x1, y1) <= r2) return 1;
    return 0;
  }
  /* dotted: 1px dots, ball edge must touch the dot (r2 = (r+1)^2) */
  for (iy = 0; iy < (int)o->h; iy += 2)
    for (ix = 0; ix < (int)o->w; ix += 2)
    {
      float dx = cx - (o->x + ix), dy = cy - (o->y + iy);
      if (dx * dx + dy * dy <= (float)((BALL_R + 1) * (BALL_R + 1))) return 1;
    }
  return 0;
}

static void update_obstacles(void)
{
  uint8_t i;
  for (i = 0; i < o_cnt; i++)
  {
    obs[i].x += obs[i].vx * DT;
    obs[i].y += obs[i].vy * DT;
  }

  /* recycle off-field obstacles: respawn them as new */
  i = 0;
  while (i < o_cnt)
  {
    obst_t *o = &obs[i];
    int off = (o->x + o->w < FIELD_X) || (o->x > FIELD_X + FIELD_W) ||
              (o->y + o->h < FIELD_Y) || (o->y > FIELD_Y + FIELD_H);
    if (off)
    {
      /* remove by swapping last in */
      o_cnt--;
      if (i != o_cnt) obs[i] = obs[o_cnt];
    }
    else i++;
  }

  /* spawn new */
  if (HAL_GetTick() >= next_obst_ms)
  {
    uint32_t period = 1900u - (uint32_t)(level - 1) * 200u;
    uint8_t limit = (uint8_t)(1 + (level - 1) / 2);
    uint8_t guard = 0;
    if (limit > MAX_OBS) limit = MAX_OBS;
    if (period < 800u) period = 800u;
    while (o_cnt < limit && guard++ < 12) obst_spawn();
    next_obst_ms = HAL_GetTick() + period;
  }
}

static void check_collisions(void)
{
  uint8_t i;

  if (HAL_GetTick() <= invuln_ms) return;
  for (i = 0; i < o_cnt; i++)
  {
    obst_t *o = &obs[i];
    /* obstacle must be visibly on screen (3px margin) before it can hit */
    if (o->x + o->w <= FIELD_X + 3) continue;
    if (o->x >= FIELD_X + FIELD_W - 3) continue;
    if (o->y + o->h <= FIELD_Y + 3) continue;
    if (o->y >= FIELD_Y + FIELD_H - 3) continue;
    if (ball_hits_obst(o))
    {
      /* freeze-frame: show the actual hit moment for 500ms so the
         player can see the ball and obstacle really overlap */
      oled_clear();
      draw_play();
      oled_show();
      HAL_Delay(500);
      lives--;
      obst_clear();
      invuln_ms = HAL_GetTick() + 2200;
      if (sound_on) sfx_hit();
      if (lives <= 0)
      {
        state = ST_GAMEOVER;
        over_t = HAL_GetTick();
        if (sound_on) sfx_gameover();
        led_off();
        draw_gameover();
      }
      else
      {
        ball.x = FIELD_X + FIELD_W / 2.0f;
        ball.y = FIELD_Y + FIELD_H / 2.0f;
        ball.vx = ball.vy = 0.0f;
      }
      return;
    }
  }
}

static void update_ball(void)
{
  float fx, fy;
  uint8_t k;
  float nx = 0.0f, ny = 0.0f;

  mpu_get_tilt(&fx, &fy);

  k = keypad_get_hold();
  if (k == 2) ny = -1.0f;
  else if (k == 8) ny = 1.0f;
  else if (k == 4) nx = -1.0f;
  else if (k == 6) nx = 1.0f;

  ball.vx += (fx * 330.0f + nx * 140.0f) * DT;
  ball.vy += (fy * 330.0f + ny * 140.0f) * DT;

  /* drag */
  ball.vx *= 0.955f;
  ball.vy *= 0.955f;

  /* speed cap (Manhattan, avoids libm) */
  {
    float sp = (ball.vx > 0.0f ? ball.vx : -ball.vx) + (ball.vy > 0.0f ? ball.vy : -ball.vy);
    if (sp > 150.0f)
    {
      float sc = 150.0f / sp;
      ball.vx *= sc; ball.vy *= sc;
    }
  }

  ball.x += ball.vx * DT;
  ball.y += ball.vy * DT;

  /* walls */
  if (ball.x < FIELD_X + BALL_R) { ball.x = FIELD_X + BALL_R; ball.vx = -ball.vx * 0.6f; }
  if (ball.x > FIELD_X + FIELD_W - BALL_R) { ball.x = FIELD_X + FIELD_W - BALL_R; ball.vx = -ball.vx * 0.6f; }
  if (ball.y < FIELD_Y + BALL_R) { ball.y = FIELD_Y + BALL_R; ball.vy = -ball.vy * 0.6f; }
  if (ball.y > FIELD_Y + FIELD_H - BALL_R) { ball.y = FIELD_Y + FIELD_H - BALL_R; ball.vy = -ball.vy * 0.6f; }

  /* coin */
  if (coin_alive)
  {
    float dx = ball.x - coin_x, dy = ball.y - coin_y;
    if (dx * dx + dy * dy < 25.0f)
    {
      coin_alive = 0;
      score += 10;
      if (score > high_score) high_score = score;
      if (score / 50 + 1 > level)
      {
        level = score / 50 + 1;
        if (sound_on) sfx_levelup();
      }
      else if (sound_on) sfx_coin();
      coin_spawn();
    }
  }
}

/* ---------------- rendering ---------------- */
static void draw_ball(void)
{
  /* trail */
  uint8_t i;
  for (i = 0; i < 4; i++)
    oled_set_pixel((int)trail_x[i], (int)trail_y[i], 1);

  oled_fill_circle((int)ball.x, (int)ball.y, BALL_R, 1);

  /* blink when invulnerable */
  if (HAL_GetTick() < invuln_ms && ((HAL_GetTick() / 100) & 1))
    oled_fill_circle((int)ball.x, (int)ball.y, BALL_R, 0);
}

static void draw_hud(void)
{
  int i;
  oled_fill_rect(FIELD_X, 0, FIELD_W, FIELD_Y, 0);
  oled_draw_str(0, 0, "SC", 1);
  oled_draw_num(16, 0, score, 1);
  oled_draw_str(66, 0, "LV", 1);
  oled_draw_num(82, 0, level, 1);
  for (i = 0; i < lives; i++)
    oled_fill_rect(108 + i * 7, 2, 4, 4, 1);
}

static void draw_coin(void)
{
  if (!coin_alive) return;
  oled_set_pixel((int)coin_x, (int)coin_y, 1);
  oled_set_pixel((int)coin_x - 1, (int)coin_y, 1);
  oled_set_pixel((int)coin_x + 1, (int)coin_y, 1);
  oled_set_pixel((int)coin_x, (int)coin_y - 1, 1);
  oled_set_pixel((int)coin_x, (int)coin_y + 1, 1);
}

static void draw_field(void)
{
  uint8_t i;
  oled_draw_rect(FIELD_X, FIELD_Y, FIELD_W, FIELD_H, 1);
  for (i = 0; i < o_cnt; i++)
  {
    if (obs[i].kind == 0)
      oled_fill_rect((int)obs[i].x, (int)obs[i].y, (int)obs[i].w, (int)obs[i].h, 1);
    else if (obs[i].kind == 1)
      oled_draw_rect((int)obs[i].x, (int)obs[i].y, (int)obs[i].w, (int)obs[i].h, 1);
    else
    {
      /* dotted block */
      int ix, iy;
      for (ix = 0; ix < (int)obs[i].w; ix += 2)
        for (iy = 0; iy < (int)obs[i].h; iy += 2)
          oled_set_pixel((int)obs[i].x + ix, (int)obs[i].y + iy, 1);
    }
  }
}

static void draw_play(void)
{
  draw_hud();
  draw_field();
  draw_coin();
  draw_ball();
}

static void draw_splash(void)
{
  oled_clear();
  oled_draw_str_big(40, 8, "BALL", 1);
  oled_draw_str_big(22, 26, "BLASTER", 1);
  oled_draw_str(14, 48, "STM32F103C8T6", 1);
  if (mpu_ok)
    oled_draw_str(0, 57, "MPU6050 OK", 1);
  else
    oled_draw_str(0, 57, "MPU6050 ERR", 1);
  oled_draw_str(64, 57, "KEY5 START", 1);
  oled_show();
}

static void draw_menu(void)
{
  int i;
  const char *items[4] = { "START", "CALIBRATE", "DIFFICULTY", "SOUND" };
  char diff_str[16];
  char snd_str[8];

  if (difficulty == 0) strcpy(diff_str, "EASY");
  else if (difficulty == 1) strcpy(diff_str, "NORMAL");
  else strcpy(diff_str, "HARD");
  strcpy(snd_str, sound_on ? "ON" : "OFF");

  oled_clear();
  oled_draw_str(38, 0, "MAIN MENU", 1);
  for (i = 0; i < 4; i++)
  {
    int y = 12 + i * 12;
    if (i == menu_idx)
    {
      oled_fill_rect(0, y - 1, 128, 10, 1);
      oled_draw_str(6, y, items[i], 0);
      if (i == 2) oled_draw_str(78, y, diff_str, 0);
      if (i == 3) oled_draw_str(78, y, snd_str, 0);
    }
    else
    {
      oled_draw_str(6, y, items[i], 1);
      if (i == 2) oled_draw_str(78, y, diff_str, 1);
      if (i == 3) oled_draw_str(78, y, snd_str, 1);
    }
  }
  oled_draw_str(2, 56, "8/2:move 5:ok", 1);
  oled_show();
}

static void draw_pause(void)
{
  oled_draw_rect(40, 24, 48, 18, 1);
  oled_draw_str(44, 28, "PAUSE", 1);
}

static void draw_gameover(void)
{
  oled_clear();
  oled_draw_str_big(4, 10, "GAME", 1);
  oled_draw_str_big(70, 10, "OVER", 1);
  oled_draw_str(4, 30, "SCORE", 1);
  oled_draw_num(44, 30, score, 1);
  oled_draw_str(78, 30, "BEST", 1);
  oled_draw_num(116, 30, high_score, 1);
  oled_draw_str(4, 44, "5=RETRY 16=MENU", 1);
  oled_show();
}

static void draw_calib(void)
{
  oled_clear();
  oled_draw_str(14, 8, "PUT MODULE FLAT", 1);
  oled_draw_str(20, 20, "THEN PRESS 5", 1);
  if (mpu_ok)
    oled_draw_str(4, 36, "MPU6050: OK", 1);
  else
    oled_draw_str(4, 36, "MPU6050: ERR", 1);
  oled_draw_str(16, 52, "16=CANCEL", 1);
  oled_show();
}

/* ---------------- update ---------------- */
static void splash_tick(void)
{
  uint8_t k = keypad_get_key();
  if (k == 5 || (HAL_GetTick() - splash_t > 3000u))
  {
    state = ST_MENU;
    if (sound_on) sfx_menu_ok();
    draw_menu();
  }
}

static void menu_tick(void)
{
  uint8_t k = keypad_get_key();
  switch (k)
  {
    case 2: case 8:
      /* 2 = up (physically above 8), 8 = down */
      menu_idx = (menu_idx + (k == 2 ? 3 : 1)) % 4;
      if (sound_on) sfx_menu_move();
      draw_menu();
      break;
    case 5:
      if (sound_on) sfx_menu_ok();
      if (menu_idx == 0) { game_start(); }
      else if (menu_idx == 1) { state = ST_CALIB; draw_calib(); }
      else if (menu_idx == 2) { difficulty = (difficulty + 1) % 3; draw_menu(); }
      else { sound_on = !sound_on; draw_menu(); }
      break;
    default:
      break;
  }
}

static void calib_tick(void)
{
  uint8_t k = keypad_get_key();
  if (k == 5)
  {
    mpu_calibrate();
    oled_clear();
    oled_draw_str_big(22, 20, "DONE", 1);
    oled_show();
    HAL_Delay(600);
    state = ST_MENU;
    draw_menu();
  }
  else if (k == 16)
  {
    state = ST_MENU;
    draw_menu();
  }
}

static void play_tick(void)
{
  uint8_t k = keypad_get_key();
  uint32_t fr_start = HAL_GetTick();

  if (k == 5)
  {
    state = ST_PAUSE;
    draw_pause();
    return;
  }
  if (k == 16)
  {
    state = ST_MENU;
    led_off();
    draw_menu();
    return;
  }

  /* fixed timestep */
  {
    uint32_t now = HAL_GetTick();
    uint32_t step = now - last_ms;
    if (step >= FRAME_MS)
    {
      uint8_t i;
      uint8_t n = (uint8_t)(step / FRAME_MS);
      if (n > 3) n = 3;
      for (i = 0; i < n; i++)
      {
        update_ball();
        if (state != ST_PLAYING) return;
        update_obstacles();
        /* only detect collision on the last substep, so the hit
           frame is always the one actually rendered on screen */
        if (i == n - 1) check_collisions();
        if (state != ST_PLAYING) return;
      }
      last_ms = now;

      /* trail */
      for (i = 3; i > 0; i--) { trail_x[i] = trail_x[i-1]; trail_y[i] = trail_y[i-1]; }
      trail_x[0] = ball.x; trail_y[0] = ball.y;
    }
  }

  oled_clear();
  draw_play();
  oled_show();

  /* lock loop to 30fps: one physics step per rendered frame, so a
     collision always happens on a frame the player can actually see */
  {
    uint32_t spent = HAL_GetTick() - fr_start;
    if (spent < FRAME_MS) HAL_Delay(FRAME_MS - spent);
  }
}

static void pause_tick(void)
{
  uint8_t k = keypad_get_key();
  if (k == 5)
  {
    state = ST_PLAYING;
    last_ms = HAL_GetTick();
    if (sound_on) sfx_menu_ok();
  }
  else if (k == 16)
  {
    state = ST_MENU;
    led_off();
    draw_menu();
  }
}

static void gameover_tick(void)
{
  uint8_t k = keypad_get_key();
  if (k == 5 && HAL_GetTick() - over_t > 800u)
  {
    game_start();
  }
  else if (k == 16 && HAL_GetTick() - over_t > 800u)
  {
    state = ST_MENU;
    draw_menu();
  }
  else
  {
    /* blink LED */
    if ((HAL_GetTick() / 250) & 1) led_on(); else led_off();
  }
}

/* ---------------- public ---------------- */
void game_init(void)
{
  uint8_t i;

  rng_seed();

  oled_init();

  /* LED */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  {
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_13;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &g);
  }
  led_off();

  mpu_ok = mpu_init();
  keypad_init();

  state = ST_SPLASH;
  splash_t = HAL_GetTick();
  last_ms = HAL_GetTick();
  menu_idx = 0;
  difficulty = 1;
  sound_on = 1;
  high_score = 0;
  coin_alive = 0;
  for (i = 0; i < 4; i++) { trail_x[i] = -10.0f; trail_y[i] = -10.0f; }

  draw_splash();
}

void game_tick(void)
{
  switch (state)
  {
    case ST_SPLASH:  splash_tick();  break;
    case ST_MENU:    menu_tick();    break;
    case ST_CALIB:   calib_tick();   break;
    case ST_PLAYING: play_tick();    break;
    case ST_PAUSE:   pause_tick();   break;
    case ST_GAMEOVER:gameover_tick();break;
  }
}
