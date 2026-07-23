#include <dogeio.h>
#include <boot/kernel.h>

void menubar_draw() {
	dogeio_text_color_change(0x000000);
	dogeio_text_background_change(0xffffff);
	cursor_y = 0;
	dogeio_text_print(" WindogeOS v0.0.3");
	for (int i = 0; i < 143; i++) {
		dogeio_text_print(" ");
	}
	dogeio_text_color_change(0xffffff);
	dogeio_text_background_change(0x000000);
}