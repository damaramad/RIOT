/*
 * Copyright (C) 2023 XXX
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     sys_shell_commands
 * @{
 *
 * @file
 * @brief       Shell command to print a snapshot of the current binaries
 *
 * @author      XXX
 *
 * @}
 */

#include "shell.h"

static int
_lsbin(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	/* XXX */

	return 0;
}

SHELL_COMMAND(lsbin, "print a snapshot of the current binaries", _lsbin);
