@echo off
pushd %~dp0\..
for %%f in (boot.o dogeio.o time.o string.o ports.o vbe.o dogeio_graphics.o font.o file.o idt.o suchboot.o data.o info.o serial.o dogeshell.o dogescript.o kernel.o) do (
    if exist %%f del /q %%f
)
popd
