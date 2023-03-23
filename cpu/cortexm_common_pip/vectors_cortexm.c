/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_cortexm_common_pip
 * @{
 *
 * @file
 * @brief       Default implementations for Cortex-M specific interrupt and
 *              exception handlers
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Daniel Krebs <github@daniel-krebs.net>
 * @author      Joakim Gebart <joakim.gebart@eistec.se>
 * @author      Sören Tempel <tempel@uni-bremen.de>
 *
 * @}
 */

#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

#include "cpu.h"
#include "periph_cpu.h"
#include "kernel_init.h"
#include "board.h"
#include "mpu.h"
#include "panic.h"
#include "sched.h"
#include "vectors_cortexm.h"
#ifdef MODULE_PUF_SRAM
#include "puf_sram.h"
#endif
#ifdef MODULE_DBGPIN
#include "dbgpin.h"
#endif

#include "interface.h"
#include "context.h"
#include "svc.h"

#ifndef SRAM_BASE
#define SRAM_BASE 0
#endif

#ifndef CPU_BACKUP_RAM_NOT_RETAINED
#define CPU_BACKUP_RAM_NOT_RETAINED 0
#endif

#define CORTEX_IRQ_NUMOF 15

/**
 * @brief   Memory markers, defined in the linker script
 * @{
 */
uint32_t *_sstack;
extern uint32_t _estack;
extern uint8_t _sram;
extern uint8_t _eram;
/** @} */

void *riotPartDesc;
void *riotGotAddr;
vidt_t *riotVidt;

/**
 * @brief   Allocation of the interrupt stack
 */
__attribute__((used,section(".isr_stack"))) uint8_t isr_stack[ISR_STACKSIZE];

/**
 * @brief   Pre-start routine for CPU-specific settings
 */
__attribute__((weak)) void pre_startup (void)
{
}

/**
 * @brief   Post-start routine for CPU-specific settings
 */
__attribute__((weak)) void post_startup (void)
{
}

/**
 * @brief   Function that will be called by the crt0.
 */
void start(interface_t *interface, void *gotaddr)
{
    /* initialization of the heap */
    extern void heap_init(void *start, void *end);
    heap_init(interface->unusedRamStart, interface->ramEnd);

    /* initialization of global variables with
     * values only known at runtime */
    _sstack = (uint32_t *) interface->stackLimit;

    extern void cortexm_pip_ctx_init(void *sp, void *sl);
    cortexm_pip_ctx_init(interface->stackTop, gotaddr);

    extern void cortexm_pip_vidt_init(vidt_t *vidt);
    cortexm_pip_vidt_init(interface->vidtStart);

    extern void nrf52_pip_ctx_init(void *sp, void *sl);
    nrf52_pip_ctx_init(interface->stackTop, gotaddr);

    extern void nrf52_pip_vidt_init(vidt_t *vidt);
    nrf52_pip_vidt_init(interface->vidtStart);

    riotPartDesc = interface->partDescBlockId;

    riotVidt = interface->vidtStart;

    riotGotAddr = gotaddr;

    Pip_setIntState(1);

    /* call RIOT entry point */
    reset_handler_default();
}

void reset_handler_default(void)
{
    uint32_t *dst;

    pre_startup();

#ifdef DEVELHELP
    /* cppcheck-suppress constVariable
     * (top is modified by asm) */
    uint32_t *top;
    /* Fill stack space with canary values up until the current stack pointer */
    /* Read current stack pointer from CPU register */
    __asm__ volatile ("mov %[top], sp" : [top] "=r" (top) : : );
    dst = _sstack;
    while (dst < top) {
        *(dst++) = STACK_CANARY_WORD;
    }
#endif

    post_startup();

#ifdef MODULE_DBGPIN
    dbgpin_init();
#endif

    /* initialize the CPU */
    extern void cpu_init(void);
    cpu_init();

    /* initialize the board (which also initiates CPU initialization) */
    board_init();

#if MODULE_NEWLIB || MODULE_PICOLIBC
    /* initialize std-c library (this must be done after board_init) */
    extern void __libc_init_array(void);
    __libc_init_array();
#endif

    /* startup the kernel */
    kernel_init();
}

__attribute__((weak))
void nmi_handler(void)
{
    core_panic(PANIC_NMI_HANDLER, "NMI HANDLER");
}

void hard_fault_default(void)
{
    core_panic(PANIC_HARD_FAULT, "HARD FAULT HANDLER");
}

#if defined(CPU_CORE_CORTEX_M3) || defined(CPU_CORE_CORTEX_M33) || \
    defined(CPU_CORE_CORTEX_M4) || defined(CPU_CORE_CORTEX_M4F) || \
    defined(CPU_CORE_CORTEX_M7)
void mem_manage_default(void)
{
    core_panic(PANIC_MEM_MANAGE, "MEM MANAGE HANDLER");
}

void bus_fault_default(void)
{
    core_panic(PANIC_BUS_FAULT, "BUS FAULT HANDLER");
}

void usage_fault_default(void)
{
    core_panic(PANIC_USAGE_FAULT, "USAGE FAULT HANDLER");
}

void debug_mon_default(void)
{
    core_panic(PANIC_DEBUG_MON, "DEBUG MON HANDLER");
}
#endif

void dummy_handler_default(void)
{
    core_panic(PANIC_DUMMY_HANDLER, "DUMMY HANDLER");
}

/* Cortex-M common interrupt vectors */
__attribute__((weak, alias("dummy_handler_default"))) void isr_svc(void);
__attribute__((weak, alias("dummy_handler_default"))) void isr_pendsv(void);
__attribute__((weak, alias("dummy_handler_default"))) void isr_systick(void);

/**
 * @brief   Handlers for each Cortex-M interrupt.
 */
static const isr_t
cortexm_pip_handlers[CORTEX_IRQ_NUMOF] =
{
    [ 0] = reset_handler_default,
    [ 1] = nmi_handler,
    [ 2] = hard_fault_default,
    [10] = isr_svc,
    [13] = isr_pendsv,
    [14] = isr_systick,

    #ifdef CORTEXM_VECTOR_RESERVED_0X1C
    [6] = (isr_t)(CORTEXM_VECTOR_RESERVED_0X1C),
    #endif  /* CORTEXM_VECTOR_RESERVED_0X1C */
    #ifdef CORTEXM_VECTOR_RESERVED_0X20
    [7] = (isr_t)(CORTEXM_VECTOR_RESERVED_0X20),
    #endif  /* CORTEXM_VECTOR_RESERVED_0X20 */
    #ifdef CORTEXM_VECTOR_RESERVED_0X24
    [8] = (isr_t)(CORTEXM_VECTOR_RESERVED_0X24),
    #endif  /* CORTEXM_VECTOR_RESERVED_0X24 */
    #ifdef CORTEXM_VECTOR_RESERVED_0X28
    [9] = (isr_t)(CORTEXM_VECTOR_RESERVED_0X28),
    #endif  /* CORTEXM_VECTOR_RESERVED_0X28 */

#if defined(CPU_CORE_CORTEX_M3) || defined(CPU_CORE_CORTEX_M33) || \
    defined(CPU_CORE_CORTEX_M4) || defined(CPU_CORE_CORTEX_M4F) || \
    defined(CPU_CORE_CORTEX_M7)
    [ 3] = mem_manage_default,
    [ 4] = bus_fault_default,
    [ 5] = usage_fault_default,
    [11] = debug_mon_default,
#endif
};

/**
 * @brief   Interrupt dispatcher for each Cortex-M interrupt.
 */
void __attribute__((noreturn)) cortexm_pip_dispatcher(void)
{
    cortexm_pip_handlers[riotVidt->currentInterrupt - 1]();
    for (;;);
}

/**
 * @brief   Context for each Cortex-M interrupt.
 */
static basicContext_t cortexm_pip_ctx = {
    .isBasicFrame = 1,
    /* We must not be interrupted in exception handler. */
    .pipflags = 0,
    .frame = {
        /* the SP field will be initialized in the
	 * start() function because its value will
	 * only be known at runtime */
        .sp = 0,
        .r4 = 0,
        .r5 = 0,
        .r6 = 0,
        .r7 = 0,
        .r8 = 0,
        .r9 = 0,
        /* the R10 field will be initialized in the
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
        .pc = (uint32_t) cortexm_pip_dispatcher,
        .xpsr = 0
    }
};

/**
 * @brief   Initialize the context for each Cortex-M interrupt
 *          with values only known at runtime.
 *
 * @param sp The address of the stack pointer.
 *
 * @param sl The address of the GOT.
 */
void cortexm_pip_ctx_init(void *sp, void *sl)
{
    cortexm_pip_ctx.frame.sp = (uint32_t) sp;
    cortexm_pip_ctx.frame.r10 = (uint32_t) sl;
}

/**
 * @brief   Initialize the entries of the VIDT corresponding to
 *          the interrupts of the Cortex-M with the address of
 *          the handler's context.
 *
 * @param vidt The address of the VIDT of RIOT.
 */
void cortexm_pip_vidt_init(vidt_t *vidt)
{
    /* the index 0 is reserved for the current thread */
    vidt->contexts[0] = NULL;
    for (size_t i = 1; i < 16; i++) {
        vidt->contexts[i] = &cortexm_pip_ctx;
    }
    /*
     * The index 8 is reserved by Pip to save an interrupted context
     * when a partition asks to be CLI. This is not relevant for the
     * root partition.
     */
    vidt->contexts[8] = NULL;
    /*
     * The index 8 is reserved by Pip to save an interrupted context
     * when a partition asks to be STI.
     */
    vidt->contexts[9] = NULL;
}
