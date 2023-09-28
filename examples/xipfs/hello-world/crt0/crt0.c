/*
 * Copyright (C) 2024 Université de Lille
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       CRT0 implementation
 *
 * @author      Damien Amara <damien.amara@univ-lille.fr>
 *
 * @}
 */

#include <stdint.h>
#include <stddef.h>

/*
 * WARNING: No global variable must be declared in this file!
 */

#ifdef __GNUC__

/**
 * @internal
 *
 * @def NORETURN
 *
 * @brief Instructs the compiler that the function will not
 * return
 *
 * @warning The _start function must have this attribute
 */
#define NORETURN __attribute__((noreturn))

/**
 * @internal
 *
 * @def NAKED
 *
 * @brief Instructs the compiler that the function will not have
 * prologue/epilogue sequences
 */
#define NAKED __attribute__((naked))

/**
 * @internal
 *
 * @def UNUSED
 *
 * @brief Instructs the compiler that the function/variable is meant
 * to be possibly unused
 */
#define UNUSED __attribute__((unused))

#else

#error "GCC is required to compile this source file"

#endif /* __GNUC__ */

/**
 * @internal
 *
 * @def LDM_STM_NB_BYTES_COPIED
 *
 * @brief Number of bytes copied by the LDM and STM instructions
 */
#define LDM_STM_NB_BYTES_COPIED (48)

/**
 * @internal
 *
 * @def LDRD_STRD_NB_BYTES_COPIED
 *
 * @brief Number of bytes copied by the LDRD and STRD
 * instructions
 */
#define LDRD_STRD_NB_BYTES_COPIED (8)

/**
 * @internal
 *
 * @def LDR_STR_NB_BYTES_COPIED
 *
 * @brief Number of bytes copied by the LDR and STR instructions
 */
#define LDR_STR_NB_BYTES_COPIED (4)

/**
 * @internal
 *
 * @def LDRB_STRB_NB_BYTES_COPIED
 *
 * @brief Number of bytes copied by the LDRB and STRB
 * instructions
 */
#define LDRB_STRB_NB_BYTES_COPIED (1)

/**
 * @internal
 *
 * @def ROUND
 *
 * @brief Round x to the next power of two y
 *
 * @param x The number to round to the next power of two y
 *
 * @param y The power of two with which to round x
 */
#define ROUND(x, y) (((x) + (y) - 1) & ~((y) - 1))

/**
 * @internal
 *
 * @define THUMB_ADDRESS
 *
 * @brief Calculate the odd address for Thumb mode from x
 *
 * @param x The address to convert to thumb mode
 */
#define THUMB_ADDRESS(x) ((x) | 1)

/**
 * @internal
 *
 * @def ERR_MSG_PREFIX
 *
 * @brief Prefix of error messages
 */
#define ERR_MSG_PREFIX "crt0: "

/**
 * @internal
 *
 * @def ERR_MSG_1
 *
 * @brief Error message number 1
 */
#define ERR_MSG_1 "not enough ram"

/**
 * @internal
 *
 * @def ERR_MSG_2
 *
 * @brief Error message number 2
 */
#define ERR_MSG_2 "out-of-bounds offset"

/**
 * @internal
 *
 * @def ERR_MSG_3
 *
 * @brief Error message number 3
 */
#define ERR_MSG_3 "cannot relocate offsets in .rom"

/**
 * @internal
 *
 * @def ERR_MSG_4
 *
 * @brief Error message number 4
 */
#define ERR_MSG_4 "cannot relocate offsets in .got"

/**
 * @internal
 *
 * @def SYS_WRITE0
 *
 * @brief Semihosting software interrupts (SWIs) to write a
 * null-terminated string to the console.
 */
#define SYS_WRITE0 "4"

/**
 * @internal
 *
 * @def ANGEL_SWI
 *
 * @brief Value indicating to the host that we are requesting a
 * semihosting operation.
 */
#define ANGEL_SWI  "0xab"

/**
 * @def PANIC
 *
 * @brief This macro handles fatal errors
 */
#define PANIC() for (;;);

/**
 * @internal
 *
 * @brief Enumeration of error message identifiers
 */
typedef enum err_msg_id_u {
    /**
     * Identifier of error message 1
     */
    ERR_MSG_ID_1,
    /**
     * Identifier of error message 2
     */
    ERR_MSG_ID_2,
    /**
     * Identifier of error message 3
     */
    ERR_MSG_ID_3,
    /**
     * Identifier of error message 4
     */
    ERR_MSG_ID_4,
} err_msg_id_t;

/**
 * @internal
 *
 * @brief Data structure that describes a symbol table
 */
typedef struct symbolTable_s
{
    /**
     * The entry point address within the partition
     */
    uint32_t entryPoint;
    /**
     * The .rom section size, in bytes, of the partition
     */
    uint32_t romSecSize;
    /**
     * The .rom.ram section size, in bytes, of the partition
     */
    uint32_t romRamSecSize;
    /**
     * The .ram section size, in bytes, of the partition
     */
    uint32_t ramSecSize;
    /**
     * The .got section size, in bytes, of the partition
     */
    uint32_t gotSecSize;
    /**
     * The .romRam section end address of the partition
     */
    uint32_t romRamEnd;
} symbolTable_t;

/**
 * @internal
 *
 * @brief Data structure that describes a patch info entry
 */
typedef struct patchinfoEntry_s
{
    /**
     * The pointer offest to patch
     */
    uint32_t ptrOff;
} patchinfoEntry_t;

/**
 * @internal
 *
 * @brief Data structure that describes a path info table
 */
typedef struct patchinfoTable_s
{
    /**
     * The number of patchinfo entry
     */
    uint32_t entryNumber;
    /**
     * The patchinfo entries
     */
    patchinfoEntry_t entries[];
} patchinfoTable_t;

/**
 * @internal
 *
 * @brief Data structure that describes metadata
 */
typedef struct metadata_s
{
    /**
     * The symbol table
     */
    symbolTable_t symbolTable;
    /**
     * The patchinfo table
     */
    patchinfoTable_t patchinfoTable;
} metadata_t;

/**
 * @internal
 *
 * @brief Data structure that describes the memory layout
 * required by the CRT0 to execute the relocatable binary
 */
typedef struct crt0_ctx_s {
    /**
     * Start address of the binary in the NVM
     */
    void *bin_base;
    /**
     * Start address of the available free RAM
     */
    void *ram_start;
    /**
     * End address of the available free RAM
     */
    void *ram_end;
    /**
     * Start address of the free NVM
     */
    void *nvm_start;
    /**
     * End address of the free NVM
     */
    void *nvm_end;
} crt0_ctx_t;

/**
 * @internal
 *
 * @brief the offset of the metadata structure
 */
extern uint32_t *__metadataOff;

/*
 * Fonction prototypes
 */

static inline void* memcpy(void *dest, const void *src, size_t n);
static NAKED void die(err_msg_id_t id UNUSED);

/**
 * @pre ctx is a pointer to a memory region containg an
 * accessible and valid CRT0 data structure
 *
 * @warning This function must be positioned first in the file
 * to ensure that the first instruction generated for it is
 * located at offset 0 in the output binary file
 *
 * @see -fno-toplevel-reorder GCC option in the Makefile
 *
 * @brief The entry point of CRT0 is responsible for copying
 * data from NVM to RAM, initializing RAM to zero, and applying
 * patch information to enable post-issuance software deployment
 *
 * @param ctx A pointer to a memory region containg a CRT0 data
 * structure
 */
NORETURN void _start(crt0_ctx_t *ctx)
{
    /* get memory layout */
    uint32_t binaryAddr = (uint32_t)ctx->bin_base;
    uint32_t unusedRamAddr = (uint32_t)ctx->ram_start;
    uint32_t ramEndAddr = (uint32_t)ctx->ram_end;

    /* get metadata */
    metadata_t *metadata = (metadata_t *)
        (binaryAddr + (uint32_t) &__metadataOff);
    uint32_t entryPointOffset = metadata->symbolTable.entryPoint;
    uint32_t romSecSize = metadata->symbolTable.romSecSize;
    uint32_t gotSecSize = metadata->symbolTable.gotSecSize;
    uint32_t romRamSecSize = metadata->symbolTable.romRamSecSize;
    uint32_t ramSecSize = metadata->symbolTable.ramSecSize;

    /* calculate section start address in ROM */
    uint32_t romSecAddr =
        (uint32_t) metadata + sizeof(metadata->symbolTable) +
        sizeof(metadata->patchinfoTable.entryNumber) +
        metadata->patchinfoTable.entryNumber *
        sizeof(patchinfoEntry_t);
    uint32_t gotSecAddr = romSecAddr + romSecSize;
    uint32_t romRamSecAddr = gotSecAddr + gotSecSize;
    uint32_t entryPointAddr = THUMB_ADDRESS(romSecAddr + entryPointOffset);

    /* calculate relocated section start address in RAM */
    uint32_t relGotSecAddr = unusedRamAddr;
    uint32_t relRomRamSecAddr = relGotSecAddr + gotSecSize;
    uint32_t relRamSecAddr = relRomRamSecAddr + romRamSecSize;

    /* check if sufficient RAM is available for relocation */
    if (relGotSecAddr + gotSecSize > ramEndAddr ||
        relRomRamSecAddr + romRamSecSize > ramEndAddr ||
        relRamSecAddr + ramSecSize > ramEndAddr) {
        die(ERR_MSG_ID_1);
    }

    /* update unused RAM value */
    ctx->ram_start = (void *)(relRamSecAddr + ramSecSize);
    /* Update unused ROM value */
    ctx->nvm_start = (void *)ROUND(
        (uintptr_t)ctx->nvm_start +
        ((uint32_t)&__metadataOff) +
        sizeof(metadata_t) +
        metadata->symbolTable.romRamEnd, 32);

    /* relocate .rom.ram section */
    (void)memcpy((void *) relRomRamSecAddr,
                 (void *) romRamSecAddr,
                 (size_t) romRamSecSize);

    /* initialize .ram section */
    for (size_t i = 0; (i << 2) < ramSecSize; i++) {
        ((uint32_t *) relRamSecAddr)[i] = 0;
    }

    /* 
     * Relocate the '.got' section from ROM to RAM,
     * dynamically updating each global variable offset
     * - originally relative to the binary file's start - to
     * the new memory addresses where they are now located
     */
    for (size_t i = 0; (i << 2) < gotSecSize; i++) {
        uint32_t off = ((uint32_t *) gotSecAddr)[i];
        uint32_t addr = 0;
        if (off < romSecSize) {
            addr = romSecAddr + off;
            goto validGotEntry;
        }
        off -= romSecSize;
        if (off < gotSecSize) {
            /* offset must always be zero for the
             * _rom_size symbol */
            addr = relGotSecAddr + off;
            goto validGotEntry;
        }
        off -= gotSecSize;
        if (off < romRamSecSize) {
            addr = relRomRamSecAddr + off;
            goto validGotEntry;
        }
        off -= romRamSecSize;
        if (off < ramSecSize) {
            addr = relRamSecAddr + off;
            goto validGotEntry;
        }
        die(ERR_MSG_ID_2);
validGotEntry:
        ((uint32_t *) relGotSecAddr)[i] = addr;
    }

    /*
     * Update each global pointer by assigning the relocated
     * address of the value it points to the corresponding
     * relocated pointer address
     */
    for (size_t i = 0; i < metadata->patchinfoTable.entryNumber; i++) {
        uint32_t ptrOff = metadata->patchinfoTable.entries[i].ptrOff;
        uint32_t off = *((uint32_t *)(romSecAddr + ptrOff));
        uint32_t ptrAddr = 0, addr = 0;
        if (ptrOff < romSecSize) {
            goto ptrOffInRom;
        }
        ptrOff -= romSecSize;
        if (ptrOff < gotSecSize) {
            goto offInGot;
        }
        ptrOff -= gotSecSize;
        if (ptrOff < romRamSecSize) {
            ptrAddr = relRomRamSecAddr + ptrOff;
            goto validPtrAddr;
        }
        ptrOff -= romRamSecSize;
        if (ptrOff < ramSecSize) {
            ptrAddr = relRamSecAddr + ptrOff;
            goto validPtrAddr;
        }
        goto offOutBounds;
validPtrAddr:
        if (off < romSecSize) {
            addr = romSecAddr + off;
            goto validAddr;
        }
        off -= romSecSize;
        if (off < gotSecSize) {
            goto offInGot;
        }
        off -= gotSecSize;
        if (off < romRamSecSize) {
            addr = relRomRamSecAddr + off;
            goto validAddr;
        }
        off -= romRamSecSize;
        if (off < ramSecSize) {
            addr = relRamSecAddr + off;
            goto validAddr;
        }
offOutBounds:
        die(ERR_MSG_ID_2);
ptrOffInRom:
        die(ERR_MSG_ID_3);
offInGot:
        die(ERR_MSG_ID_4);
validAddr:
        *((uint32_t *) ptrAddr) = addr;
    }

    /*
     * Set R0 to the address of the first parameter passed to
     * the start() function, initialize R10 with the address of
     * the relocated GOT, and branch to the address of the
     * start() function
     */
    __asm__ volatile
    (
        "   mov    r0, %0 \n"
        "   mov    sl, %1 \n"
        "   bx     %2     \n"
        :
        : "r" (ctx),
          "r" (relGotSecAddr),
          "r" (entryPointAddr)
        : "r0", "r1", "sl"
    );

    PANIC();
}

/**
 * @brief A version of memcpy optimized for Cortex-M4
 *
 * @param dest Destination memory area
 *
 * @param src Source memory area
 *
 * @param n Number of bytes to copy
 *
 * @return Returns a pointer to dest
 *
 * @see Cortex-M4 Technical Reference Manual 3.3.1 Cortex-M4
 * instructions
 */
static inline void* memcpy(void *dest,
                           const void *src,
                           size_t n)
{
    const char *src0 = src;
    char *dest0 = dest;

    while (n >= LDM_STM_NB_BYTES_COPIED) {
        __asm__ volatile
        (
            "ldmia %0!, {r2-r12,r14}\n"
            "stmia %1!, {r2-r12,r14}\n"
            : "+r" (src0), "+r" (dest0)
            :
            :  "r2",  "r3",  "r4",  "r5",
               "r6",  "r7",  "r8",  "r9",
              "r10", "r11", "r12", "r14",
              "memory"
        );
        n -= LDM_STM_NB_BYTES_COPIED;
    }
    while (n >= LDRD_STRD_NB_BYTES_COPIED) {
        __asm__ volatile
        (
            "ldrd r2, r3, [%0], #8\n"
            "strd r2, r3, [%1], #8\n"
            : "+r" (src0), "+r" (dest0)
            :
            : "r2", "r3", "memory"
        );
        n -= LDRD_STRD_NB_BYTES_COPIED;
    }
    if (n >= LDR_STR_NB_BYTES_COPIED) {
        __asm__ volatile
        (
            "ldr r2, [%0], #4\n"
            "str r2, [%1], #4\n"
            : "+r" (src0), "+r" (dest0)
            :
            : "r2", "memory"
        );
        n -= LDR_STR_NB_BYTES_COPIED;
    }
    while (n >= LDRB_STRB_NB_BYTES_COPIED) {
        __asm__ volatile
        (
            "ldrb r2, [%0], #1\n"
            "strb r2, [%1], #1\n"
            : "+r" (src0), "+r" (dest0)
            :
            : "r2", "memory"
        );
        n -= LDRB_STRB_NB_BYTES_COPIED;
    }

    return dest;
}

/**
 * @brief Print error message and stop execution
 *
 * @param Identifier of the message to print
 */
static NAKED void die(err_msg_id_t id UNUSED)
{
    __asm__ volatile
    (
        "   mov    r2, r0                 \n"
        "   mov    r0, #" SYS_WRITE0 "    \n"
        "   adr.w  r1, 3f                 \n"
        "   bkpt   " ANGEL_SWI "          \n"
        "   mov    r0, #" SYS_WRITE0 "    \n"
        "   adr.w  r3, 1f                 \n"
        "   add.w  r2, r3, r2, lsl #3     \n"
        "   orr.w  r2, #1                 \n"
        "   bx     r2                     \n"
        "1: adr.w  r1, 4f                 \n"
        "   b.w    2f                     \n"
        "   adr.w  r1, 5f                 \n"
        "   b.w    2f                     \n"
        "   adr.w  r1, 6f                 \n"
        "   b.w    2f                     \n"
        "   adr.w  r1, 7f                 \n"
        "2: bkpt   " ANGEL_SWI "          \n"
        "   b      .                      \n"
        "3: .asciz \"" ERR_MSG_PREFIX "\" \n"
        "4: .asciz \"" ERR_MSG_1 "\\n\"   \n"
        "5: .asciz \"" ERR_MSG_2 "\\n\"   \n"
        "6: .asciz \"" ERR_MSG_3 "\\n\"   \n"
        "7: .asciz \"" ERR_MSG_4 "\\n\"   \n"
        "   .align 1                      \n"
    );
}
