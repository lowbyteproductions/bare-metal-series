#include "core/system.h"
#include "core/simple-timer.h"

void simple_timer_setup(simple_timer_t* timer, uint64_t wait_time, bool auto_reset) {
  timer->wait_time = wait_time;
  timer->auto_reset = auto_reset;
  timer->target_time = system_get_ticks() + wait_time;
  timer->has_elapsed = false;
}

bool simple_timer_has_elapsed(simple_timer_t* timer) {
  uint64_t now = system_get_ticks();
  bool has_elapsed = now >= timer->target_time;

  if (timer->has_elapsed) return false;

  if (has_elapsed) {
    if (timer->auto_reset) {
      uint64_t drift = now - timer->target_time;
      timer->target_time = (now + timer->wait_time) - drift;
    } else {
      timer->has_elapsed = true;
    }
  }

  return has_elapsed;
}

void simple_timer_reset(simple_timer_t* timer) {
  simple_timer_setup(timer, timer->wait_time, timer->auto_reset);
}
