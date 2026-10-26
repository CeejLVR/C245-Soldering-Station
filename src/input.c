/* src/input.c */
#include "input.h"

#include "hardware/gpio.h"

#include "queue.h"
#include "timers.h"

#include "PinDefinitions.h"
#include "app_config.h"

static QueueHandle_t s_queue;

/* Encoder */
// interrupts on both edges of A and B. the table gives +1/-1 for valid steps
// and 0 for bounces, then 4 steps = 1 click
static const int8_t QTAB[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };
static volatile uint8_t s_prev_ab;
static volatile int8_t s_acc;

static inline uint8_t read_ab(void) {
    return (uint8_t)((gpio_get(PIN_ENCODER_A) << 1) | gpio_get(PIN_ENCODER_B));
}

void input_encoder_isr(void) {
    uint8_t ab = read_ab();
    s_acc += QTAB[(s_prev_ab << 2) | ab];
    s_prev_ab = ab;

    input_event_t ev;
    if (s_acc >= ENCODER_TRANSITIONS_PER_DETENT) {
        s_acc = 0;
        ev = ENCODER_REVERSE ? EV_ENC_CCW : EV_ENC_CW;
    } else if (s_acc <= -ENCODER_TRANSITIONS_PER_DETENT) {
        s_acc = 0;
        ev = ENCODER_REVERSE ? EV_ENC_CW : EV_ENC_CCW;
    } else return;

    BaseType_t woke = pdFALSE;
    xQueueSendFromISR(s_queue, &ev, &woke);
    portYIELD_FROM_ISR(woke);
}

/* Buttons */
// checked every 5ms from a software timer. counter goes up while held and down
// while released, so it only counts as pressed after 4 samples in a row
typedef struct {
    uint8_t pin;
    input_event_t short_ev, long_ev;
    uint8_t integ;
    bool pressed;
    bool long_sent;
    uint32_t held_ms;
} button_t;

static button_t s_buttons[] = {
    { PIN_ENCODER_BUTTON, EV_PSH, EV_PSH_LONG, 0, false, false, 0 },
    { PIN_BUTTON_CON, EV_CON, EV_CON_LONG, 0, false, false, 0 },
    { PIN_BUTTON_BAK, EV_BAK, EV_BAK_LONG, 0, false, false, 0 },
};

static void post(input_event_t ev) {
    xQueueSend(s_queue, &ev, 0);
}

static void button_timer_cb(TimerHandle_t t) {
    (void)t;
    for (unsigned i = 0; i < sizeof s_buttons / sizeof s_buttons[0]; i++) {
        button_t *b = &s_buttons[i];
        bool raw_down = !gpio_get(b->pin);  // active low
        if (raw_down && b->integ < BUTTON_DEBOUNCE_SAMPLES) b->integ++;
        if (!raw_down && b->integ > 0) b->integ--;

        if (!b->pressed && b->integ == BUTTON_DEBOUNCE_SAMPLES) {
            b->pressed = true;
            b->long_sent = false;
            b->held_ms = 0;
        } else if (b->pressed && b->integ == 0) {
            // released. only send a short press if we didnt already send a long one
            b->pressed = false;
            if (!b->long_sent) post(b->short_ev);
        } else if (b->pressed) {
            b->held_ms += BUTTON_POLL_MS;
            if (!b->long_sent && b->held_ms >= BUTTON_LONG_PRESS_MS) {
                b->long_sent = true;
                post(b->long_ev);
            }
        }
    }
}

void input_init(void) {
    s_queue = xQueueCreate(16, sizeof(input_event_t));
    configASSERT(s_queue);

    const uint pins[] = { PIN_ENCODER_A, PIN_ENCODER_B, PIN_ENCODER_BUTTON, PIN_BUTTON_CON, PIN_BUTTON_BAK };
    for (unsigned i = 0; i < sizeof pins / sizeof pins[0]; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }
    s_prev_ab = read_ab();
    gpio_set_irq_enabled(PIN_ENCODER_A, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(PIN_ENCODER_B, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);

    TimerHandle_t t = xTimerCreate("buttons", pdMS_TO_TICKS(BUTTON_POLL_MS), pdTRUE, NULL, button_timer_cb);
    configASSERT(t);
    xTimerStart(t, 0);
}

bool input_get_event(input_event_t *ev, TickType_t wait) {
    return xQueueReceive(s_queue, ev, wait) == pdTRUE;
}
