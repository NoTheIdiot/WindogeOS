#include "../library/dogeio.h"
#include "../library/dogelib.h"
#include "../library/stdint.h"
#include "../library/stddef.h"
#include "../library/bool.h"

void _start(void) {
    println("WindogeOS GUI v0.0.1-Beta1");
    println("Press any key to start, Ctrl+O to exit, Middle click to reset cursor.");
    get_key();

    set_video_mode();

    uint32_t screen_width = 1280; 
    uint32_t screen_height = 800;
    uint32_t bg_color = COLOR_BRIGHT_BLUE;
    uint32_t cursor_color = COLOR_CURSOR;

    int32_t accel_option = 1;

    draw_rec(0, 0, screen_height, screen_width, bg_color);

    int32_t mouse_x = (int32_t)(screen_width / 2);
    int32_t mouse_y = (int32_t)(screen_height / 2);

    for (int32_t r = 0; r < 9; r++) {
        for (int32_t c = 0; c < 9; c++) {
            draw_pixel((uint32_t)(mouse_x + c), (uint32_t)(mouse_y + r), cursor_color);
        }
    }

    while (1)
    {
        uint64_t key = get_key_delay(1);
        if (key == KEY_CTRL_O) 
        {
            break;
        }

        int32_t dx = 0, dy = 0;
        uint8_t buttons = 0;
        
        read_mouse(&dx, &dy, &buttons);

        if (buttons & 4)
        {
            for (int32_t r = 0; r < 9; r++) {
                for (int32_t c = 0; c < 9; c++) {
                    draw_pixel((uint32_t)(mouse_x + c), (uint32_t)(mouse_y + r), bg_color);
                }
            }
            
            mouse_x = (int32_t)(screen_width / 2);
            mouse_y = (int32_t)(screen_height / 2);

            for (int32_t r = 0; r < 9; r++) {
                for (int32_t c = 0; c < 9; c++) {
                    draw_pixel((uint32_t)(mouse_x + c), (uint32_t)(mouse_y + r), cursor_color);
                }
            }
            
            dx = 0;
            dy = 0;
        }

        if (dx || dy)
        {
            int32_t multiplier_fp = 256; 
            int32_t abs_dx = dx < 0 ? -dx : dx;
            int32_t abs_dy = dy < 0 ? -dy : dy;
            int32_t speed = abs_dx > abs_dy ? abs_dx : abs_dy;

            if (accel_option == 1)
            {
                if (speed > 4) {
                    multiplier_fp = 281; 
                }
            }
            else if (accel_option == 2)
            {
                if (speed > 3 && speed <= 8) {
                    multiplier_fp = 384; 
                } else if (speed > 8) {
                    multiplier_fp = 512; 
                }
            }
            else if (accel_option == 3)
            {
                if (speed > 2 && speed <= 6) {
                    multiplier_fp = 512; 
                } else if (speed > 6) {
                    multiplier_fp = 768; 
                }
            }

            int32_t accel_dx = (dx * multiplier_fp) / 256;
            int32_t accel_dy = (dy * multiplier_fp) / 256;

            int32_t next_x = mouse_x + accel_dx;
            int32_t next_y = mouse_y + accel_dy;

            if (next_x < 0) next_x = 0;
            if (next_y < 0) next_y = 0;
            if (next_x > (int32_t)(screen_width - 9))  next_x = (int32_t)(screen_width - 9);
            if (next_y > (int32_t)(screen_height - 9)) next_y = (int32_t)(screen_height - 9);

            if (next_x != mouse_x || next_y != mouse_y) 
            {
                for (int32_t r = 0; r < 9; r++) {
                    for (int32_t c = 0; c < 9; c++) {
                        draw_pixel((uint32_t)(mouse_x + c), (uint32_t)(mouse_y + r), bg_color);
                    }
                }

                for (int32_t r = 0; r < 9; r++) {
                    for (int32_t c = 0; c < 9; c++) {
                        draw_pixel((uint32_t)(next_x + c), (uint32_t)(next_y + r), cursor_color);
                    }
                }
                
                mouse_x = next_x;
                mouse_y = next_y;
            }
        }
    }

    set_text_mode();
    clear();
}
