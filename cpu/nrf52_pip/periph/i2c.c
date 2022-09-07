/*
 * Copyright (C) 2017 HAW Hamburg
 *               2018 Freie Universität Berlin
 *               2018 Mesotic SAS
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_nrf52_pip
 * @{
 *
 * @file
 * @brief       Low-level I2C (TWI) peripheral driver implementation
 *
 * @author      Dimitri Nahm <dimitri.nahm@haw-hamburg.de>
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Dylan Laduranty <dylan.laduranty@mesotic.com>
 *
 * @}
 */

#include <assert.h>
#include <string.h>
#include <errno.h>

#include "cpu.h"
#include "mutex.h"
#include "assert.h"
#include "periph/i2c.h"
#include "periph/gpio.h"

#include "svc.h"

#define ENABLE_DEBUG        0
#include "debug.h"

/**
 * @brief   If any of the 8 lower bits are set, the speed value is invalid
 */
#define INVALID_SPEED_MASK  (0xff)

/**
 * @brief   Initialized dev locks (we have a maximum of two devices...)
 */
static mutex_t locks[I2C_NUMOF];

/**
 * @brief   array with a busy mutex for each I2C device, used to block the
 *          thread until the transfer is done
 */
static mutex_t busy[I2C_NUMOF];

void i2c_isr_handler(void *arg);

static inline uint32_t bus(i2c_t dev)
{
    return i2c_config[dev].dev;
}

static int finish(i2c_t dev)
{
    uint32_t reg;

    DEBUG("[i2c] waiting for STOPPED or ERROR event\n");
    /* Unmask interrupts */
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_INTENSET_INDEX, TWIM_INTEN_STOPPED_Msk | TWIM_INTEN_ERROR_Msk);
    mutex_lock(&busy[dev]);

    Pip_in(bus(dev) + PIP_NRF_TWIM_TWIM1_EVENTS_STOPPED_INDEX, &reg);
    if (reg) {
        Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_EVENTS_STOPPED_INDEX, 0);
        DEBUG("[i2c] finish: stop event occurred\n");
    }

    Pip_in(bus(dev) + PIP_NRF_TWIM_TWIM1_EVENTS_ERROR_INDEX, &reg);
    if (reg) {
        Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_EVENTS_ERROR_INDEX, 0);
        Pip_in(bus(dev) + PIP_NRF_TWIM_TWIM1_ERRORSRC_INDEX, &reg);
        if (reg & TWIM_ERRORSRC_ANACK_Msk) {
            Pip_out(bus(dev) + bus(dev) + PIP_NRF_TWIM_TWIM1_ERRORSRC_INDEX, TWIM_ERRORSRC_ANACK_Msk);
            DEBUG("[i2c] check_error: NACK on address byte\n");
            return -ENXIO;
        }
        Pip_in(bus(dev) + PIP_NRF_TWIM_TWIM1_ERRORSRC_INDEX, &reg);
        if (reg & TWIM_ERRORSRC_DNACK_Msk) {
            Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_ERRORSRC_INDEX, TWIM_ERRORSRC_DNACK_Msk);
            DEBUG("[i2c] check_error: NACK on data byte\n");
            return -EIO;
        }
    }

    return 0;
}

static void _init_pins(i2c_t dev)
{
    gpio_init(i2c_config[dev].scl, GPIO_IN_OD_PU);
    gpio_init(i2c_config[dev].sda, GPIO_IN_OD_PU);
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_PSEL_SCL_INDEX, i2c_config[dev].scl);
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_PSEL_SDA_INDEX, i2c_config[dev].sda);
}

void i2c_init(i2c_t dev)
{
    assert(dev < I2C_NUMOF);

    /* Initialize mutex */
    mutex_init(&locks[dev]);
    mutex_init(&busy[dev]);
    mutex_lock(&busy[dev]);

    /* disable device during initialization, will be enabled when acquire is
     * called */
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_ENABLE_INDEX, TWIM_ENABLE_ENABLE_Disabled);

    /* configure pins */
    _init_pins(dev);

    /* configure dev clock speed */
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_FREQUENCY_INDEX, i2c_config[dev].speed);

    spi_twi_irq_register_i2c(bus(dev), i2c_isr_handler, (void *)dev);

    /* We expect that the device was being acquired before
     * the i2c_init_master() function is called, so it should be enabled when
     * exiting this function. */
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_ENABLE_INDEX, TWIM_ENABLE_ENABLE_Enabled);
}

#ifdef MODULE_PERIPH_I2C_RECONFIGURE
void i2c_init_pins(i2c_t dev)
{
    assert(dev < I2C_NUMOF);

    _init_pins(dev);

    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_ENABLE_INDEX, TWIM_ENABLE_ENABLE_Enabled);

    mutex_unlock(&locks[dev]);
}

void i2c_deinit_pins(i2c_t dev)
{
    assert(dev < I2C_NUMOF);

    mutex_lock(&locks[dev]);
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_ENABLE_INDEX, TWIM_ENABLE_ENABLE_Disabled);
}
#endif /* MODULE_PERIPH_I2C_RECONFIGURE */

int i2c_acquire(i2c_t dev)
{
    assert(dev < I2C_NUMOF);

    mutex_lock(&locks[dev]);
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_ENABLE_INDEX, TWIM_ENABLE_ENABLE_Enabled);

    DEBUG("[i2c] acquired dev %i\n", (int)dev);
    return 0;
}

void i2c_release(i2c_t dev)
{
    assert(dev < I2C_NUMOF);

    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_ENABLE_INDEX, TWIM_ENABLE_ENABLE_Disabled);
    mutex_unlock(&locks[dev]);

    DEBUG("[i2c] released dev %i\n", (int)dev);
}

int i2c_write_regs(i2c_t dev, uint16_t addr, uint16_t reg,
                   const void *data, size_t len, uint8_t flags)
{
    assert((dev < I2C_NUMOF) && data && (len > 0) && (len < 255));

    if (flags & (I2C_NOSTART | I2C_REG16 | I2C_ADDR10)) {
        return -EOPNOTSUPP;
    }
    /* the nrf52's TWI device does not support to do two consecutive transfers
     * without a repeated start condition in between. So we have to put all data
     * to be transferred into a temporary buffer
     *
     * CAUTION: this might become critical when transferring large blocks of
     *          data as the temporary buffer is allocated on the stack... */
    uint8_t buf_tmp[len + 1];
    buf_tmp[0] = reg;
    memcpy(&buf_tmp[1], data, len);
    return i2c_write_bytes(dev, addr, buf_tmp, (len + 1), flags);
}

int i2c_read_bytes(i2c_t dev, uint16_t addr, void *data, size_t len,
                   uint8_t flags)
{
    assert((dev < I2C_NUMOF) && data && (len > 0) && (len < 256));

    if (flags & (I2C_NOSTART | I2C_REG16 | I2C_ADDR10)) {
        return -EOPNOTSUPP;
    }
    DEBUG("[i2c] read_bytes: %i bytes from addr 0x%02x\n", (int)len, (int)addr);

    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_ADDRESS_INDEX, addr);
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_RXD_PTR_INDEX, (uint32_t)data);
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_RXD_MAXCNT_INDEX, (uint8_t)len);

    if (!(flags & I2C_NOSTOP)) {
        Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_SHORTS_INDEX, TWIM_SHORTS_LASTRX_STOP_Msk);
    }
    /* Start transmission */
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_TASKS_STARTRX_INDEX, 1);

    return finish(dev);
}

int i2c_read_regs(i2c_t dev, uint16_t addr, uint16_t reg,
                  void *data, size_t len, uint8_t flags)
{
    uint32_t reg2;

    assert((dev < I2C_NUMOF) && data && (len > 0) && (len < 256));

    if (flags & (I2C_NOSTART | I2C_REG16 | I2C_ADDR10)) {
        return -EOPNOTSUPP;
    }
    DEBUG("[i2c] read_regs: %i byte(s) from reg 0x%02x at addr 0x%02x\n",
           (int)len, (int)reg, (int)addr);

    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_ADDRESS_INDEX, addr);
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_TXD_PTR_INDEX, (uint32_t)&reg);
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_TXD_MAXCNT_INDEX, 1);
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_RXD_PTR_INDEX, (uint32_t)data);
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_RXD_MAXCNT_INDEX, (uint8_t)len);
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_SHORTS_INDEX, TWIM_SHORTS_LASTTX_STARTRX_Msk);
    if (!(flags & I2C_NOSTOP)) {
        Pip_in(bus(dev) + PIP_NRF_TWIM_TWIM1_SHORTS_INDEX, &reg2);
        reg2 |=  TWIM_SHORTS_LASTRX_STOP_Msk;
        Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_SHORTS_INDEX, reg2);
    }
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_TASKS_STARTTX_INDEX, 1);

    return finish(dev);
}

int i2c_write_bytes(i2c_t dev, uint16_t addr, const void *data, size_t len,
                    uint8_t flags)
{
    assert((dev < I2C_NUMOF) && data && (len > 0) && (len < 256));

    if (flags & (I2C_NOSTART | I2C_REG16 | I2C_ADDR10)) {
        return -EOPNOTSUPP;
    }
    DEBUG("[i2c] write_bytes: %i byte(s) to addr 0x%02x\n", (int)len, (int)addr);

    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_ADDRESS_INDEX, addr);
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_TXD_PTR_INDEX, (uint32_t)data);
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_TXD_MAXCNT_INDEX, (uint8_t)len);
    if (!(flags & I2C_NOSTOP)) {
        Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_SHORTS_INDEX, TWIM_SHORTS_LASTTX_STOP_Msk);
    }
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_TASKS_STARTTX_INDEX, 1);

    return finish(dev);
}

void i2c_isr_handler(void *arg)
{
    i2c_t dev = (i2c_t)arg;

    /* Mask interrupts to ensure that they only trigger once */
    Pip_out(bus(dev) + PIP_NRF_TWIM_TWIM1_INTENCLR_INDEX, TWIM_INTEN_STOPPED_Msk | TWIM_INTEN_ERROR_Msk);

    mutex_unlock(&busy[dev]);
}
