section .rodata
global font_bin_start
global font_bin_end

font_bin_start:
	incbin "fonts/font.psf"
font_bin_end:
