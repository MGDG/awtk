/**
 * File:   input_method_default.c
 * Author: AWTK Develop Team
 * Brief:  input method default
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
 * 2018-06-19 Li XianJing <xianjimli@hotmail.com> created
 *
 */

#include "tkc/mem.h"
#include "base/idle.h"
#include "widgets/window.h"
#include "keyboard/keyboard.h"
#include "suggest_words.inc"
#include "base/input_method.h"
#include "base/window_manager.h"
#include "ui_loader/ui_builder_default.h"

static const char* input_type_to_keyboard_name(int32_t input_type) {
  const char* keyboard = NULL;

  switch (input_type) {
    case INPUT_PHONE: {
      keyboard = "kb_phone";
      break;
    }
    case INPUT_INT: {
      keyboard = "kb_int";
      break;
    }
    case INPUT_FLOAT: {
      keyboard = "kb_float";
      break;
    }
    case INPUT_UINT: {
      keyboard = "kb_uint";
      break;
    }
    case INPUT_UFLOAT: {
      keyboard = "kb_ufloat";
      break;
    }
    case INPUT_HEX: {
      keyboard = "kb_hex";
      break;
    }
    case INPUT_CUSTOM: {
      log_debug("custom keyboard\n");
      break;
    }
    case INPUT_EMAIL:
    case INPUT_PASSWORD: {
      keyboard = "kb_ascii";
      break;
    }
    default: {
      keyboard = "kb_default";
      break;
    }
  }

  return keyboard;
}

static ret_t on_push_window(void* ctx, event_t* e) {
  input_method_t* im = (input_method_t*)ctx;
  im->win->y -= im->win_delta_y;
  widget_invalidate_force(im->win, NULL);

  return RET_REMOVE;
}

static ret_t input_method_default_restore_win_position(input_method_t* im) {
  if (im->win != NULL) {
    im->win->y += im->win_delta_y;
    widget_invalidate_force(im->win, NULL);
    im->win = NULL;
  }

  return RET_OK;
}

static ret_t input_method_on_win_close(void* ctx, event_t* e) {
  input_method_t* im = (input_method_t*)ctx;

  input_method_default_restore_win_position(im);

  if (im->keyboard != NULL) {
    window_manager_close_window_force(im->keyboard->parent, im->keyboard);
    im->keyboard = NULL;
  }

  im->widget = NULL;
  im->win = NULL;
  (void)e;

  return RET_REMOVE;
}

static ret_t input_method_default_on_keyboard_open(void* ctx, event_t* e) {
  input_method_t* im = (input_method_t*)ctx;
  im->busy = FALSE;

  return RET_REMOVE;
}

static ret_t input_type_open_keyboard(input_method_t* im, int32_t input_type, bool_t open_anim) {
  value_t v;
  point_t p = {0, 0};
  widget_t* widget = im->widget;
  widget_t* win = widget_get_window(widget);
  const char* close_anim_hint = WINDOW_ANIMATOR_POPUP;
  const char* open_anim_hint = open_anim ? close_anim_hint : "";
  const char* keyboard = input_type_to_keyboard_name(input_type);

  if (keyboard == NULL) {
    return RET_OK;
  }

  widget_to_screen(widget, &p);
  im->keyboard = window_open(keyboard);
  if (im->keyboard == NULL) {
    log_debug("Not found keyboard [%s], try default.\n", keyboard);
    im->keyboard = window_open("kb_default");
  }
  return_value_if_fail(im->keyboard != NULL, RET_NOT_FOUND);

  im->busy = TRUE;
  widget_on(im->keyboard, EVT_WINDOW_OPEN, input_method_default_on_keyboard_open, im);

  if ((p.y + widget->h) > im->keyboard->y) {
    close_anim_hint = "vtranslate";
    open_anim_hint = close_anim_hint;

    im->win = win;
    im->win_delta_y = win->y + win->h - im->keyboard->y;
    widget_on(im->keyboard, EVT_WINDOW_OPEN, on_push_window, im);
  } else if (win->y + win->h < im->keyboard->y) {
    /*when new keyboard height is not eq old keyboard height*/
    close_anim_hint = "vtranslate";
    im->win_delta_y += win->y + win->h - im->keyboard->y;
    win->y = im->keyboard->y - win->h;
    widget_invalidate_force(im->win, NULL);
  } else if (p.y < 0) {
    im->win = win;
    im->win->y += im->win_delta_y;
    im->win_delta_y = 0;
    widget_invalidate_force(im->win, NULL);
  }

  value_set_str(&v, open_anim_hint);
  widget_set_prop(im->keyboard, WIDGET_PROP_OPEN_ANIM_HINT, &v);
  value_set_str(&v, close_anim_hint);
  widget_set_prop(im->keyboard, WIDGET_PROP_CLOSE_ANIM_HINT, &v);

  widget_on(win, EVT_WINDOW_CLOSE, input_method_on_win_close, im);

  return RET_OK;
}

static ret_t input_method_default_show_keyboard(input_method_t* im) {
  value_t v;
  point_t p = {0, 0};
  int32_t input_type = 0;
  bool_t open_anim = TRUE;
  widget_t* widget = im->widget;

  value_set_int(&v, 0);
  widget_get_prop(im->widget, WIDGET_PROP_INPUT_TYPE, &v);
  input_type = value_int(&v);

  widget_to_screen(widget, &p);
  if (im->keyboard != NULL) {
    bool_t edit_is_in_view = (p.y >= 0) && ((p.y + widget->h) <= im->keyboard->y);

    if (im->input_type == input_type && edit_is_in_view) {
      /*old edit and new edit has same input type, reuse old keyboard*/

      return RET_OK;
    } else {
      /*keyboard is open, close old one, disable open animation of new one*/
      value_set_str(&v, "");
      widget_set_prop(im->keyboard, WIDGET_PROP_OPEN_ANIM_HINT, &v);
      widget_set_prop(im->keyboard, WIDGET_PROP_CLOSE_ANIM_HINT, &v);
      keyboard_close(im->keyboard);
      im->keyboard = NULL;

      open_anim = FALSE;
    }
  }
  im->input_type = (input_type_t)input_type;

  return input_type_open_keyboard(im, input_type, open_anim);
}

typedef struct _idle_close_info_t {
  widget_t* widget;
  widget_t* keyboard;
  input_method_t* im;
} idle_close_info_t;

static ret_t on_idle_close_keyboard(const idle_info_t* idle) {
  idle_close_info_t* info = (idle_close_info_t*)(idle->ctx);
  input_method_t* im = info->im;

  if (im->keyboard == info->keyboard && im->widget == info->widget) {
    input_method_default_restore_win_position(im);
    keyboard_close(im->keyboard);
    im->keyboard = NULL;
    im->widget = NULL;
  }
  TKMEM_FREE(info);

  return RET_OK;
}

static ret_t input_method_default_request(input_method_t* im, widget_t* widget) {
  if (im->widget == widget) {
    return RET_OK;
  }

  input_engine_reset_input(im->engine);
  input_method_dispatch_candidates(im, "", 0);

  if (widget != NULL) {
    im->widget = widget;
    input_method_default_show_keyboard(im);
  } else {
    if (im->keyboard != NULL) {
      idle_close_info_t* info = TKMEM_ZALLOC(idle_close_info_t);
      /*do not close immediately, so we have chance to reuse it.*/
      if (info != NULL) {
        info->im = im;
        info->widget = im->widget;
        info->keyboard = im->keyboard;

        idle_add(on_idle_close_keyboard, info);
      } else {
        keyboard_close(im->keyboard);
        im->keyboard = NULL;
        im->widget = NULL;
      }
    }
  }

  return RET_OK;
}

ret_t input_method_default_destroy(input_method_t* im) {
  return_value_if_fail(im != NULL, RET_BAD_PARAMS);

  emitter_deinit(&(im->emitter));
  input_engine_destroy(im->engine);
  suggest_words_destroy(im->suggest_words);

  TKMEM_FREE(im);

  return RET_OK;
}

input_method_t* input_method_default_create(void) {
  input_method_t* im = TKMEM_ZALLOC(input_method_t);
  return_value_if_fail(im != NULL, NULL);

  im->action_button_enable = TRUE;
  im->request = input_method_default_request;
  im->destroy = input_method_default_destroy;

  emitter_init(&(im->emitter));
  im->engine = input_engine_create();
  im->suggest_words = suggest_words_create((const asset_info_t*)(data_suggest_words));

  return im;
}
