#ifndef CPS_KEYMAP_H
#define CPS_KEYMAP_H

#include "stdint.h"
#include "keycode.h"
#include "set.h"
#include "keyboard.hpp"

typedef struct
{
  uint8_t scan_keys[5];
  uint8_t scan_key_len;
  uint8_t trigger_keys[5];
  uint8_t trigger_key_len;
  uint8_t key_allow_insert[8];
  uint8_t key_allow_insert_len;
  uint8_t matched;
  uint8_t unmatched;
  bool has_triggered;
} composite_key_t;

#define DEFAULT_CPS_BASE .matched = 0,   \
                         .unmatched = 0, \
                         .has_triggered = false

#define COMMON_INSERT_KEY_LEN 8
#define COMMON_INSERT_KEY                                            \
  {                                                                  \
    KC_LCTRL, KC_RCTRL, KC_LSHIFT, KC_RSHIFT, KC_W, KC_A, KC_S, KC_D \
  }

#define composite_key_len 4
composite_key_t composite_keymap[composite_key_len] = {
    {.scan_keys = {KC_HOME, KC_W},
     .scan_key_len = 2,
     .trigger_keys = {KC_UP},
     .trigger_key_len = 1,
     .key_allow_insert = COMMON_INSERT_KEY,
     .key_allow_insert_len = COMMON_INSERT_KEY_LEN,
     DEFAULT_CPS_BASE},

    {.scan_keys = {KC_HOME, KC_A},
     .scan_key_len = 2,
     .trigger_keys = {KC_LEFT},
     .trigger_key_len = 1,
     .key_allow_insert = COMMON_INSERT_KEY,
     .key_allow_insert_len = COMMON_INSERT_KEY_LEN,
     DEFAULT_CPS_BASE},

    {.scan_keys = {KC_HOME, KC_S},
     .scan_key_len = 2,
     .trigger_keys = {KC_DOWN},
     .trigger_key_len = 1,
     .key_allow_insert = COMMON_INSERT_KEY,
     .key_allow_insert_len = COMMON_INSERT_KEY_LEN,
     DEFAULT_CPS_BASE},

    {.scan_keys = {KC_HOME, KC_D},
     .scan_key_len = 2,
     .trigger_keys = {KC_RIGHT},
     .trigger_key_len = 1,
     .key_allow_insert = COMMON_INSERT_KEY,
     .key_allow_insert_len = COMMON_INSERT_KEY_LEN,
     DEFAULT_CPS_BASE}};

extern uint8_t key_pressed_num;

Set press_trigger_key_set;
Set blocked_press_key_set;

bool check_in_allow_insert_key_list(composite_key_t *cps_key, uint8_t keycode)
{
  uint8_t i;
  for (i = 0; i < cps_key->key_allow_insert_len; i++)
    if (cps_key->key_allow_insert[i] == keycode)
      return true;
  return false;
}

void trigger_composite_key(uint8_t keycode, bool pressed)
{
  uint8_t i, j;
  bool block = false;
  bool is_short_recover_key = false;

  composite_key_t *cps_key;
  for (i = 0; i < composite_key_len; i++)
  {
    cps_key = &composite_keymap[i];

    if (pressed)
    {
      if (cps_key->scan_keys[cps_key->matched] == keycode)
      {
        cps_key->matched++;
        // 阻塞该默认
        block = true;
        blocked_press_key_set.add(keycode);
        // 判断时候打到触发条件
        if (cps_key->matched == cps_key->scan_key_len && cps_key->unmatched == 0)
        {
          // 触发组合键, 一次性按下所有的按键
          for (j = 0; j < cps_key->trigger_key_len; j++)
            if (press_trigger_key_set.add(cps_key->trigger_keys[j]))
              send_key(cps_key->trigger_keys[j], pressed);

          cps_key->has_triggered = true;
        }
      }

      // 非scan_key和insert_key
      else if (!check_in_allow_insert_key_list(cps_key, keycode))
      {
        cps_key->unmatched++;
        // 现在组合键处于未触发状态, 则释放被阻塞的按钮, 会重复触发, 不该在这里触发
        if (cps_key->matched != cps_key->trigger_key_len)
          for (j = 0; j < cps_key->unmatched; j++)
            // 把已经match, 那些被阻塞的, 取消阻塞
            // press home (block home) -> press g (unblock home press home press g)
            if (blocked_press_key_set.remove(cps_key->scan_keys[i]))
              send_key(cps_key->scan_keys[i], true);
      }
    }
    else // release
    {
      // 检测是否属于组合键的扫描键
      if (cps_key->scan_keys[cps_key->matched - 1] == keycode)
      {
        cps_key->matched--;
        block = true;
        blocked_press_key_set.remove(keycode);

        // 当已经触发过的时候
        if (cps_key->has_triggered)
        {
          // 慢释放
          // 需要释放的keycode
          j = cps_key->trigger_key_len - (cps_key->scan_key_len - cps_key->matched);
          if (j >= 0 && press_trigger_key_set.remove(cps_key->trigger_keys[j]))
            send_key(cps_key->trigger_keys[j], false);

          // 当没有键被按下的话, 把所有剩下的key都释放
          if (cps_key->matched == 0)
            for (j = 0; j < cps_key->trigger_key_len - cps_key->scan_key_len; j++)
              if (press_trigger_key_set.remove(cps_key->trigger_keys[j]))
                send_key(cps_key->trigger_keys[j], false);
        }

        // 未触发的时候释放了中间状态
        // 仅仅触发了一个的时候
        else if (key_pressed_num == 1)
          is_short_recover_key = true;
      }

      // 非scan_key和非insert key
      else if (!check_in_allow_insert_key_list(cps_key, keycode))
      {
        cps_key->unmatched--;

        // 如果减去之后会恢复match的阻塞状态
        if (cps_key->unmatched == 0)
          for (j = 0; j < cps_key->matched; j++)
            if (blocked_press_key_set.add(cps_key->scan_keys[j]))
              send_key(cps_key->scan_keys[j], false);
      }
    }
  }

  if (!block)
  {
    send_key(keycode, pressed);
  }

  if (is_short_recover_key)
  {
    send_key(keycode, true);
    delay(20);
    send_key(keycode, false);
  }
}

#endif