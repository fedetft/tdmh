# Debugging

## Connection
 - remove the ST-LINK jumpers
 - connect GND, SWDIO, SWCLK between the STM32 SWD connector (http://www.openstm32.org/dl302?display) and the wandstem CN3 (debug connector, respectively pin 7, 4, 2)
```
         *   * GND
         *   *
  SWDIO  *   *
  SWDCLK *   *                         WandStem revx.xx
```
 - connect to the stm32 using a micro USB cable
 - power up the WandStem

## Setup image
 - decomment `#define JTAG_DISABLE_SLEEP` in miosix/config/miosix_settings.h:152
 - set compilation optimization to -O0 in miosix/config/Makefile.inc:45
 - compile and flash the image on the board

## GDB
 - sudo openocd -f miosix/arch/cortexM3_efm32gg/efm32gg332f1024_wandstem/wandstem-stlink.cfg
 - arm-miosix-eabi-gdb main.elf
 - target remote :3333
 - monitor reset halt
 - break main
 - c
