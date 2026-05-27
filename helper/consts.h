#ifndef CONSTS_H
#define CONSTS_H

#define MAX_COLS 80
#define MAX_ROWS 25
#define VRAM_ADDRESS 0xB8000

#define DOGE_COLOR 0x0E
#define HUMAN_COLOR 0x0F

enum such_terminal_color {
	BLACK = 0x00,
    BLUE = 0x01,
    GREEN = 0x02,
    CYAN = 0x03,
    RED = 0x04,
    MAGENTA = 0x05,
    BROWN = 0x06,
    LIGHT_GREY = 0x07,
    DARK_GREY = 0x08,
    LIGHT_BLUE = 0x09,
    LIGHT_GREEN = 0x0A,
    LIGHT_CYAN = 0x0B,
    LIGHT_RED = 0x0C,
    LIGHT_MAGENTA = 0x0D,
    LIGHT_BROWN = 0x0E,
    WHITE = 0x0F
};

#endif
