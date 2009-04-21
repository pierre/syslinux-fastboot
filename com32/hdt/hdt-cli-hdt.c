/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Pierre-Alexandre Meyer - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * -----------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslinux/config.h>

#include "hdt-menu.h"
#include "hdt-cli.h"
#include "hdt-common.h"

/**
 * cli_clear_screen - clear (erase) the entire screen
 **/
static void cli_clear_screen(int argc __unused, char** argv __unused,
			     struct s_hardware *hardware __unused)
{
	clear_screen();
}

/**
 * main_show_modes - show availables modes
 **/
static void main_show_modes(int argc __unused, char** argv __unused,
			    struct s_hardware *hardware __unused)
{
	int i = 0;

	printf("Available modes:\n");
	while (list_modes[i]) {
		more_printf("\t%s\n", list_modes[i]->name);
		i++;
	}
}

/**
 * cli_set_mode - set the mode of the cli, in the cli
 *
 * The mode number must be supplied in argv, position 0.
 **/
static void cli_set_mode(int argc, char **argv,
			 struct s_hardware *hardware)
{
	cli_mode_t new_mode;

	if (argc <= 0) {
		printf("Which mode?\n");
		return;
	}

	/*
	 * Note! argv[0] is a string representing the mode, we need the
	 * equivalent cli_mode_t to pass it to set_mode.
	 */
	new_mode = mode_s_to_mode_t(argv[0]);
	set_mode(new_mode, hardware);
}

/**
 * do_exit - shared helper to exit a mode
 **/
static void do_exit(int argc __unused, char** argv __unused,
		    struct s_hardware *hardware)
{
	int new_mode = HDT_MODE;

	switch (hdt_cli.mode) {
	case HDT_MODE:
		new_mode = EXIT_MODE;
		break;
	default:
		new_mode = HDT_MODE;
		break;
	}

	dprintf("CLI DEBUG: Switching from mode %d to mode %d\n", hdt_cli.mode,
								  new_mode);
	set_mode(new_mode, hardware);
}

/**
 * show_cli_help - shared helper to show available commands
 **/
static void show_cli_help(int argc __unused, char** argv __unused,
			  struct s_hardware *hardware __unused)
{
	int j = 0;
	struct cli_mode_descr *current_mode;
	struct cli_callback_descr* associated_module = NULL;

	find_cli_mode_descr(hdt_cli.mode, &current_mode);

	printf("Available commands are:\n");

	/* List first default modules of the mode */
	if (current_mode->default_modules &&
	    current_mode->default_modules->modules) {
		while (current_mode->default_modules->modules[j].name) {
			more_printf("%s ",
			       current_mode->default_modules->modules[j].name);
			j++;
		}
		more_printf("\n");
	}

	/* List secondly the show modules of the mode */
	if (current_mode->show_modules &&
	    current_mode->show_modules->modules) {
		printf("show commands:\n");
		j = 0;
		while (current_mode->show_modules->modules[j].name) {
			more_printf("\t%s\n",
			       current_mode->show_modules->modules[j].name);
			j++;
		}
	}

	/* List thirdly the set modules of the mode */
	if (current_mode->set_modules &&
	    current_mode->set_modules->modules) {
		printf("set commands:\n");
		j = 0;
		while (current_mode->set_modules->modules[j].name) {
			more_printf("\t%s\n",
			       current_mode->set_modules->modules[j].name);
			j++;
		}
	}

	/* List finally the default modules of the hdt mode */
	if (current_mode->mode != hdt_mode.mode &&
	    hdt_mode.default_modules &&
	    hdt_mode.default_modules->modules) {
		j = 0;
		while (hdt_mode.default_modules->modules[j].name) {
			/*
			 * Any default command that is present in hdt mode but
			 * not in the current mode is available. A default
			 * command can be redefined in the current mode though.
			 * This next call test this use case: if it is
			 * overwritten, do not print it again.
			 */
			find_cli_callback_descr(hdt_mode.default_modules->modules[j].name,
					     current_mode->default_modules,
					     &associated_module);
			if (associated_module == NULL)
				printf("%s ",
				       hdt_mode.default_modules->modules[j].name);
			j++;
		}
		printf("\n");
	}

	main_show_modes(argc, argv, hardware);
}

/**
 * show_cli_help - shared helper to show available commands
 **/
static void goto_menu(int argc __unused, char** argv __unused,
		      struct s_hardware *hardware)
{
	char version_string[256];
	snprintf(version_string, sizeof version_string, "%s %s by %s",
		 PRODUCT_NAME, VERSION, AUTHOR);
	start_menu_mode(hardware, version_string);
	return;
}

/**
 * main_show_summary - give an overview of the system
 **/
void main_show_summary(int argc __unused, char **argv __unused,
		       struct s_hardware *hardware)
{
	detect_pci(hardware);	/* pxe is detected in the pci */
	detect_dmi(hardware);
	cpu_detect(hardware);
	clear_screen();
	main_show_cpu(argc, argv, hardware);
	if (hardware->is_dmi_valid) {
		more_printf("System\n");
		more_printf(" Manufacturer : %s\n",
			    hardware->dmi.system.manufacturer);
		more_printf(" Product Name : %s\n",
			    hardware->dmi.system.product_name);
		more_printf(" Serial       : %s\n",
			    hardware->dmi.system.serial);
		more_printf("Bios\n");
		more_printf(" Version      : %s\n", hardware->dmi.bios.version);
		more_printf(" Release      : %s\n",
			    hardware->dmi.bios.release_date);

		int argc = 2;
		char *argv[2] = { "0", "0" };
		show_dmi_memory_modules(argc, argv, hardware);
	}
	main_show_pci(argc, argv, hardware);

	if (hardware->is_pxe_valid)
		main_show_pxe(argc, argv, hardware);

	main_show_kernel(argc, argv, hardware);
}

void main_show_hdt(int argc __unused, char **argv __unused,
		   struct s_hardware *hardware __unused)
{
	printf("HDT\n");
	printf(" Product     : %s\n", PRODUCT_NAME);
	printf(" Version     : %s\n", VERSION);
	printf(" Author      : %s\n", AUTHOR);
	printf(" Contact     : %s\n", CONTACT);
	char *contributors[NB_CONTRIBUTORS] = CONTRIBUTORS;
	for (int c = 0; c < NB_CONTRIBUTORS; c++) {
		printf(" Contributor : %s\n", contributors[c]);
	}
}

/* Default hdt mode */
struct cli_callback_descr list_hdt_default_modules[] = {
	{
		.name = CLI_CLEAR,
		.exec = cli_clear_screen,
	},
	{
		.name = CLI_EXIT,
		.exec = do_exit,
	},
	{
		.name = CLI_HELP,
		.exec = show_cli_help,
	},
	{
		.name = CLI_MENU,
		.exec = goto_menu,
	},
	{
		.name = NULL,
		.exec = NULL
	},
};

struct cli_callback_descr list_hdt_show_modules[] = {
	{
		.name = CLI_SUMMARY,
		.exec = main_show_summary,
	},
	{
		.name = CLI_PCI,
		.exec = main_show_pci,
	},
	{
		.name = CLI_DMI,
		.exec = main_show_dmi,
	},
	{
		.name = CLI_CPU,
		.exec = main_show_cpu,
	},
	{
		.name = CLI_DISK,
		.exec = main_show_disk,
	},
	{
		.name = CLI_PXE,
		.exec = main_show_pxe,
	},
	{
		.name = CLI_SYSLINUX,
		.exec = main_show_syslinux,
	},
	{
		.name = CLI_KERNEL,
		.exec = main_show_kernel,
	},
	{
		.name = CLI_VESA,
		.exec = main_show_vesa,
	},
	{
		.name = CLI_HDT,
		.exec = main_show_hdt,
	},
	{
		.name = "modes",
		.exec = main_show_modes,
	},
	{
		.name = NULL,
		.exec = NULL,
	},
};

struct cli_callback_descr list_hdt_set_modules[] = {
	{
		.name = CLI_MODE,
		.exec = cli_set_mode,
	},
	{
		.name = NULL,
		.exec = NULL,
	},
};

struct cli_module_descr hdt_default_modules = {
	.modules = list_hdt_default_modules,
};

struct cli_module_descr hdt_show_modules = {
	.modules = list_hdt_show_modules,
	.default_callback = main_show_summary,
};

struct cli_module_descr hdt_set_modules = {
	.modules = list_hdt_set_modules,
};

struct cli_mode_descr hdt_mode = {
	.mode = HDT_MODE,
	.name = CLI_HDT,
	.default_modules = &hdt_default_modules,
	.show_modules = &hdt_show_modules,
	.set_modules = &hdt_set_modules,
};
