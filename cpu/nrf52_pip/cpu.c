/*
 * Copyright (C) 2015 Jan Wagner <mail@jwagner.eu>
 *               2016 Freie Universität Berlin
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
 * @brief       Implementation of the CPU initialization
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Jan Wagner <mail@jwagner.eu>
 *
 * @}
 */

#define DONT_OVERRIDE_NVIC

#include "cpu.h"
#include "nrfx.h"
#include "nrf_clock.h"
#include "periph_conf.h"
#include "periph/init.h"
#include "stdio_base.h"

#include "svc.h"

/* FTPAN helper functions */
static bool ftpan_32(void);
static bool ftpan_37(void);
static bool ftpan_36(void);

/**
 * @brief   Initialize the CPU, set IRQ priorities
 */
void cpu_init(void)
{
    uint32_t reg;

    /* Workaround for FTPAN-32
     * "DIF: Debug session automatically enables TracePort pins." */
    if (ftpan_32()) {
        Pip_in(PIP_ARMV7M_SCS_SCID_DEMCR, &reg);
        reg &= ~CoreDebug_DEMCR_TRCENA_Msk;
        Pip_out(PIP_ARMV7M_SCS_SCID_DEMCR, reg);
    }

    /* Workaround for FTPAN-37
     * "AMLI: EasyDMA is slow with Radio, ECB, AAR and CCM." */
    if (ftpan_37()) {
        Pip_out(PIP_NRF_RADIO_RADIO_ERRATA_32, 0x3);
    }

    /* Workaround for FTPAN-36
     * "CLOCK: Some registers are not reset when expected." */
    if (ftpan_36()) {
        Pip_in(PIP_NRF_CLOCK_CLOCK_EVENTS_DONE, 0);
        Pip_in(PIP_NRF_CLOCK_CLOCK_EVENTS_CTTO, 0);
    }
    /* Enable the DC/DC power converter */
    nrfx_dcdc_init();

    /* initialize hf clock */
    clock_init_hf();

#ifdef NVMC_ICACHECNF_CACHEEN_Msk
    /* enable instruction cache */
    Pip_out(PIP_NRF_NVMC_NVMC_ICACHECNF, NVMC_ICACHECNF_CACHEEN_Msk);
#endif

    /* call cortexm default initialization */
    cortexm_init();

    /* enable wake up on events for __WFE CPU sleep */
    Pip_in(PIP_ARMV7M_SCS_SCID_SCR, &reg);
    reg |= SCB_SCR_SEVONPEND_Msk;
    Pip_out(PIP_ARMV7M_SCS_SCID_SCR, reg);

    /* initialize stdio prior to periph_init() to allow use of DEBUG() there */
    stdio_init();

    /* trigger static peripheral initialization */
    periph_init();
}

/**
 * @brief   Check workaround for FTPAN-32
 */
static bool ftpan_32(void)
{
    uint32_t reg;
    Pip_in(PIP_NRF_ERRATA_REG0, &reg);
    if ((reg & 0x000000FF) == 0x6) {
      Pip_in(PIP_NRF_ERRATA_REG1, &reg);
      if ((reg & 0x0000000F) == 0x0) {
        Pip_in(PIP_NRF_ERRATA_REG2, &reg);
        if ((reg & 0x000000F0) == 0x30) {
          Pip_in(PIP_NRF_ERRATA_REG3, &reg);
          if ((reg & 0x000000F0) == 0x0) {
            return true;
	  }
        }
      }
    }
    return false;
}

/**
 * @brief   Check workaround for FTPAN-36
 */
static bool ftpan_36(void)
{
    uint32_t reg;
    Pip_in(PIP_NRF_ERRATA_REG0, &reg);
    if ((reg & 0x000000FF) == 0x6) {
      Pip_in(PIP_NRF_ERRATA_REG1, &reg);
      if ((reg & 0x0000000F) == 0x0) {
        Pip_in(PIP_NRF_ERRATA_REG2, &reg);
        if ((reg & 0x000000F0) == 0x30) {
          Pip_in(PIP_NRF_ERRATA_REG3, &reg);
          if ((reg & 0x000000F0) == 0x0) {
            return true;
	  }
        }
      }
    }
    return false;
}

/**
 * @brief   Check workaround for FTPAN-37
 */
static bool ftpan_37(void)
{
    uint32_t reg;
    Pip_in(PIP_NRF_ERRATA_REG0, &reg);
    if ((reg & 0x000000FF) == 0x6) {
      Pip_in(PIP_NRF_ERRATA_REG1, &reg);
      if ((reg & 0x0000000F) == 0x0) {
        Pip_in(PIP_NRF_ERRATA_REG2, &reg);
        if ((reg & 0x000000F0) == 0x30) {
          Pip_in(PIP_NRF_ERRATA_REG3, &reg);
          if ((reg & 0x000000F0) == 0x0) {
            return true;
	  }
        }
      }
    }
    return false;
}
