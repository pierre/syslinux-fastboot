/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Erwan Velu - All Rights Reserved
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
#include <errno.h>

#include "hdt-cli.h"
#include "hdt-common.h"

static void show_dmi_modules(int argc, char** argv,
                             struct s_hardware *hardware)
{
  char available_dmi_commands[1024];
  memset(available_dmi_commands, 0, sizeof(available_dmi_commands));

  if (hardware->dmi.base_board.filled == true) {
    strncat(available_dmi_commands, CLI_DMI_BASE_BOARD,
      sizeof(CLI_DMI_BASE_BOARD) - 1);
    strncat(available_dmi_commands, " ", 1);
  }
  if (hardware->dmi.battery.filled == true) {
    strncat(available_dmi_commands, CLI_DMI_BATTERY,
      sizeof(CLI_DMI_BATTERY) - 1);
    strncat(available_dmi_commands, " ", 1);
  }
  if (hardware->dmi.bios.filled == true) {
    strncat(available_dmi_commands, CLI_DMI_BIOS,
      sizeof(CLI_DMI_BIOS) - 1);
    strncat(available_dmi_commands, " ", 1);
  }
  if (hardware->dmi.chassis.filled == true) {
    strncat(available_dmi_commands, CLI_DMI_CHASSIS,
      sizeof(CLI_DMI_CHASSIS) - 1);
    strncat(available_dmi_commands, " ", 1);
  }
  for (int i = 0; i < hardware->dmi.memory_count; i++) {
    if (hardware->dmi.memory[i].filled == true) {
      strncat(available_dmi_commands, CLI_DMI_MEMORY,
        sizeof(CLI_DMI_MEMORY) - 1);
      strncat(available_dmi_commands, " bank<bank_number> ",
        19);
      break;
    }
  }
  if (hardware->dmi.processor.filled == true) {
    strncat(available_dmi_commands, CLI_DMI_PROCESSOR,
      sizeof(CLI_DMI_PROCESSOR) - 1);
    strncat(available_dmi_commands, " ", 1);
  }
  if (hardware->dmi.system.filled == true) {
    strncat(available_dmi_commands, CLI_DMI_SYSTEM,
      sizeof(CLI_DMI_SYSTEM) - 1);
    strncat(available_dmi_commands, " ", 1);
  }
  printf("Available DMI modules: %s\n", available_dmi_commands);
}

static void show_dmi_help(int argc, char** argv,
                          struct s_hardware *hardware)
{
  more_printf("Show supports the following commands : \n");
  more_printf(" %s\n", CLI_SHOW_LIST);
  more_printf(" <module_name>\n");
  more_printf(" -\n");
  show_dmi_modules(0, NULL, hardware);
}

static void show_dmi_base_board(int argc, char** argv,
                                struct s_hardware *hardware)
{
  if (hardware->dmi.base_board.filled == false) {
    printf("Base_board module not available\n");
    return;
  }
  clear_screen();
  more_printf("Base board\n");
  more_printf(" Manufacturer : %s\n",
        hardware->dmi.base_board.manufacturer);
  more_printf(" Product Name : %s\n",
        hardware->dmi.base_board.product_name);
  more_printf(" Version      : %s\n", hardware->dmi.base_board.version);
  more_printf(" Serial       : %s\n", hardware->dmi.base_board.serial);
  more_printf(" Asset Tag    : %s\n", hardware->dmi.base_board.asset_tag);
  more_printf(" Location     : %s\n", hardware->dmi.base_board.location);
  more_printf(" Type         : %s\n", hardware->dmi.base_board.type);
  for (int i = 0; i < BASE_BOARD_NB_ELEMENTS; i++) {
    if (((bool *) (&hardware->dmi.base_board.features))[i] == true) {
      more_printf(" %s\n", base_board_features_strings[i]);
    }
  }
}

static void show_dmi_system(int argc, char** argv,
                            struct s_hardware *hardware)
{
  if (hardware->dmi.system.filled == false) {
    printf("System module not available\n");
    return;
  }
  clear_screen();
  more_printf("System\n");
  more_printf(" Manufacturer : %s\n", hardware->dmi.system.manufacturer);
  more_printf(" Product Name : %s\n", hardware->dmi.system.product_name);
  more_printf(" Version      : %s\n", hardware->dmi.system.version);
  more_printf(" Serial       : %s\n", hardware->dmi.system.serial);
  more_printf(" UUID         : %s\n", hardware->dmi.system.uuid);
  more_printf(" Wakeup Type  : %s\n", hardware->dmi.system.wakeup_type);
  more_printf(" SKU Number   : %s\n", hardware->dmi.system.sku_number);
  more_printf(" Family       : %s\n", hardware->dmi.system.family);
}

static void show_dmi_bios(int argc, char** argv,
                          struct s_hardware *hardware)
{
  if (hardware->dmi.bios.filled == false) {
    printf("Bios module not available\n");
    return;
  }
  clear_screen();
  more_printf("BIOS\n");
  more_printf(" Vendor            : %s\n", hardware->dmi.bios.vendor);
  more_printf(" Version           : %s\n", hardware->dmi.bios.version);
  more_printf(" Release           : %s\n",
        hardware->dmi.bios.release_date);
  more_printf(" Bios Revision     : %s\n",
        hardware->dmi.bios.bios_revision);
  more_printf(" Firmware Revision : %s\n",
        hardware->dmi.bios.firmware_revision);
  more_printf(" Address           : 0x%04X0\n",
        hardware->dmi.bios.address);
  more_printf(" Runtime address   : %u %s\n",
        hardware->dmi.bios.runtime_size,
        hardware->dmi.bios.runtime_size_unit);
  more_printf(" Rom size          : %u %s\n", hardware->dmi.bios.rom_size,
        hardware->dmi.bios.rom_size_unit);

  for (int i = 0; i < BIOS_CHAR_NB_ELEMENTS; i++) {
    if (((bool *) (&hardware->dmi.bios.characteristics))[i] == true) {
      more_printf(" %s\n", bios_charac_strings[i]);
    }
  }
  for (int i = 0; i < BIOS_CHAR_X1_NB_ELEMENTS; i++) {
    if (((bool *) (&hardware->dmi.bios.characteristics_x1))[i] ==
        true) {
      more_printf(" %s\n", bios_charac_x1_strings[i]);
    }
  }

  for (int i = 0; i < BIOS_CHAR_X2_NB_ELEMENTS; i++) {
    if (((bool *) (&hardware->dmi.bios.characteristics_x2))[i] ==
        true) {
      more_printf(" %s\n", bios_charac_x2_strings[i]);
    }
  }

}

static void show_dmi_chassis(int argc, char** argv,
                             struct s_hardware *hardware)
{
  if (hardware->dmi.chassis.filled == false) {
    printf("Chassis module not available\n");
    return;
  }
  clear_screen();
  more_printf("Chassis\n");
  more_printf(" Manufacturer       : %s\n",
        hardware->dmi.chassis.manufacturer);
  more_printf(" Type               : %s\n", hardware->dmi.chassis.type);
  more_printf(" Lock               : %s\n", hardware->dmi.chassis.lock);
  more_printf(" Version            : %s\n",
        hardware->dmi.chassis.version);
  more_printf(" Serial             : %s\n", hardware->dmi.chassis.serial);
  more_printf(" Asset Tag          : %s\n",
        hardware->dmi.chassis.asset_tag);
  more_printf(" Boot up state      : %s\n",
        hardware->dmi.chassis.boot_up_state);
  more_printf(" Power supply state : %s\n",
        hardware->dmi.chassis.power_supply_state);
  more_printf(" Thermal state      : %s\n",
        hardware->dmi.chassis.thermal_state);
  more_printf(" Security Status    : %s\n",
        hardware->dmi.chassis.security_status);
  more_printf(" OEM Information    : %s\n",
        hardware->dmi.chassis.oem_information);
  more_printf(" Height             : %u\n", hardware->dmi.chassis.height);
  more_printf(" NB Power Cords     : %u\n",
        hardware->dmi.chassis.nb_power_cords);
}

static void show_dmi_battery(int argc, char** argv,
                             struct s_hardware *hardware)
{
  if (hardware->dmi.battery.filled == false) {
    printf("Battery module not available\n");
    return;
  }
  clear_screen();
  more_printf("Battery \n");
  more_printf(" Vendor             : %s\n",
        hardware->dmi.battery.manufacturer);
  more_printf(" Manufacture Date   : %s\n",
        hardware->dmi.battery.manufacture_date);
  more_printf(" Serial             : %s\n", hardware->dmi.battery.serial);
  more_printf(" Name               : %s\n", hardware->dmi.battery.name);
  more_printf(" Chemistry          : %s\n",
        hardware->dmi.battery.chemistry);
  more_printf(" Design Capacity    : %s\n",
        hardware->dmi.battery.design_capacity);
  more_printf(" Design Voltage     : %s\n",
        hardware->dmi.battery.design_voltage);
  more_printf(" SBDS               : %s\n", hardware->dmi.battery.sbds);
  more_printf(" SBDS Manuf. Date   : %s\n",
        hardware->dmi.battery.sbds_manufacture_date);
  more_printf(" SBDS Chemistry     : %s\n",
        hardware->dmi.battery.sbds_chemistry);
  more_printf(" Maximum Error      : %s\n",
        hardware->dmi.battery.maximum_error);
  more_printf(" OEM Info           : %s\n",
        hardware->dmi.battery.oem_info);
}

static void show_dmi_cpu(int argc, char** argv,
                         struct s_hardware *hardware)
{
  if (hardware->dmi.processor.filled == false) {
    printf("Processor module not available\n");
    return;
  }
  clear_screen();
  more_printf("CPU\n");
  more_printf(" Socket Designation : %s\n",
        hardware->dmi.processor.socket_designation);
  more_printf(" Type               : %s\n", hardware->dmi.processor.type);
  more_printf(" Family             : %s\n",
        hardware->dmi.processor.family);
  more_printf(" Manufacturer       : %s\n",
        hardware->dmi.processor.manufacturer);
  more_printf(" Version            : %s\n",
        hardware->dmi.processor.version);
  more_printf(" External Clock     : %u\n",
        hardware->dmi.processor.external_clock);
  more_printf(" Max Speed          : %u\n",
        hardware->dmi.processor.max_speed);
  more_printf(" Current Speed      : %u\n",
        hardware->dmi.processor.current_speed);
  more_printf(" Cpu Type           : %u\n",
        hardware->dmi.processor.signature.type);
  more_printf(" Cpu Family         : %u\n",
        hardware->dmi.processor.signature.family);
  more_printf(" Cpu Model          : %u\n",
        hardware->dmi.processor.signature.model);
  more_printf(" Cpu Stepping       : %u\n",
        hardware->dmi.processor.signature.stepping);
  more_printf(" Cpu Minor Stepping : %u\n",
        hardware->dmi.processor.signature.minor_stepping);
// more_printf(" Voltage            : %f\n",hardware->dmi.processor.voltage);
  more_printf(" Status             : %s\n",
        hardware->dmi.processor.status);
  more_printf(" Upgrade            : %s\n",
        hardware->dmi.processor.upgrade);
  more_printf(" Cache L1 Handle    : %s\n",
        hardware->dmi.processor.cache1);
  more_printf(" Cache L2 Handle    : %s\n",
        hardware->dmi.processor.cache2);
  more_printf(" Cache L3 Handle    : %s\n",
        hardware->dmi.processor.cache3);
  more_printf(" Serial             : %s\n",
        hardware->dmi.processor.serial);
  more_printf(" Part Number        : %s\n",
        hardware->dmi.processor.part_number);
  more_printf(" ID                 : %s\n", hardware->dmi.processor.id);
  for (int i = 0; i < PROCESSOR_FLAGS_ELEMENTS; i++) {
    if (((bool *) (&hardware->dmi.processor.cpu_flags))[i] == true) {
      more_printf(" %s\n", cpu_flags_strings[i]);
    }
  }
}

static void show_dmi_memory_bank(int argc, char** argv,
                                 struct s_hardware *hardware)
{
  long bank = -1;

  /* Sanitize arguments */
  if (argc > 0)
    bank = strtol(argv[0], (char **)NULL, 10);

  if (errno == ERANGE || bank < 0) {
    printf("This bank number is incorrect\n");
    return;
  }

  if ((bank >= hardware->dmi.memory_count) || (bank < 0)) {
    printf("Bank %d number doesn't exists\n", bank);
    return;
  }
  if (hardware->dmi.memory[bank].filled == false) {
    printf("Bank %d doesn't contain any information\n", bank);
    return;
  }

  printf("Memory Bank %d\n", bank);
  more_printf(" Form Factor  : %s\n",
        hardware->dmi.memory[bank].form_factor);
  more_printf(" Type         : %s\n", hardware->dmi.memory[bank].type);
  more_printf(" Type Detail  : %s\n",
        hardware->dmi.memory[bank].type_detail);
  more_printf(" Speed        : %s\n", hardware->dmi.memory[bank].speed);
  more_printf(" Size         : %s\n", hardware->dmi.memory[bank].size);
  more_printf(" Device Set   : %s\n",
        hardware->dmi.memory[bank].device_set);
  more_printf(" Device Loc.  : %s\n",
        hardware->dmi.memory[bank].device_locator);
  more_printf(" Bank Locator : %s\n",
        hardware->dmi.memory[bank].bank_locator);
  more_printf(" Total Width  : %s\n",
        hardware->dmi.memory[bank].total_width);
  more_printf(" Data Width   : %s\n",
        hardware->dmi.memory[bank].data_width);
  more_printf(" Error        : %s\n", hardware->dmi.memory[bank].error);
  more_printf(" Vendor       : %s\n",
        hardware->dmi.memory[bank].manufacturer);
  more_printf(" Serial       : %s\n", hardware->dmi.memory[bank].serial);
  more_printf(" Asset Tag    : %s\n",
        hardware->dmi.memory[bank].asset_tag);
  more_printf(" Part Number  : %s\n",
        hardware->dmi.memory[bank].part_number);
}

void main_show_dmi(struct s_hardware *hardware)
{

  detect_dmi(hardware);

  if (hardware->is_dmi_valid == false) {
    printf("No valid DMI table found, exiting.\n");
    return;
  }
  printf("DMI Table version %d.%d found\n",
         hardware->dmi.dmitable.major_version,
         hardware->dmi.dmitable.minor_version);

  show_dmi_modules(0, NULL, hardware);
}

void show_dmi_memory_modules(int argc, char** argv,
                             struct s_hardware *hardware)
{
  int clear = 1, show_free_banks = 1;

  /* Sanitize arguments */
  if (argc > 0) {
    clear = strtol(argv[0], NULL, 10);
    if (errno == ERANGE || clear < 0 || clear > 1)
      goto usage;

    if (argc > 1) {
      show_free_banks = strtol(argv[1], NULL, 10);
      if (errno == ERANGE || show_free_banks < 0 || show_free_banks > 1)
        goto usage;
    }
  }

  char bank_number[10];
  char available_dmi_commands[1024];
  memset(available_dmi_commands, 0, sizeof(available_dmi_commands));

  if (hardware->dmi.memory_count <= 0) {
    printf("No memory module found\n");
    return;
  }

  if (clear)
    clear_screen();
  more_printf("Memory Banks\n");
  for (int i = 0; i < hardware->dmi.memory_count; i++) {
    if (hardware->dmi.memory[i].filled == true) {
      /* When discovering the first item, let's clear the screen */
      strncat(available_dmi_commands, CLI_DMI_MEMORY_BANK,
        sizeof(CLI_DMI_MEMORY_BANK) - 1);
      memset(bank_number, 0, sizeof(bank_number));
      snprintf(bank_number, sizeof(bank_number), "%d ", i);
      strncat(available_dmi_commands, bank_number,
        sizeof(bank_number));
      if (show_free_banks == false) {
        if (strncmp
            (hardware->dmi.memory[i].size, "Free", 4))
          printf(" bank %02d      : %s %s@%s\n",
                 i, hardware->dmi.memory[i].size,
                 hardware->dmi.memory[i].type,
                 hardware->dmi.memory[i].speed);
      } else {
        printf(" bank %02d      : %s %s@%s\n", i,
               hardware->dmi.memory[i].size,
               hardware->dmi.memory[i].type,
               hardware->dmi.memory[i].speed);
      }
    }
  }

  return;
  //printf("Type 'show bank<bank_number>' for more details.\n");

usage:
  printf("show memory <clear screen? <show free banks?>>\n");
  return;
}

struct commands_module list_dmi_show_modules[] = {
  {
    .name = CLI_DMI_BASE_BOARD,
    .exec = show_dmi_base_board,
  },
  {
    .name = CLI_DMI_BIOS,
    .exec = show_dmi_bios,
  },
  {
    .name = CLI_DMI_BATTERY,
    .exec = show_dmi_battery,
  },
  {
    .name = CLI_DMI_CHASSIS,
    .exec = show_dmi_chassis,
  },
  {
    .name = CLI_DMI_MEMORY,
    .exec = show_dmi_memory_modules,
  },
  {
    .name = CLI_DMI_MEMORY_BANK,
    .exec = show_dmi_memory_bank,
  },
  {
    .name = CLI_DMI_PROCESSOR,
    .exec = show_dmi_cpu,
  },
  {
    .name = CLI_DMI_SYSTEM,
    .exec = show_dmi_system,
  },
  {
    .name = CLI_DMI_LIST,
    .exec = show_dmi_modules,
  },
};

struct commands_module_descr dmi_show_modules = {
  .modules = list_dmi_show_modules,
  .nb_modules = CLI_DMI_MAX_MODULES,
};

struct commands_mode dmi_mode = {
  .mode = DMI_MODE,
  .show_modules = &dmi_show_modules,
};
