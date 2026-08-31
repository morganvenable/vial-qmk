#include "quantum.h"
#include "pointing_device.h"
#include "pointing_device_internal.h"
#include "axis_scale.h"
#include "i2c_master.h"
#include "drivers/sensors/azoteq_iqs5xx.h"
#include "azoteq.h"
#include "firmware/iqs5xx_fw_image.h"

extern const pointing_device_driver_t *real_device_driver;

/* ------------------------------------------------------------------------
 * TPS43 + 0.7 mm glass calibration.
 *
 * Source: Georg Visagie (Azoteq applications), 2026-04-24, after bonding a
 * TPS43 to 0.7 mm glass and characterising it on a CT210A.  The thin, high-k
 * glass gives ~2x the touch deltas of the 1 mm plastic overlay the stock
 * module settings assume (700-800 counts for an 8 mm touch), so the stock
 * touch thresholds (87 / 65 counts) fire on noise and on environmental
 * drift; he doubled them and made the part fall into ALP (low-power,
 * environment-tracking) mode sooner.  See firmware/README.md.
 *
 * Two layers:
 *   1. If the module does not already carry his settings image (export
 *      version IQS5XX_FW_EXPORT_VERSION at reg 0x0677), flash it through the
 *      IQS5xx I2C bootloader.  One-time per module, ~4 s at boot.
 *   2. Re-apply the runtime-writable registers every boot anyway, because the
 *      stock QMK driver init overwrites the idle-mode timeout (sets 255 s)
 *      and because it costs nothing.
 * ---------------------------------------------------------------------- */

#ifndef AZOTEQ_IQS5XX_ADDRESS
#    define AZOTEQ_IQS5XX_ADDRESS (0x74 << 1)
#endif

#define SVAL_IQS_REG_LP1_REPORT_RATE 0x0580  /* 2 bytes, ms */
#define SVAL_IQS_REG_IDLE_MODE_TIMEOUT 0x0586 /* 1 byte, s */
#define SVAL_IQS_REG_TOUCH_MULT_SET 0x0596    /* set + clear, 1 byte each */
#define SVAL_IQS_REG_END_COMMS 0xEEEE

/* Azoteq-recommended values; override in config.h to experiment. */
#ifndef SVAL_IQS_TOUCH_MULT_SET
#    define SVAL_IQS_TOUCH_MULT_SET IQS5XX_FW_TOUCH_MULT_SET /* 32 (stock ~16) */
#endif
#ifndef SVAL_IQS_TOUCH_MULT_CLEAR
#    define SVAL_IQS_TOUCH_MULT_CLEAR IQS5XX_FW_TOUCH_MULT_CLEAR /* 24 (stock ~12) */
#endif
#ifndef SVAL_IQS_LP1_REPORT_RATE_MS
#    define SVAL_IQS_LP1_REPORT_RATE_MS 50 /* stock 80 */
#endif
/* Seconds of no touch before the part drops from Idle into LP1 (ALP mode).
 * Azoteq: 2 s.  The stock QMK driver sets 255 (never) to avoid the LP1 report
 * rate adding up to SVAL_IQS_LP1_REPORT_RATE_MS of first-touch latency; set
 * this to 255 to get that behaviour back if the wake-up feels laggy. */
#ifndef SVAL_IQS_IDLE_MODE_TIMEOUT_S
#    define SVAL_IQS_IDLE_MODE_TIMEOUT_S 2
#endif
/* Flash the bundled settings image into modules whose export version differs. */
#ifndef SVAL_IQS_AUTOFLASH
#    define SVAL_IQS_AUTOFLASH true
#endif

static i2c_status_t sval_iqs_write(uint16_t reg, const uint8_t *data, uint16_t len) {
    return i2c_write_register16(AZOTEQ_IQS5XX_ADDRESS, reg, data, len, AZOTEQ_IQS5XX_TIMEOUT_MS);
}

static void sval_iqs_end_comms(void) {
    const uint8_t end = 0x00;
    sval_iqs_write(SVAL_IQS_REG_END_COMMS, &end, 1);
}

static i2c_status_t sval_iqs_apply_tuning(void) {
    i2c_status_t status = I2C_STATUS_SUCCESS;

    const uint8_t mult[2] = {SVAL_IQS_TOUCH_MULT_SET, SVAL_IQS_TOUCH_MULT_CLEAR};
    status |= sval_iqs_write(SVAL_IQS_REG_TOUCH_MULT_SET, mult, sizeof(mult));

    const uint8_t lp1[2] = {(SVAL_IQS_LP1_REPORT_RATE_MS >> 8) & 0xFF, SVAL_IQS_LP1_REPORT_RATE_MS & 0xFF};
    status |= sval_iqs_write(SVAL_IQS_REG_LP1_REPORT_RATE, lp1, sizeof(lp1));

    const uint8_t idle_tmo = SVAL_IQS_IDLE_MODE_TIMEOUT_S;
    status |= sval_iqs_write(SVAL_IQS_REG_IDLE_MODE_TIMEOUT, &idle_tmo, 1);

    sval_iqs_end_comms();
    return status;
}

static iqs5xx_identity_t  sval_iqs_id;
static iqs5xx_bl_result_t sval_iqs_last_flash  = IQS5XX_BL_OK;
static bool               sval_iqs_flash_tried = false;

const iqs5xx_identity_t *sval_iqs5xx_identity(void) {
    return &sval_iqs_id;
}

/* Flash the bundled image regardless of version. Returns true on success. */
uint16_t sval_iqs5xx_expected_version(void) {
    return IQS5XX_FW_EXPORT_VERSION;
}

const char *sval_iqs5xx_flash_status_str(void) {
    return sval_iqs_flash_tried ? iqs5xx_bl_result_str(sval_iqs_last_flash) : "not needed";
}

bool sval_iqs5xx_flash_image(void) {
    iqs5xx_bl_result_t r = iqs5xx_bl_program(iqs5xx_fw_image, sizeof(iqs5xx_fw_image));
    sval_iqs_flash_tried = true;
    sval_iqs_last_flash  = r;
    dprintf("IQS5XX: flash %s -> %s\n", IQS5XX_FW_IMAGE_SOURCE, iqs5xx_bl_result_str(r));
    real_device_driver->init();
    iqs5xx_read_identity(&sval_iqs_id);
    return r == IQS5XX_BL_OK;
}

static bool sval_iqs_needs_flash(const iqs5xx_identity_t *id) {
    if (id->product_number != 58 && id->product_number != 40 && id->product_number != 52) return false; /* not an IQS5xx */
    if (id->project_number != 15) return false;                                                        /* not B000 firmware */
    if (id->bootloader_status != 0xA5) return false;                                                   /* no bootloader */
    return id->export_version != IQS5XX_FW_EXPORT_VERSION;
}

/* Re-read the identity (with retries: a single read can land outside the
 * IQS5xx comm window) and, once it is readable, run the auto-flash check and
 * re-apply the runtime tuning. Safe to call repeatedly. */
static bool sval_iqs_link_ok = false;

bool sval_iqs5xx_link_ok(void) {
    return sval_iqs_link_ok;
}

void sval_iqs5xx_refresh(uint8_t attempts) {
    iqs5xx_identity_t tmp;
    bool              ok = false;
    for (uint8_t i = 0; i < attempts && !ok; i++) {
        ok = iqs5xx_read_identity(&tmp);
        if (!ok) wait_ms(10);
    }
    sval_iqs_link_ok = ok;
    if (!ok) {
        /* keep the last good identity; a miss just means we fell outside the
         * device's comm window */
        dprintf("IQS5XX: identity read missed\n");
        return;
    }
    sval_iqs_id = tmp;
    dprintf("IQS5XX: product %u project %u v%u.%u bl 0x%02X settings v%u\n", sval_iqs_id.product_number, sval_iqs_id.project_number, sval_iqs_id.major, sval_iqs_id.minor, sval_iqs_id.bootloader_status, sval_iqs_id.export_version);

#if SVAL_IQS_AUTOFLASH
    if (!sval_iqs_flash_tried && sval_iqs_needs_flash(&sval_iqs_id)) {
        dprintf("IQS5XX: settings v%u != bundled v%u, flashing\n", sval_iqs_id.export_version, IQS5XX_FW_EXPORT_VERSION);
        sval_iqs5xx_flash_image();
        wait_ms(10);
    }
#endif

    i2c_status_t status = sval_iqs_apply_tuning();
    if (status != I2C_STATUS_SUCCESS) {
        wait_ms(10);
        status = sval_iqs_apply_tuning();
    }
    dprintf("IQS5XX: glass tuning %s (i2c status %d)\n", status == I2C_STATUS_SUCCESS ? "applied" : "FAILED", status);
}

void pointing_device_driver_init(void) {
    real_device_driver->init();
    wait_ms(10);
    sval_iqs5xx_refresh(10);
}

#ifdef SVAL_IQS_DIAG
/* TEST DIAGNOSTICS: tally per-touch statistics and type a summary line ~1 s
 * after each touch ends. Enabled only with -DSVAL_IQS_DIAG; do not ship. */
static struct {
    uint32_t cycles, i2c_ok, i2c_fail;
    uint32_t f1, hold_cycles, tap_events;
    uint32_t hold_gaps;  /* hold bit dropped while a finger stayed down */
    uint32_t fail_gaps;  /* i2c read failed while a finger was down */
    uint32_t latch_saves; /* failed reads bridged by the button latch */
    uint32_t palm, reati, ati_err, too_many, rr_missed;
    uint16_t max_cycle_ms;
    bool     active, prev_hold, prev_finger;
    uint32_t last_activity;
} diag;

static void diag_flush(void) {
    char buf[176];
    sprintf(buf, "TPdiag: cyc %lu ok %lu fail %lu | f1 %lu tap %lu hold %lu holdgap %lu failgap %lu latched %lu | palm %lu reati %lu atierr %lu many %lu rrmiss %lu maxct %ums\n",
            (unsigned long)diag.cycles, (unsigned long)diag.i2c_ok, (unsigned long)diag.i2c_fail,
            (unsigned long)diag.f1, (unsigned long)diag.tap_events, (unsigned long)diag.hold_cycles,
            (unsigned long)diag.hold_gaps, (unsigned long)diag.fail_gaps, (unsigned long)diag.latch_saves,
            (unsigned long)diag.palm, (unsigned long)diag.reati, (unsigned long)diag.ati_err,
            (unsigned long)diag.too_many, (unsigned long)diag.rr_missed, diag.max_cycle_ms);
    send_string(buf);
    memset(&diag, 0, sizeof(diag));
}
#endif

/* The IQS5xx only accepts I2C inside its per-cycle comm window, so roughly
 * half of the polled reads fail mid-touch.  The stock driver returns an
 * all-zero report on a failed read, which releases any held button for that
 * cycle -- tap-and-drag flutters.  This replicates the stock report logic but
 * LATCHES the button state across failed reads: buttons only change on data
 * actually read from the device.  A 150 ms staleness cutoff clears the latch
 * so a disconnected pad cannot hold a button down. */
#define SVAL_IQS_LATCH_TIMEOUT_MS 150

report_mouse_t pointing_device_driver_get_report(report_mouse_t mouse_report) {
    static uint8_t  latched_buttons = 0;
    static uint32_t last_ok         = 0;

    report_mouse_t            temp_report     = {0};
    azoteq_iqs5xx_base_data_t bd              = {0};
    i2c_status_t              status          = azoteq_iqs5xx_get_base_data(&bd);
    bool                      ignore_movement = false;

    if (status == I2C_STATUS_SUCCESS) {
        bool hold = bd.gesture_events_0.press_and_hold;
        bool tap  = bd.gesture_events_0.single_tap;

#ifdef SVAL_IQS_DIAG
        diag.i2c_ok++;
        bool finger = bd.number_of_fingers > 0;
        if (finger || hold || tap) {
            diag.active        = true;
            diag.last_activity = timer_read32();
        }
        if (diag.active) {
            diag.cycles++;
            if (bd.number_of_fingers == 1) diag.f1++;
            if (hold) diag.hold_cycles++;
            if (tap) diag.tap_events++;
            if (diag.prev_hold && !hold && finger) diag.hold_gaps++;
            if (bd.system_info_1.palm_detect) diag.palm++;
            if (bd.system_info_0.reati_occurred) diag.reati++;
            if (bd.system_info_0.ati_error) diag.ati_err++;
            if (bd.system_info_1.too_many_fingers) diag.too_many++;
            if (bd.system_info_1.rr_missed) diag.rr_missed++;
            if (bd.previous_cycle_time > diag.max_cycle_ms) diag.max_cycle_ms = bd.previous_cycle_time;
        }
        diag.prev_hold   = hold;
        diag.prev_finger = finger;
#endif

        /* stock driver behaviour */
        if (tap || hold) {
            temp_report.buttons = pointing_device_handle_buttons(temp_report.buttons, true, POINTING_DEVICE_BUTTON1);
        } else if (bd.gesture_events_1.two_finger_tap) {
            temp_report.buttons = pointing_device_handle_buttons(temp_report.buttons, true, POINTING_DEVICE_BUTTON2);
        } else if (bd.gesture_events_1.scroll) {
            temp_report.h = CONSTRAIN_HID(AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(bd.x.h, bd.x.l));
            temp_report.v = CONSTRAIN_HID(AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(bd.y.h, bd.y.l));
        }
        if (bd.number_of_fingers == 1 && !ignore_movement) {
            temp_report.x = CONSTRAIN_HID_XY(AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(bd.x.h, bd.x.l));
            temp_report.y = CONSTRAIN_HID_XY(AZOTEQ_IQS5XX_COMBINE_H_L_BYTES(bd.y.h, bd.y.l));
        }

        latched_buttons = temp_report.buttons;
        last_ok         = timer_read32();
    } else {
        /* Failed read: keep the last known button state instead of releasing. */
        if (timer_elapsed32(last_ok) < SVAL_IQS_LATCH_TIMEOUT_MS) {
            temp_report.buttons = latched_buttons;
        } else {
            latched_buttons = 0;
        }
#ifdef SVAL_IQS_DIAG
        diag.i2c_fail++;
        if (diag.active) {
            diag.cycles++;
            if (diag.prev_finger) diag.fail_gaps++;
            if (temp_report.buttons) diag.latch_saves++;
        }
#endif
    }

#ifdef SVAL_IQS_DIAG
    if (diag.active && timer_elapsed32(diag.last_activity) > 1000) {
        diag.active = false;
        diag_flush();
    }
#endif
    return temp_report;
}
