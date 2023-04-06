/*
 * Copyright (C) 2016 Freie Universität Berlin
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
 * @brief       nRF52832 interrupt vector definitions
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdint.h>

#include "cpu.h"
#include "vectors_cortexm.h"

#include "adt.h"
#include "context.h"
#include "svc.h"

extern vidt_t *riotVidt;
extern void *riotPartDesc;

/* Minimum stack size. @see Figure B1-3 Alignment options when stacking the basic frame. */
#define DUMMY_STACK_SIZE 0x24
static char dummy_stack[DUMMY_STACK_SIZE];

/* define a local dummy handler as it needs to be in the same compilation unit
 * as the alias definition */
void dummy_handler(void) {
    dummy_handler_default();
}

/* nRF52 specific interrupt vectors */
WEAK_DEFAULT void isr_power_clock(void);
WEAK_DEFAULT void isr_radio(void);
WEAK_DEFAULT void isr_uart0(void);
WEAK_DEFAULT void isr_spi0(void);
WEAK_DEFAULT void isr_spi0_twi0(void);
WEAK_DEFAULT void isr_spi1_twi0(void);
WEAK_DEFAULT void isr_spi1_twi1(void);
WEAK_DEFAULT void isr_nfct(void);
WEAK_DEFAULT void isr_gpiote(void);
WEAK_DEFAULT void isr_saadc(void);
WEAK_DEFAULT void isr_timer0(void);
WEAK_DEFAULT void isr_timer1(void);
WEAK_DEFAULT void isr_timer2(void);
WEAK_DEFAULT void isr_rtc0(void);
WEAK_DEFAULT void isr_temp(void);
WEAK_DEFAULT void isr_twi0(void);
WEAK_DEFAULT void isr_rng(void);
WEAK_DEFAULT void isr_ecb(void);
WEAK_DEFAULT void isr_ccm_aar(void);
WEAK_DEFAULT void isr_wdt(void);
WEAK_DEFAULT void isr_rtc1(void);
WEAK_DEFAULT void isr_qdec(void);
WEAK_DEFAULT void isr_lpcomp(void);
WEAK_DEFAULT void isr_swi0(void);
WEAK_DEFAULT void isr_swi1(void);
WEAK_DEFAULT void isr_swi2(void);
WEAK_DEFAULT void isr_swi3(void);
WEAK_DEFAULT void isr_swi4(void);
WEAK_DEFAULT void isr_swi5(void);
WEAK_DEFAULT void isr_timer3(void);
WEAK_DEFAULT void isr_timer4(void);
WEAK_DEFAULT void isr_pwm0(void);
WEAK_DEFAULT void isr_pdm(void);
WEAK_DEFAULT void isr_mwu(void);
WEAK_DEFAULT void isr_pwm1(void);
WEAK_DEFAULT void isr_pwm2(void);
WEAK_DEFAULT void isr_spi2(void);
WEAK_DEFAULT void isr_rtc2(void);
WEAK_DEFAULT void isr_i2s(void);
WEAK_DEFAULT void isr_fpu(void);

/**
 * @brief   Handlers for each nRF52832 interrupt.
 */
static isr_t __attribute((used)) nrf52_pip_handlers[CPU_IRQ_NUMOF] = {
    isr_power_clock,       /* power_clock */
    isr_radio,             /* radio */
    isr_uart0,             /* uart0 */
    isr_spi0_twi0,         /* spi0_twi0 */
    isr_spi1_twi1,         /* spi1_twi1 */
    isr_nfct,              /* nfct */
    isr_gpiote,            /* gpiote */
    isr_saadc,             /* adc */
    isr_timer0,            /* timer0 */
    isr_timer1,            /* timer1 */
    isr_timer2,            /* timer2 */
    isr_rtc0,              /* rtc0 */
    isr_temp,              /* temp */
    isr_rng,               /* rng */
    isr_ecb,               /* ecb */
    isr_ccm_aar,           /* ccm_aar */
    isr_wdt,               /* wdt */
    isr_rtc1,              /* rtc1 */
    isr_qdec,              /* qdec */
    isr_lpcomp,            /* lpcomp */
    isr_swi0,              /* swi0 */
    isr_swi1,              /* swi1 */
    isr_swi2,              /* swi2 */
    isr_swi3,              /* swi3 */
    isr_swi4,              /* swi4 */
    isr_swi5,              /* swi5 */
    isr_timer3,            /* timer 3 */
    isr_timer4,            /* timer 4 */
    isr_pwm0,              /* pwm 0 */
    isr_pdm,               /* pdm */
    NULL,                  /* reserved */
    NULL,                  /* reserved */
    isr_mwu,               /* mwu */
    isr_pwm1,              /* pwm 1 */
    isr_pwm2,              /* pwm 2 */
    isr_spi2,              /* spi 2 */
    isr_rtc2,              /* rtc 2 */
    isr_i2s,               /* i2s */
    isr_fpu,               /* fpu */
};

/**
 * @brief   Interrupt dispatcher for each nRF52832 interrupt.
 */
void __attribute__((naked)) nrf52_pip_dispatcher(void)
{
    /*
     * We use r4-r8 because they are callee-saved registers.
     */
    __asm__ volatile
    (
        "ldr    r4, .L1                \n"
        "ldr    r4, [r10, r4]          \n"
        "ldr    r4, [r4]               \n"
        "ldr    r5, [r4, #40]          \n"
        "ldr    r6, [r5]               \n"
        "cmp    r6, #0                 \n"
        "ittee  eq                     \n"
        "ldreq  r6, [r5, #72]          \n"
        "subeq  r6, #0x68              \n"
        "ldrne  r6, [r5, #8]           \n"
        "subne  r6, #0x20              \n"
        "bic    r6, #4                 \n"
        "ite    eq                     \n"
        "subeq  r6, #108               \n"
        "subne  r6, #44                \n"
        "ldr    r5, [r5, #4]           \n"
        "str    r5, [r6, #4]           \n"
        "mov    sp, r6                 \n"
        "ldr    r5, [r4]               \n"
        "subs   r5, #16                \n"
        "ldr    r6, .L1+4              \n"
        "ldr    r6, [r10, r6]          \n"
        "ldr    r6, [r6, r5, lsl #2]   \n"
        "blx    r6                     \n"
        "str    sp, [r4, #4]           \n"
        "ldr    r4, .L1+8              \n"
        "ldr    r0, [r10, r4]          \n"
        "ldr    r0, [r0]               \n"
        "movs   r1, #0                 \n"
        "movs   r2, #46                \n"
        "movs   r3, #0                 \n"
        "movs   r4, #0                 \n"
        "svc    #12                    \n"
        "b      .                      \n"
        ".align 2                      \n"
        ".L1:                          \n"
        ".word riotVidt(GOT)           \n"
        ".word nrf52_pip_handlers(GOT) \n"
        ".word riotPartDesc(GOT)       \n"
    );
}

/**
 * @brief   Context for each nRF52832 interrupt.
 */
static basicContext_t nrf52_pip_ctx = {
    .isBasicFrame = 1,
    /* We must not be interrupted in exception handler. */
    .pipflags = 0,
    .frame = {
        .sp = (uint32_t)dummy_stack + DUMMY_STACK_SIZE,
        .r4 = 0,
        .r5 = 0,
        .r6 = 0,
        .r7 = 0,
        .r8 = 0,
        .r9 = 0,
        /* The R10 will be initialized in the
	 * start() function because its value will
	 * only be known at runtime. */
        .r10 = 0,
        .r11 = 0,
        .r0 = 0,
        .r1 = 0,
        .r2 = 0,
        .r3 = 0,
        .r12 = 0,
        .lr = 0,
        .pc = (uint32_t) nrf52_pip_dispatcher,
        .xpsr = 0
    }
};

/**
 * @brief   Initialize the context for each nRF52832 interrupt
 *          with values only known at runtime.
 *
 * @param sp The address of the stack pointer.
 *
 * @param sl The address of the GOT.
 */
void nrf52_pip_ctx_init(void *sp, void *sl)
{
    (void)sp;
    nrf52_pip_ctx.frame.r10 = (uint32_t) sl;
}

/**
 * @brief   Initialize the entries of the VIDT corresponding to
 *          the interrupts of the nRF52832 with the address of
 *          the handler's context.
 *
 * @param vidt The address of the VIDT of RIOT.
 */
void nrf52_pip_vidt_init(vidt_t *vidt)
{
    for (size_t i = 0; i < CPU_IRQ_NUMOF; i++) {
        vidt->contexts[i + 16] = &nrf52_pip_ctx;
    }
    /* reserved */
    vidt->contexts[46] = NULL;
    vidt->contexts[47] = NULL;
}
