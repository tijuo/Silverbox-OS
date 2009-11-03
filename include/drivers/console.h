#ifndef CONSOLE
#define CONSOLE

#include <kernel/video.h>
#include <drivers/keyboard.h>
#include <os/file.h>

#define CONSOLES_MAX		10
#define VCONSOLE_SIZE           (LINES * COLUMNS * SCREENS * 2)

console_t       *console_devs[CONSOLES_MAX];

#endif
