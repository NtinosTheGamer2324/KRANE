@echo off
REM ==============================================================
REM KRANE - run.bat
REM ==============================================================
REM KRANE doesn't stand for anything. It's just KRANE.
REM
REM Build (make) happens inside the modu-os Docker container.
REM Boot (qemu) happens on the host, against the image the
REM container just produced at build\krane.img.
REM ==============================================================

echo Made in the Hellenic Republic

docker run --rm -it --privileged -v /dev:/dev -v "%cd%":/root/env buildenv-krane /bin/bash -c "cd /root/env && make clean && make all"

start "cmdQEMU" qemu-system-x86_64 ^
    -machine pc ^
    -m 512M ^
    -drive file=build\krane.img,format=raw,index=0,media=disk ^
    -no-reboot ^
    -no-shutdown ^
    -d int,cpu_reset ^
    -D qemu.log ^
    -serial file:com1.log

"./vendor/ntsoftware/logviewer/Log Viewer.exe" ./com1.log