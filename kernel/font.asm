bits 64
section .data
global terminus
global ubuntu_mono
align 4
terminus:
	incbin "fonts/Terminus.psf"

ubuntu_mono:
    incbin "fonts/UbuntuMono.psf"
