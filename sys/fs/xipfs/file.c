/*
 * Copyright (C) 2024 Université de Lille
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     sys_fs_xipfs
 * @{
 *
 * @file
 * @brief       xipfs file implementation
 *
 * @author      Damien Amara <damien.amara@univ-lille.fr>
 *
 * @}
 */

/*
 * libc includes
 */
#include <stddef.h>
#include <string.h>

/*
 * RIOT include
 */
#include "periph/flashpage.h"

/*
 * xipfs includes
 */
#include "fs/xipfs.h"
#include "include/buffer.h"
#include "include/errno.h"
#include "include/file.h"
#include "include/flash.h"

/*
 * Macro definitions
 */

/**
 * @internal
 *
 * @def XIPFS_SYSCALL_TABLE_MAX
 *
 * @brief Maximum size of the syscall table used by the
 * relocatable binary
 */
#define XIPFS_SYSCALL_TABLE_MAX (2)

/**
 * @internal
 *
 * @def XIPFS_FREE_RAM_SIZE
 *
 * @brief Amount of free RAM available for the relocatable
 * binary to use
 */
#define XIPFS_FREE_RAM_SIZE (512)

/**
 * @internal
 *
 * @def EXEC_STACKSIZE_DEFAULT
 *
 * @brief The default execution stack size of the binary
 */
#define EXEC_STACKSIZE_DEFAULT 1024

/**
 * @internal
 *
 * @def NAKED
 *
 * @brief Indicates that the specified function does not need
 * prologue/epilogue sequences generated by the compiler
 */
#ifndef __GNUC__
#error "sys/fs/file: Your compiler does not support GNU extensions"
#else
#define NAKED __attribute__((naked))
#endif /* !__GNUC__ */

/**
 * @internal
 *
 * @def
 *
 * @brief Indicates that the specified variable or function may
 * be intentionally unused
 */
#ifndef __GNUC__
#error "sys/fs/file: Your compiler does not support GNU extensions"
#else
#define UNUSED __attribute__((unused))
#endif /* !__GNUC__ */

/*
 * Internal structure
 */

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
 * @warning If a member of this data structure is added, removed,
 * or moved, the OFFSET variable in the script
 * example/xipfs/hello-world/scripts/gdbinit.py needs to be
 * updated accordingly
 *
 * @brief Data structure that describes the execution context of
 * a relocatable binary
 */
typedef struct exec_ctx_s {
    /**
     * Data structure required by the CRT0 to execute the
     * relocatable binary
     */
    crt0_ctx_t crt0_ctx;
    /**
     * Reserved memory space in RAM for the stack to be used by
     * the relocatable binary
     */
    char stkbot[EXEC_STACKSIZE_DEFAULT-4];
    /**
     * Last word of the stack indicating the top of the stack
     */
    char stktop[4];
    /**
     * Number of arguments passed to the relocatable binary
     */
    int argc;
    /**
     * Arguments passed to the relocatable binary
     */
    char *argv[EXEC_ARGC_MAX];
    /**
     * Table of function pointers for the libc and RIOT
     * functions used by the relocatable binary
     */
    void *syscall_table[XIPFS_SYSCALL_TABLE_MAX];
    /**
     * Reserved memory space in RAM for the free RAM to be used
     * by the relocatable binary
     */
    char ram_start[XIPFS_FREE_RAM_SIZE-1];
    /**
     * Last byte of the free RAM
     */
    char ram_end;
} exec_ctx_t;

/**
 * @internal
 *
 * @brief An enumeration describing the index of the functions
 * of the libc and RIOT in the system call table
 */
enum syscall_index_e {
    /**
     * Index of exit(3)
     */
    SYSCALL_EXIT,
    /**
     * Index of printf(3)
     */
    SYSCALL_PRINTF,
};

/*
 * Global variables
 */

/**
 * @internal
 *
 * @brief The execution context of a relocatable binary
 */
static exec_ctx_t exec_ctx;

/**
 * @internal
 *
 * @brief A pointer to the first instructure to execute in the
 * relocatable binary
 */
static void *_exec_entry_point;

/**
 * @internal
 *
 * @brief A reference to the stack's state prior to invoking
 * execv(2)
 */
static void *_exec_curr_stack __attribute__((used));

/**
 * @brief A pointer to a virtual file name
 */
char *xipfs_infos_file = ".xipfs_infos";

/*
 * Helper functions
 */

/**
 * @internal
 *
 * @brief Local implementation of exit(3) passed to the binary
 * through the syscall table
 *
 * @param status The exit(3) status of the binary is stored in
 * the R0 register
 */
static void NAKED xipfs_exit(int status UNUSED)
{
    __asm__ volatile
    (
        "   ldr   r4, =_exec_curr_stack   \n"
        "   ldr   sp, [r4]                \n"
        "   pop   {r4, pc}                \n"
    );
}

/**
 * @internal
 *
 * @note This function has the same prototype as the xipfs_exit
 * function
 *
 * @brief Starts the execution of the binary in the current RIOT
 * thread
 */
static void NAKED xipfs_start(int status UNUSED)
{
    __asm__ volatile
    (
        "   push   {r4, lr}               \n"
        "   ldr    r4, =_exec_curr_stack  \n"
        "   str    sp, [r4]               \n"
        "   ldr    r0, =exec_ctx          \n"
        "   add    r4, r0, #1040          \n"
        "   mov    sp, r4                 \n"
        "   ldr    r4, =_exec_entry_point \n"
        "   ldr    r4, [r4]               \n"
        "   blx    r4                     \n"
    );
}

/**
 * @internal
 *
 * @brief Converts an address into a thumb address
 *
 * @param addr The address to convert
 *
 * @return The converted address
 */
static inline void *thumb(void *addr)
{
    return (void *)((uintptr_t)addr | 1);
}

/**
 * @internal
 *
 * @brief Fills the CRT0 data structure
 *
 * @param ctx A pointer to a memory region containing an
 * accessible execution context
 *
 * @param filp A pointer to a memory region containing an
 * accessible xipfs file structure
 */
static void exec_crt0_struct_init(exec_ctx_t *ctx, xipfs_file_t *filp)
{
    crt0_ctx_t *crt0_ctx;
    size_t size;
    void *end;

    crt0_ctx = &ctx->crt0_ctx;
    crt0_ctx->bin_base = filp->buf;
    crt0_ctx->ram_start = ctx->ram_start;
    crt0_ctx->ram_end = &ctx->ram_end;
    size = xipfs_file_get_size_(filp);
    crt0_ctx->nvm_start = &filp->buf[size];
    end = (char *)filp + filp->reserved;
    crt0_ctx->nvm_end = end;
}

/**
 * @internal
 *
 * @brief Copies argument pointers to the execution context
 *
 * @param ctx A pointer to a memory region containing an
 * accessible execution context
 *
 * @param argv A pointer to a list of pointers to memory regions
 * containing accessible arguments to pass to the binary
 */
static void exec_arguments_init(exec_ctx_t *ctx, char *const argv[])
{
    while (ctx->argc < EXEC_ARGC_MAX && argv[ctx->argc] != NULL) {
        ctx->argv[ctx->argc] = argv[ctx->argc];
        ctx->argc++;
    }
}

/**
 * @internal
 *
 * @brief Fills the syscall table with libc and RIOT function
 * addresses
 *
 * @param ctx A pointer to a memory region containing an
 * accessible execution context
 */
static void exec_syscall_table_init(exec_ctx_t *ctx)
{
    ctx->syscall_table[SYSCALL_EXIT] = xipfs_exit;
    ctx->syscall_table[SYSCALL_PRINTF] = vprintf;
}

/*
 * Extern functions
 */

/**
 * @internal
 *
 * @brief Checks if the character passed as an argument is in
 * the xipfs charset
 *
 * @param c The character to check
 *
 * @return Returns one if the character passed as an argument is
 * in the xipfs charset or a zero otherwise
 */
static int xipfs_file_path_charset_check(char c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
            c == '/' || c == '.'  ||
            c == '-' || c == '_';
}

/**
 * @pre path must be a pointer that references a path which is
 * accessible, null-terminated, starts with a slash, normalized,
 * and shorter than XIPFS_PATH_MAX
 *
 * @brief Checks if the path passed as an argument is a valid
 * xipfs path
 *
 * @param path The path to check
 *
 * @return Returns zero if the path passed an an argument is a
 * valid xipfs path or a negative value otherwise
 */
int xipfs_file_path_check(const char *path)
{
    size_t i;

    if (path == NULL) {
        xipfs_errno = XIPFS_ENULLP;
        return -1;
    }
    if (path[0] == '\0') {
        xipfs_errno = XIPFS_EEMPTY;
        return -1;
    }
    for (i = 0; i < XIPFS_PATH_MAX && path[i] != '\0'; i++) {
        if (xipfs_file_path_charset_check(path[i]) == 0) {
            xipfs_errno = XIPFS_EINVAL;
            return -1;
        }
    }
    if (path[i] != '\0') {
        xipfs_errno = XIPFS_ENULTER;
        return -1;
    }

    return 0;
}

/**
 * @pre filp must be a pointer to an accessible and valid xipfs
 * file structure
 *
 * @brief Checks if the xipfs file structure passed as an
 * argument is a valid one
 *
 * @param filp The xipfs file structure to check
 *
 * @return Returns zero if the xipfs file structure passed as an
 * argument is a valid one or a negative value otherwise
 */
int xipfs_file_filp_check(xipfs_file_t *filp)
{
    if (filp == NULL) {
        xipfs_errno = XIPFS_ENULLF;
        return -1;
    }
    if (xipfs_flash_page_aligned(filp) < 0) {
        xipfs_errno = XIPFS_EALIGN;
        return -1;
    }
    if (xipfs_flash_in(filp) < 0) {
        xipfs_errno = XIPFS_EOUTNVM;
        return -1;
    }
    if (filp->next == NULL) {
        xipfs_errno = XIPFS_ENULLF;
        return -1;
    }
    if (filp->next != filp) {
        if (xipfs_flash_page_aligned(filp->next) == 0) {
            xipfs_errno = XIPFS_EALIGN;
            return -1;
        }
        if (xipfs_flash_in(filp->next) == 0) {
            xipfs_errno = XIPFS_EOUTNVM;
            return -1;
        }
        if ((uintptr_t)filp >= (uintptr_t)filp->next) {
            xipfs_errno = XIPFS_ELINK;
            return -1;
        }
        if ((uintptr_t)filp + filp->reserved != (uintptr_t)filp->next) {
            xipfs_errno = XIPFS_ELINK;
            return -1;
        }
    }
    if (xipfs_file_path_check(filp->path) < 0) {
        /* xipfs_errno was set */
        return -1;
    }
    if (filp->exec != 0 && filp->exec != 1) {
        xipfs_errno = XIPFS_EPERM;
        return -1;
    }

    return 0;
}

/**
 * @pre vfs_filp must be a pointer to an accessible and valid
 * VFS file structure
 *
 * @brief Retrieves the maximum possible position of a file
 *
 * @param vfs_filp A pointer to a memory region containing a
 * VFS file structure
 *
 * @return Returns the maximum possible position of the file or
 * a negative value otherwise
 */
off_t xipfs_file_get_max_pos(vfs_file_t *vfs_filp)
{
    xipfs_file_t *xipfs_filp;
    off_t max_pos;

    assert(vfs_filp != NULL);

    xipfs_filp = vfs_filp->private_data.ptr;
    if (xipfs_file_filp_check(xipfs_filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }
    max_pos  = (off_t)xipfs_filp->reserved;
    max_pos -= (off_t)sizeof(*xipfs_filp);

    return max_pos;
}

/**
 * @pre vfs_filp must be a pointer to an accessible and valid
 * VFS file structure
 *
 * @brief Retrieves the reserved size of a file
 *
 * @param vfs_filp A pointer to a memory region containing a
 * VFS file structure
 *
 * @return Returns the reserved size of the file or a negative
 * value otherwise
 */
off_t xipfs_file_get_reserved(vfs_file_t *vfs_filp)
{
    xipfs_file_t *xipfs_filp;

    assert(vfs_filp != NULL);

    xipfs_filp = vfs_filp->private_data.ptr;
    if (xipfs_file_filp_check(xipfs_filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    return (off_t)xipfs_filp->reserved;
}

/**
 * @pre filp must be a pointer to an accessible and valid xipfs
 * file structure
 *
 * @brief Removes a file from the file system
 *
 * @param filp A pointer to a memory region containing an
 * accessible xipfs file structure
 *
 * @return Returns zero if the function succeeds or a negative
 * value otherwise
 */
int xipfs_file_erase(xipfs_file_t *filp)
{
    unsigned start, number, i;

    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    start = flashpage_page(filp);
    number = filp->reserved / FLASHPAGE_SIZE;

    for (i = 0; i < number; i++) {
        if (xipfs_flash_erase_page(start + i) < 0) {
            /* xipfs_errno was set */
            return -1;
        }
    }

    return 0;
}

/**
 * @pre filp must be a pointer to an accessible and valid xipfs
 * file structure
 *
 * @brief Retrieves the current file size from the list of
 * previous sizes
 *
 * @param filp A pointer to a memory region containing an
 * accessible xipfs file structure
 *
 * @return Returns the current file size or a negative value
 * otherwise
 */
off_t xipfs_file_get_size_(xipfs_file_t *filp)
{
    size_t i = 1;
    off_t size;

    if (filp->size[0] == XIPFS_FLASH_ERASE_STATE) {
        /* file size not in flash yet */
        return 0;
    }

    while (i < XIPFS_FILESIZE_SLOT_MAX) {
        if (filp->size[i] == XIPFS_FLASH_ERASE_STATE) {
            return (off_t)filp->size[i-1];
        }
        i++;
    }

    if (xipfs_buffer_read_32((unsigned *)&size, &filp->size[i-1]) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    return size;
}

/**
 * @pre vfs_filp must be a pointer to an accessible and valid
 * xipfs file structure
 *
 * @brief Wrapper to the xipfs_file_get_size_ function that
 * checks the validity of the xipfs file strucutre
 *
 * @param vfs_filp A pointer to a memory region containing an
 * accessible xipfs file structure
 *
 * @return Returns the current file size or a negative value
 * otherwise
 */
off_t xipfs_file_get_size(vfs_file_t *vfs_filp)
{
    xipfs_file_t *xipfs_filp;

    xipfs_filp = vfs_filp->private_data.ptr;

    if (xipfs_file_filp_check(xipfs_filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    return xipfs_file_get_size_(xipfs_filp);
}

/**
 * @pre filp must be a pointer to an accessible and valid xipfs
 * file structure
 *
 * @brief Sets the new file size to the list of previous sizes
 *
 * @param vfs_fp A pointer to a memory region containing an
 * accessible xipfs file structure
 *
 * @param size The size to set to the file
 *
 * @return Returns zero if the function succeed or a negative
 * value otherwise
 */
int xipfs_file_set_size(vfs_file_t *vfs_fp, off_t size)
{
    xipfs_file_t *filp;
    size_t i = 1;

    filp = vfs_fp->private_data.ptr;

    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    while (i < XIPFS_FILESIZE_SLOT_MAX) {
        if (filp->size[i] != XIPFS_FLASH_ERASE_STATE) {
            break;
        }
        i++;
    }
    i %= XIPFS_FILESIZE_SLOT_MAX;

    if (xipfs_buffer_write_32(&filp->size[i], size) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    if (xipfs_buffer_flush() < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    return 0;
}

/**
 * @pre filp must be a pointer to an accessible and valid xipfs
 * file structure
 *
 * @pre path must be a pointer that references a path which is
 * accessible, null-terminated, starts with a slash, normalized,
 * and shorter than XIPFS_PATH_MAX
 *
 * @brief Changes the path of an xipfs file
 *
 * @param filp A pointer to a memory region containing an
 * accessible xipfs file structure
 *
 * @param to_path The new path of the file
 *
 * @return Returns zero if the function succeed or a negative
 * value otherwise
 */
int xipfs_file_rename(xipfs_file_t *filp, const char *to_path)
{
    size_t len;

    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    if (xipfs_file_path_check(to_path) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    len = strlen(to_path) + 1;

    if (xipfs_buffer_write(filp->path, to_path, len) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    if (xipfs_buffer_flush() < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    return 0;
}

/**
 * @pre vfs_filp must be a pointer to an accessible and valid VFS
 * file structure
 *
 * @brief Reads a byte from the current position of the open VFS
 * file
 *
 * @param vfs_filp A pointer to a memory region containing an
 * accessible and open VFS file structure
 *
 * @param byte A pointer to a memory region where to store the
 * read byte
 *
 * @return Returns zero if the function succeed or a negative
 * value otherwise
 */
int xipfs_file_read_8(vfs_file_t *vfs_filp, char *byte)
{
    xipfs_file_t *filp;
    off_t pos, pos_max;

    filp = vfs_filp->private_data.ptr;

    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    if ((pos_max = xipfs_file_get_max_pos(vfs_filp)) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    pos = vfs_filp->pos;

    /* since off_t is defined as a signed integer type, we must
     * verify that the value is non-negative */
    if (pos < 0 || pos > pos_max) {
        xipfs_errno = XIPFS_EMAXOFF;
        return -1;
    }

    if (xipfs_buffer_read_8(byte, &filp->buf[pos]) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    return 0;
}

/**
 * @pre vfs_filp must be a pointer to an accessible and valid VFS
 * file structure
 *
 * @brief Writes a byte from to the current position of the open
 * VFS file
 *
 * @param vfs_filp A pointer to a memory region containing an
 * accessible and open VFS file structure
 *
 * @param byte The byte to write to the current position of the
 * open file
 *
 * @return Returns zero if the function succeed or a negative
 * value otherwise
 */
int xipfs_file_write_8(vfs_file_t *vfs_filp, char byte)
{
    xipfs_file_t *filp;
    off_t pos, pos_max;

    filp = vfs_filp->private_data.ptr;

    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    if ((pos_max = xipfs_file_get_max_pos(vfs_filp)) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    pos = vfs_filp->pos;

    /* since off_t is defined as a signed integer type, we must
     * verify that the value is non-negative */
    if (pos < 0 || pos > pos_max) {
        xipfs_errno = XIPFS_EMAXOFF;
        return -1;
    }

    if (xipfs_buffer_write_8(&filp->buf[pos], byte) < 0) {
        /* xipfs_errno was set */
        return -1;
    }

    return 0;
}

/**
 * @pre filp must be a pointer to an accessible and valid xipfs
 * file structure
 *
 * @brief Executes a binary in the current RIOT thread
 *
 * @param filp A pointer to a memory region containing an
 * accessible xipfs file structure
 *
 * @param argv A pointer to a list of pointers to memory regions
 * containing accessible arguments to pass to the binary
 *
 * @return Returns zero if the function succeed or a negative
 * value otherwise
 */
int xipfs_file_exec(xipfs_file_t *filp, char *const argv[])
{
    if (xipfs_file_filp_check(filp) < 0) {
        /* xipfs_errno was set */
        return -1;
    }
    (void)memset(&exec_ctx, 0, sizeof(exec_ctx));
    exec_crt0_struct_init(&exec_ctx, filp);
    exec_arguments_init(&exec_ctx, argv);
    exec_syscall_table_init(&exec_ctx);
    _exec_entry_point = thumb(&filp->buf[0]);
    xipfs_start(0);

    return 0;
}
