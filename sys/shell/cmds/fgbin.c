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
 * @brief       Shell command to run a binary in the foreground
 *
 * @author      XXX
 *
 * @}
 */

#include "shell.h"

static int
_fgbin(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	/* XXX */

	return 0;
}

SHELL_COMMAND(fgbin, "run a binary in the foreground", _fgbin);
