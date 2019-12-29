#!/bin/sh
#rm -rf PSXLinux
#tar -xvf PSXLinux-kernel-2.4.x-beta1.tar.gz
cd linux
make mrproper
cp Config .config
make dep
make menuconfig
make
cd ..
./tools/elf2psx/elf2psx -p PSXLinux/linux bin/kernel.exe
exec wine ./tools/no\$psx/NO\$PSX.EXE bin/kernel.exe

