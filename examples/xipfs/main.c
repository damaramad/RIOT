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
 * @brief       An application demonstrating xipfs
 *
 * @author      Damien Amara <damien.amara@univ-lille.fr>
 *
 * @}
 */

/*
 * RIOT includes
 */
/*
 * XXX: Workaround solution as most MCUs do not appear to define
 * this macro that allows determining if their non-volatile
 * memory is addressable
 */
#define MODULE_PERIPH_FLASHPAGE_IN_ADDRESS_SPACE
#include "periph/flashpage.h"
#include "shell.h"

/*
 * xipfs include
 */
#include "fs/xipfs.h"

/**
 * @def PANIC
 *
 * @brief This macro handles fatal errors
 */
#define PANIC() for (;;);

/*
 * Allocate a new contiguous space for the xipfs_1 file system
 */
XIPFS_NEW_PARTITION(xipfs_1, "/dev/nvme0p0", 10);

/*
 * Allocate a new contiguous space for the xipfs_2 file system
 */
XIPFS_NEW_PARTITION(xipfs_2, "/dev/nvme0p1",  15);

int main(void)
{
    char line_buf[SHELL_DEFAULT_BUFSIZE];

    if (vfs_mount(&xipfs_1.vfs) < 0) {
        printf("vfs_mount: \"%s\": file system has not been "
            "initialized or is corrupted\n", xipfs_1.vfs.mount_point);
        printf("vfs_format: \"%s\": try initializing it\n",
            xipfs_1.vfs.mount_point);
        vfs_format(&xipfs_1.vfs);
        printf("vfs_format: \"%s\": OK\n", xipfs_1.vfs.mount_point);
        if (vfs_mount(&xipfs_1.vfs) < 0) {
            printf("vfs_mount: \"%s\": file system is corrupted!\n",
                xipfs_1.vfs.mount_point);
            PANIC();
        }
    }
    printf("vfs_mount: \"%s\": OK\n", xipfs_1.vfs.mount_point);

    if (vfs_mount(&xipfs_2.vfs) < 0) {
        printf("vfs_mount: \"%s\": file system has not been "
            "initialized or is corrupted\n", xipfs_2.vfs.mount_point);
        printf("vfs_format: \"%s\": try initializing it\n",
            xipfs_2.vfs.mount_point);
        vfs_format(&xipfs_2.vfs);
        printf("vfs_format: \"%s\": OK\n", xipfs_2.vfs.mount_point);
        if (vfs_mount(&xipfs_2.vfs) < 0) {
            printf("vfs_mount: \"%s\": file system is corrupted!\n",
                xipfs_2.vfs.mount_point);
            PANIC();
        }
    }
    printf("vfs_mount: \"%s\": OK\n", xipfs_2.vfs.mount_point);

    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
