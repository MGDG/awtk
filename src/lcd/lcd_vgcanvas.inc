﻿/**
 * File:   lcd.c
 * Author: Li XianJing <xianjimli@hotmail.com>
 * Brief:  vgcanvasi implemented lcd_t
 *
 * Copyright (c) 2018 - 2019  Guangzhou ZHIYUAN Electronics Co.,Ltd.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * License file for more details.
 *
 */

/**
 * History:
 * ================================================================
 * 2018-04-11 Li XianJing <xianjimli@hotmail.com> created
 *
 */

#include "base/lcd.h"
#include "tkc/utf8.h"
#include "base/vgcanvas.h"
#include "base/system_info.h"

typedef struct _lcd_vgcanvas_t {
  lcd_t base;

  vgcanvas_t* canvas;
} lcd_vgcanvas_t;

#define LCD(lcd) ((lcd_vgcanvas_t*)lcd)

static ret_t lcd_vgcanvas_resize(lcd_t* lcd, wh_t w, wh_t h, uint32_t line_length) {
  LCD(lcd)->canvas->w = w;
  LCD(lcd)->canvas->h = h;
  (void)line_length;

  return RET_OK;
}

static ret_t lcd_vgcanvas_set_font_name(lcd_t* lcd, const char* name) {
  return vgcanvas_set_font(LCD(lcd)->canvas, name);
}

static ret_t lcd_vgcanvas_set_font_size(lcd_t* lcd, uint32_t size) {
  return vgcanvas_set_font_size(LCD(lcd)->canvas, size);
}

static ret_t lcd_vgcanvas_begin_frame(lcd_t* lcd, rect_t* dirty_rect) {
  return vgcanvas_begin_frame(LCD(lcd)->canvas, dirty_rect);
}

static ret_t lcd_vgcanvas_set_clip_rect(lcd_t* lcd, rect_t* r) {
  return vgcanvas_clip_rect(LCD(lcd)->canvas, r->x, r->y, r->w, r->h);
}

static ret_t lcd_vgcanvas_get_clip_rect(lcd_t* lcd, rect_t* r) {
  vgcanvas_t* canvas = LCD(lcd)->canvas;
  *r = canvas->clip_rect;

  return RET_OK;
}

static ret_t lcd_vgcanvas_set_global_alpha(lcd_t* lcd, uint8_t alpha) {
  return vgcanvas_set_global_alpha(LCD(lcd)->canvas, (float_t)alpha / 255.0);
}

static ret_t lcd_vgcanvas_draw_vline(lcd_t* lcd, xy_t x, xy_t y, wh_t h) {
  vgcanvas_t* canvas = LCD(lcd)->canvas;

  vgcanvas_begin_path(canvas);
  vgcanvas_move_to(canvas, x, y);
  vgcanvas_line_to(canvas, x, y + h);
  vgcanvas_set_line_width(canvas, 1);
  vgcanvas_set_stroke_color(canvas, lcd->stroke_color);
  vgcanvas_stroke(canvas);

  return RET_OK;
}

static ret_t lcd_vgcanvas_draw_hline(lcd_t* lcd, xy_t x, xy_t y, wh_t w) {
  vgcanvas_t* canvas = LCD(lcd)->canvas;

  vgcanvas_begin_path(canvas);
  vgcanvas_move_to(canvas, x, y);
  vgcanvas_line_to(canvas, x + w, y);
  vgcanvas_set_line_width(canvas, 1);
  vgcanvas_set_stroke_color(canvas, lcd->stroke_color);
  vgcanvas_stroke(canvas);

  return RET_OK;
}

static ret_t lcd_vgcanvas_fill_rect(lcd_t* lcd, xy_t x, xy_t y, wh_t w, wh_t h) {
  vgcanvas_t* canvas = LCD(lcd)->canvas;

  vgcanvas_begin_path(canvas);
  vgcanvas_rect(canvas, x, y, w, h);
  vgcanvas_set_fill_color(canvas, lcd->fill_color);
  vgcanvas_fill(canvas);

  return RET_OK;
}

static ret_t lcd_vgcanvas_stroke_rect(lcd_t* lcd, xy_t x, xy_t y, wh_t w, wh_t h) {
  vgcanvas_t* canvas = LCD(lcd)->canvas;

  vgcanvas_begin_path(canvas);
  vgcanvas_rect(canvas, x, y, w, h);
  vgcanvas_set_line_width(canvas, 1);
  vgcanvas_set_stroke_color(canvas, lcd->stroke_color);
  vgcanvas_stroke(canvas);

  return RET_OK;
}

static ret_t lcd_vgcanvas_draw_points(lcd_t* lcd, point_t* points, uint32_t nr) {
  uint32_t i = 0;
  vgcanvas_t* canvas = LCD(lcd)->canvas;
  vgcanvas_set_stroke_color(canvas, lcd->stroke_color);

  vgcanvas_begin_path(canvas);
  for (i = 0; i < nr; i++) {
    float x = points[i].x;
    float y = points[i].y;

    vgcanvas_move_to(canvas, x, y);
    vgcanvas_line_to(canvas, x, y);
  }
  vgcanvas_stroke(canvas);

  return RET_OK;
}

static ret_t lcd_vgcanvas_draw_image(lcd_t* lcd, bitmap_t* img, rect_t* src, rect_t* dst) {
  int sx = src->x;
  int sy = src->y;
  int sw = src->w;
  int sh = src->h;
  int dx = dst->x;
  int dy = dst->y;
  int dw = dst->w;
  int dh = dst->h;
  vgcanvas_t* canvas = LCD(lcd)->canvas;

  return vgcanvas_draw_image(canvas, img, sx, sy, sw, sh, dx, dy, dw, dh);
}

static ret_t lcd_vgcanvas_draw_image_matrix(lcd_t* lcd, draw_image_info_t* info) {
  matrix_t* m = &(info->matrix);
  rect_t* s = &(info->src);
  rect_t* d = &(info->dst);
  vgcanvas_t* canvas = LCD(lcd)->canvas;
  vgcanvas_save(canvas);
  vgcanvas_set_transform(canvas, m->a0, m->a1, m->a2, m->a3, m->a4, m->a5);
  vgcanvas_draw_image(canvas, info->img, s->x, s->y, s->w, s->h, d->x, d->y, d->w, d->h);
  vgcanvas_restore(canvas);

  return RET_OK;
}

#define MAX_CHARS 256
float_t lcd_vgcanvas_measure_text(lcd_t* lcd, const wchar_t* str, uint32_t nr) {
  wchar_t temp[MAX_CHARS + 1];
  char text[2 * MAX_CHARS + 1];
  vgcanvas_t* canvas = LCD(lcd)->canvas;
  return_value_if_fail(nr < MAX_CHARS, RET_FAIL);

  if (nr == 0) {
    return 0;
  }

  wcsncpy(temp, str, nr);
  temp[nr] = 0;

  utf8_from_utf16(temp, text, sizeof(text) - 1);

  vgcanvas_set_font(canvas, lcd->font_name);
  vgcanvas_set_font_size(canvas, lcd->font_size);
  return vgcanvas_measure_text(LCD(lcd)->canvas, text);
}

static ret_t lcd_vgcanvas_draw_text(lcd_t* lcd, const wchar_t* str, uint32_t nr, xy_t x, xy_t y) {
  wchar_t temp[MAX_CHARS + 1];
  char text[2 * MAX_CHARS + 1];
  vgcanvas_t* canvas = LCD(lcd)->canvas;
  return_value_if_fail(nr < MAX_CHARS, RET_FAIL);

  if (nr == 0) {
    return RET_OK;
  }

  wcsncpy(temp, str, nr);
  temp[nr] = 0;

  utf8_from_utf16(temp, text, sizeof(text) - 1);

  vgcanvas_set_font(canvas, lcd->font_name);
  vgcanvas_set_font_size(canvas, lcd->font_size);
  vgcanvas_set_fill_color(canvas, lcd->text_color);
  vgcanvas_set_text_align(canvas, "left");
  vgcanvas_set_text_baseline(canvas, "top");

  return vgcanvas_fill_text(canvas, text, x, y, 0xffff);
}

wh_t lcd_vgcanvas_get_width(lcd_t* lcd) {
  vgcanvas_t* canvas = LCD(lcd)->canvas;

  return vgcanvas_get_width(canvas);
}

wh_t lcd_vgcanvas_get_height(lcd_t* lcd) {
  vgcanvas_t* canvas = LCD(lcd)->canvas;

  return vgcanvas_get_height(canvas);
}

static ret_t lcd_vgcanvas_end_frame(lcd_t* lcd) {
  return vgcanvas_end_frame(LCD(lcd)->canvas);
}

static ret_t lcd_vgcanvas_destroy(lcd_t* lcd) {
  (void)lcd;
  return RET_OK;
}

static vgcanvas_t* lcd_vgcanvas_get_vgcanvas(lcd_t* lcd) {
  vgcanvas_t* canvas = LCD(lcd)->canvas;
  vgcanvas_begin_path(canvas);

  return canvas;
}

static bitmap_format_t lcd_vgcanvas_get_desired_bitmap_format(lcd_t* lcd) {
  return BITMAP_FMT_RGBA8888;
}

lcd_t* lcd_vgcanvas_init(wh_t w, wh_t h, vgcanvas_t* canvas) {
  static lcd_vgcanvas_t lcd;
  lcd_t* base = &(lcd.base);
  system_info_t* info = system_info();
  return_value_if_fail(canvas != NULL, NULL);

  memset(&lcd, 0x00, sizeof(lcd_vgcanvas_t));

  base->begin_frame = lcd_vgcanvas_begin_frame;
  base->set_clip_rect = lcd_vgcanvas_set_clip_rect;
  base->get_clip_rect = lcd_vgcanvas_get_clip_rect;
  base->draw_vline = lcd_vgcanvas_draw_vline;
  base->draw_hline = lcd_vgcanvas_draw_hline;
  base->fill_rect = lcd_vgcanvas_fill_rect;
  base->stroke_rect = lcd_vgcanvas_stroke_rect;
  base->draw_image = lcd_vgcanvas_draw_image;
  base->draw_image_matrix = lcd_vgcanvas_draw_image_matrix;
  base->draw_points = lcd_vgcanvas_draw_points;
  base->draw_text = lcd_vgcanvas_draw_text;
  base->measure_text = lcd_vgcanvas_measure_text;
  base->end_frame = lcd_vgcanvas_end_frame;
  base->get_vgcanvas = lcd_vgcanvas_get_vgcanvas;
  base->set_font_name = lcd_vgcanvas_set_font_name;
  base->set_font_size = lcd_vgcanvas_set_font_size;
  base->set_global_alpha = lcd_vgcanvas_set_global_alpha;
  base->get_desired_bitmap_format = lcd_vgcanvas_get_desired_bitmap_format;
  base->resize = lcd_vgcanvas_resize;
  base->get_width = lcd_vgcanvas_get_width;
  base->get_height = lcd_vgcanvas_get_height;
  base->destroy = lcd_vgcanvas_destroy;

  base->w = (wh_t)w;
  base->h = (wh_t)h;
  base->ratio = canvas->ratio;
  base->type = LCD_VGCANVAS;
  base->support_dirty_rect = FALSE;

  system_info_set_lcd_w(info, base->w);
  system_info_set_lcd_h(info, base->h);
  system_info_set_lcd_type(info, base->type);
  system_info_set_device_pixel_ratio(info, canvas->ratio);

  lcd.canvas = canvas;

  return base;
}
