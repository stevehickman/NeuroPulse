/*
 * NeurOne Hub Control Program — the status indicators and stealth mode (REQ-FWHUB-84, REQ-FWHUB-85)
 * See np_status_led.h.
 */
#include "np_status_led.h"

#include "np_gatt_server.h"
#include "np_transport.h"   /* np_transport_hal_enter/exit_critical */

_Static_assert(NP_GATT_STEALTH_LEN == 1U, "wire length is the generated constant");

typedef enum { ACT_IDLE, ACT_SESSION, ACT_FAULT } activity_t;

static np_status_led_out_fn s_out;
static activity_t           s_act;
static bool                 s_stealth;
static np_led_state_t       s_shown = NP_LED_IDLE;

/* Caller holds the critical section. */
static void show_locked(void)
{
    np_led_state_t want;
    if (s_act == ACT_FAULT) {
        want = NP_LED_FAULT;              /* a fault always fires, stealth or not */
    } else if (s_stealth) {
        want = NP_LED_OFF;
    } else {
        want = (s_act == ACT_SESSION) ? NP_LED_SESSION : NP_LED_IDLE;
    }
    s_shown = want;
    if (s_out != NULL) {
        s_out(want);
    }
}

void np_status_led_bind(np_status_led_out_fn out)
{
    np_transport_hal_enter_critical();
    s_out = out;
    np_transport_hal_exit_critical();
}

void np_status_led_init(void)
{
    np_transport_hal_enter_critical();
    s_act     = ACT_IDLE;
    s_stealth = false;                    /* every boot is visible: stealth is RAM only */
    show_locked();
    np_transport_hal_exit_critical();
}

void np_status_led_session_started(void)
{
    np_transport_hal_enter_critical();
    s_act = ACT_SESSION;
    show_locked();
    np_transport_hal_exit_critical();
}

void np_status_led_session_ended(np_hub_status_t rc)
{
    np_transport_hal_enter_critical();
    s_act = (rc == NP_HUB_OK || rc == NP_HUB_ERR_STOPPED) ? ACT_IDLE : ACT_FAULT;
    show_locked();
    np_transport_hal_exit_critical();
}

void np_status_led_fault(void)
{
    np_transport_hal_enter_critical();
    s_act = ACT_FAULT;
    show_locked();
    np_transport_hal_exit_critical();
}

void np_status_led_set_stealth(bool on)
{
    np_transport_hal_enter_critical();
    s_stealth = on;
    show_locked();
    np_transport_hal_exit_critical();
}

bool np_status_led_stealth(void)
{
    np_transport_hal_enter_critical();
    const bool v = s_stealth;
    np_transport_hal_exit_critical();
    return v;
}

np_led_state_t np_status_led_shown(void)
{
    np_transport_hal_enter_critical();
    const np_led_state_t v = s_shown;
    np_transport_hal_exit_critical();
    return v;
}

np_hub_status_t np_status_led_read(uint8_t *buf, size_t cap, size_t *len_out)
{
    if (buf == NULL || len_out == NULL || cap < NP_GATT_STEALTH_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    buf[0] = np_status_led_stealth() ? (uint8_t)NP_STEALTH_FLAG_ON : 0U;
    *len_out = NP_GATT_STEALTH_LEN;
    return NP_HUB_OK;
}

np_hub_status_t np_status_led_write(const uint8_t *data, size_t len)
{
    if (data == NULL || len != NP_GATT_STEALTH_LEN) {
        return NP_HUB_ERR_INVALID_ARG;
    }
    if ((data[0] & (uint8_t)~NP_STEALTH_FLAG_ON) != 0U) {
        return NP_HUB_ERR_INVALID_ARG;    /* a bit this firmware does not know is not half-understood */
    }
    np_status_led_set_stealth((data[0] & NP_STEALTH_FLAG_ON) != 0U);
    return NP_HUB_OK;
}
