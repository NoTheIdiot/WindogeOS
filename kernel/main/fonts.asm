bits 64
section .data
global terminus
global ubuntu_mono
global vga_text
global spleen
global tamzen
align 4
terminus:
	incbin "fonts/Terminus.psf"

ubuntu_mono:
    incbin "fonts/UbuntuMono.psf"

tamzen:
    incbin "fonts/Tamzen8x16.psf"

vga_text:
    incbin "fonts/vga_text.psf"

spleen:
    incbin "fonts/spleen.psf"