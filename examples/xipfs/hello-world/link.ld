/*
 * Copyright (C) 2024 Université de Lille
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

OUTPUT_FORMAT(
    "elf32-littlearm",
    "elf32-littlearm",
    "elf32-littlearm"
)
OUTPUT_ARCH(arm)
ENTRY(start)

SECTIONS
{
    /*
     * The .rom output section gathers all input sections that
     * must be retained in ROM
     */
    .rom :
    {
        . = ALIGN( 4 ) ;
        __romStart = . ;

        *(.text*)
        . = ALIGN( 4 ) ;
        *(.rodata*)

        . = ALIGN( 4 ) ;
        __romEnd = . ;
    }
    __romSize = __romEnd - __romStart;

    /*
     * The .got output section gathers all .got input sections
     * that need to be relocated to RAM and patched by the CRT0.
     * To optimize ROM space, this section should be placed
     * immediately after the .rom section
     */
    .got :
    {
        . = ALIGN( 4 ) ;
        __gotStart = . ;

        *(.got*)

        . = ALIGN( 4 ) ;
        __gotEnd = . ;
    }
    __gotSize = __gotEnd - __gotStart;

    /*
     * The .rom.ram output section gathers all input sections
     * with initialized global variables that the CRT0 must copy
     * from ROM to RAM
     */
    .rom.ram :
    {
        . = ALIGN( 4 ) ;
        __romRamStart = . ;

        *(.data*)

        . = ALIGN( 4 ) ;
        __romRamEnd = . ;
    }
    __romRamSize = __romRamEnd - __romRamStart;

    /*
     * The .ram output section gathers all input sections with
     * uninitialized global variables that must be initialized
     * to zero in RAM
     */
    .ram (NOLOAD) :
    {
        . = ALIGN( 4 ) ;
        __ramStart = . ;

        *(.bss* COMMON)

        . = ALIGN( 4 ) ;
        __ramEnd = . ;
    }
    __ramSize = __ramEnd - __ramStart;
}
