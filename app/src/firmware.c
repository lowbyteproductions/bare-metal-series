#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/vector.h>

#define LED_PORT      (GPIOA)
#define LED_PIN       (GPIO5)

#define CPU_FREQ      (84000000)
#define SYSTICK_FREQ  (1000)

volatile uint64_t ticks = 0;
void sys_tick_handler(void) {
  ticks++;
}

static uint64_t get_ticks(void) {
  return ticks;
}

static void rcc_setup(void) {
  rcc_clock_setup_pll(&rcc_hsi_configs[RCC_CLOCK_3V3_84MHZ]);
}

static void gpio_setup(void) {
  rcc_periph_clock_enable(RCC_GPIOA);
  gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
}

static void systick_setup(void) {
  systick_set_frequency(SYSTICK_FREQ, CPU_FREQ);
  systick_counter_enable();
  systick_interrupt_enable();
}

int main(void) {
  rcc_setup();
  gpio_setup();

  systick_setup();

  uint64_t start_time = get_ticks();

  while (1) {
    if (get_ticks() - start_time >= 100) {
      gpio_toggle(LED_PORT, LED_PIN);
      start_time = get_ticks();
    }

    // Do useful work
  }

  // Never return
  return 0;
}
