#!/bin/sh
#rm -rf PSXLinux
#tar -xvf PSXLinux-kernel-2.4.x-beta1.tar.gz

function build {
cd linux
make mrproper
cp Config .config
make dep
make menuconfig
make 2>&1 | tee ../build.log
cd ..
tools/elf2psx/elf2psx -p linux/linux bin/kernel.exe
}

function delete {
rm bin/kernel.exe
cd linux
make mrproper
rm 
exit
}

function run {
exec wine ./tools/no\$psx/NO\$PSX.EXE bin/kernel.exe
}

#----------------------------------------------------------------------
#process commandline arguments
while [[ $# -gt 0 ]]
do
key="$1"
case $key in
    -d|-delete|-deleteall)
    delete
    shift; # past argument and value
    ;;-r|-run)
    run
    exit
    shift; # past argument and value
    ;;
esac
done

build
run
