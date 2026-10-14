bits 64
section .data
global font
align 4
font:
	incbin "fonts/font.psf"
font_end:
