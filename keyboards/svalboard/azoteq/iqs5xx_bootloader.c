// Copyright 2026 Svalboard
// SPDX-License-Identifier: GPL-2.0-or-later

#include "iqs5xx_bootloader.h"

#include <string.h>
#include "i2c_master.h"
#include "wait.h"
#include "print.h"
#include "debug.h"
#include "progmem.h"
#include "drivers/sensors/azoteq_iqs5xx.h"

#ifndef AZOTEQ_IQS5XX_ADDRESS
#    define AZOTEQ_IQS5XX_ADDRESS (0x74 << 1)
#endif
#ifndef AZOTEQ_IQS5XX_TIMEOUT_MS
#    define AZOTEQ_IQS5XX_TIMEOUT_MS 10
#endif

/* QMK uses 8-bit (shifted) addresses; the XOR mask shifts with it. */
#define IQS5XX_BL_ADDRESS ((uint8_t)(AZOTEQ_IQS5XX_ADDRESS ^ (0x40 << 1)))

#define IQS5XX_BL_CMD_VER 0x00
#define IQS5XX_BL_CMD_READ 0x01
#define IQS5XX_BL_CMD_EXEC 0x02
#define IQS5XX_BL_CMD_CRC 0x03
#define IQS5XX_BL_ID 0x0200
#define IQS5XX_BL_CRC_PASS 0x00
#define IQS5XX_BL_BLK_LEN 64

#define IQS5XX_CHKSM 0x83C0 /* start of programmable map (checksum block) */
#define IQS5XX_CSTM 0xBE00  /* start of settings/custom region */
#define IQS5XX_PMAP_END 0xBFFF
#define IQS5XX_PMAP_LEN (IQS5XX_PMAP_END + 1 - IQS5XX_CHKSM)

#define IQS5XX_REG_PRODUCT_NUMBER 0x0000 /* product(2) project(2) major(1) minor(1) bl_status(1) */
#define IQS5XX_REG_EXPORT_VERSION 0x0677
#define IQS5XX_REG_END_COMMS 0xEEEE

#define IQS5XX_BL_RESET_ATTEMPTS 3
#define IQS5XX_BL_OPEN_POLLS 60 /* x ~100 us + I2C time: covers the 2 ms boot window generously */
#define IQS5XX_BL_BLOCK_RETRIES 3

static void iqs5xx_end_comms(void) {
    const uint8_t b = 0;
    i2c_write_register16(AZOTEQ_IQS5XX_ADDRESS, IQS5XX_REG_END_COMMS, &b, 1, AZOTEQ_IQS5XX_TIMEOUT_MS);
}

bool iqs5xx_read_identity(iqs5xx_identity_t *id) {
    memset(id, 0, sizeof(*id));
    uint8_t buf[7] = {0};
    if (i2c_read_register16(AZOTEQ_IQS5XX_ADDRESS, IQS5XX_REG_PRODUCT_NUMBER, buf, sizeof(buf), AZOTEQ_IQS5XX_TIMEOUT_MS) != I2C_STATUS_SUCCESS) {
        iqs5xx_end_comms();
        return false;
    }
    uint8_t ver[2] = {0};
    bool    ok     = i2c_read_register16(AZOTEQ_IQS5XX_ADDRESS, IQS5XX_REG_EXPORT_VERSION, ver, sizeof(ver), AZOTEQ_IQS5XX_TIMEOUT_MS) == I2C_STATUS_SUCCESS;
    iqs5xx_end_comms();
    if (!ok) return false;

    id->product_number    = ((uint16_t)buf[0] << 8) | buf[1];
    id->project_number    = ((uint16_t)buf[2] << 8) | buf[3];
    id->major             = buf[4];
    id->minor             = buf[5];
    id->bootloader_status = buf[6];
    id->export_version    = ((uint16_t)ver[0] << 8) | ver[1];
    return true;
}

/* VER: write 0x00, repeated-start read 2 bytes. */
static bool iqs5xx_bl_ver(void) {
    uint8_t id[2] = {0};
    if (i2c_read_register(IQS5XX_BL_ADDRESS, IQS5XX_BL_CMD_VER, id, sizeof(id), 2) != I2C_STATUS_SUCCESS) {
        return false;
    }
    return (((uint16_t)id[0] << 8) | id[1]) == IQS5XX_BL_ID;
}

/* Reset the application and catch the bootloader in its post-reset window. */
static bool iqs5xx_bl_open(void) {
    for (uint8_t attempt = 0; attempt < IQS5XX_BL_RESET_ATTEMPTS; attempt++) {
        /* Maybe we are already in the bootloader (e.g. previous attempt aborted). */
        if (iqs5xx_bl_ver()) return true;

        /* Software reset via System Control 1 (RESET bit); the driver helper does the
         * read-modify-write and closes the comms window so the reset actually fires. */
        i2c_status_t st = I2C_STATUS_ERROR;
        for (uint8_t r = 0; r < 5 && st != I2C_STATUS_SUCCESS; r++) {
            st = azoteq_iqs5xx_reset_suspend(true, false, true);
            if (st != I2C_STATUS_SUCCESS) wait_ms(2);
        }

        for (uint8_t poll = 0; poll < IQS5XX_BL_OPEN_POLLS; poll++) {
            if (iqs5xx_bl_ver()) {
                wait_ms(10);
                return true;
            }
            wait_us(100);
        }
        wait_ms(50);
    }
    return false;
}

static bool iqs5xx_bl_write_block(uint16_t addr, const uint8_t *image, uint16_t off) {
    uint8_t pkt[2 + IQS5XX_BL_BLK_LEN];
    pkt[0] = addr >> 8;
    pkt[1] = addr & 0xFF;
    for (uint8_t i = 0; i < IQS5XX_BL_BLK_LEN; i++) {
        pkt[2 + i] = pgm_read_byte(&image[off + i]);
    }
    for (uint8_t r = 0; r < IQS5XX_BL_BLOCK_RETRIES; r++) {
        if (i2c_transmit(IQS5XX_BL_ADDRESS, pkt, sizeof(pkt), 20) == I2C_STATUS_SUCCESS) {
            wait_ms(10); /* flash write time per block (Linux driver uses 10 ms) */
            return true;
        }
        wait_ms(5);
    }
    return false;
}

static bool iqs5xx_bl_crc_ok(void) {
    const uint8_t cmd = IQS5XX_BL_CMD_CRC;
    if (i2c_transmit(IQS5XX_BL_ADDRESS, &cmd, 1, 5) != I2C_STATUS_SUCCESS) return false;
    wait_ms(50);
    uint8_t res = 0xFF;
    for (uint8_t r = 0; r < 5; r++) {
        if (i2c_receive(IQS5XX_BL_ADDRESS, &res, 1, 5) == I2C_STATUS_SUCCESS) {
            return res == IQS5XX_BL_CRC_PASS;
        }
        wait_ms(10);
    }
    return false;
}

static bool iqs5xx_bl_verify_block(uint16_t addr, const uint8_t *image, uint16_t off) {
    const uint8_t cmd[3] = {IQS5XX_BL_CMD_READ, addr >> 8, addr & 0xFF};
    uint8_t       rd[IQS5XX_BL_BLK_LEN];
    if (i2c_transmit(IQS5XX_BL_ADDRESS, cmd, sizeof(cmd), 5) != I2C_STATUS_SUCCESS) return false;
    if (i2c_receive(IQS5XX_BL_ADDRESS, rd, sizeof(rd), 10) != I2C_STATUS_SUCCESS) return false;
    for (uint8_t i = 0; i < IQS5XX_BL_BLK_LEN; i++) {
        if (rd[i] != pgm_read_byte(&image[off + i])) return false;
    }
    return true;
}

static bool iqs5xx_bl_exec(void) {
    const uint8_t cmd = IQS5XX_BL_CMD_EXEC;
    bool          ok  = i2c_transmit(IQS5XX_BL_ADDRESS, &cmd, 1, 5) == I2C_STATUS_SUCCESS;
    wait_ms(10);
    return ok;
}

iqs5xx_bl_result_t iqs5xx_bl_program(const uint8_t *image, uint16_t image_len) {
    if (image_len != IQS5XX_PMAP_LEN) return IQS5XX_BL_ERR_WRITE;

    dprintf("IQS5XX BL: entering bootloader\n");
    if (!iqs5xx_bl_open()) return IQS5XX_BL_ERR_NO_BOOTLOADER;

    dprintf("IQS5XX BL: writing %u bytes\n", (unsigned)image_len);
    for (uint16_t off = 0; off < image_len; off += IQS5XX_BL_BLK_LEN) {
        if (!iqs5xx_bl_write_block(IQS5XX_CHKSM + off, image, off)) {
            dprintf("IQS5XX BL: block write failed at 0x%04X\n", IQS5XX_CHKSM + off);
            return IQS5XX_BL_ERR_WRITE;
        }
    }

    if (!iqs5xx_bl_crc_ok()) {
        dprintf("IQS5XX BL: CRC failed\n");
        return IQS5XX_BL_ERR_CRC; /* leave it in the bootloader; a reset will retry */
    }

    bool verified = true;
    for (uint16_t addr = IQS5XX_CSTM; addr <= IQS5XX_PMAP_END; addr += IQS5XX_BL_BLK_LEN) {
        if (!iqs5xx_bl_verify_block(addr, image, addr - IQS5XX_CHKSM)) {
            dprintf("IQS5XX BL: verify mismatch at 0x%04X\n", addr);
            verified = false;
            break;
        }
    }

    if (!iqs5xx_bl_exec()) return IQS5XX_BL_ERR_EXEC;
    wait_ms(100); /* application boot */
    dprintf("IQS5XX BL: done (%s)\n", verified ? "verified" : "CRC ok, verify mismatch");
    return verified ? IQS5XX_BL_OK : IQS5XX_BL_ERR_VERIFY;
}

const char *iqs5xx_bl_result_str(iqs5xx_bl_result_t r) {
    switch (r) {
        case IQS5XX_BL_OK:
            return "ok";
        case IQS5XX_BL_ERR_NO_BOOTLOADER:
            return "no bootloader";
        case IQS5XX_BL_ERR_WRITE:
            return "write failed";
        case IQS5XX_BL_ERR_CRC:
            return "crc failed";
        case IQS5XX_BL_ERR_VERIFY:
            return "verify mismatch";
        case IQS5XX_BL_ERR_EXEC:
            return "exec failed";
    }
    return "?";
}
