bits 64
section .data
global terminus
global ubuntu_mono
global tamzen
align 4
terminus:
	incbin "fonts/Terminus.psf"

ubuntu_mono:
    incbin "fonts/UbuntuMono.psf"

tamzen:
    incbin "fonts/Tamzen8x16.psf"