# Linux for the Playstation 1

Ongoing project to bring Linux back to the original Sony PlayStation, also called the PS1 or PSX, based on the original work by the wonderful Russian Runix team.

## Plans

Reverse engineer or atleast figure out how they intended to store a filesystem on the memory units.
change the video driver a bit.
figure out how to connect their usb host ic to the PIO.
bring a modern mipsel crosscompiler to the game

## How to build

Tobedone, it's not realy that hard when you got the right tool to convert the elf kernel into PS-EXE.
Can be compiled on a x86_64 host using crosscompilers.
Ive included the crosscompiler from the original runix team in this branch/latest commit. 
I might be removing it for a modern solution later.

The main thread where talks be made was here: <http://www.psxdev.net/forum/viewtopic.php?f=55&t=152>
I suggest we create a new thread if any progress will be made or questions from new people tho.

So you really wanna try this build?:

- ``git clone https://github.com/CodeAsm/PS1Linux.git``
- The worst command ever: ``$ sudo tar -xvf tools/mipsel-linux-cross-i586.tar.gz -C /`` cause this goes in YOUR root. no support given
-  ``./build``

The build command supports also the ``-r`` switch to just run the emulator again with a precompiled kernel.

### Delete previous compilation attempt

To clean up the linux kernel sources, one could run ``make mrproper`` inside the kernel source tree or my ``build -d`` command

# Intresting facts or finds

Inside linux/drivers/block/ we find bu.c (and bu.h) which contains the memorycard driver code. based on this I got some parts of the memorycard working (and detected). but now the actual filesystem...

Probably a better method or intended way, but if you define DEBUG inside the bu.c file ``#define DEBUG`` for example on line 69. Youll get more debug output from this driver. its not much but something. printk in general can be handy

Inside linux/arch/mipsnommu/ps we find specific Playstation driver things, like a RTC? SIO for serial port and if you check the boot folder here, youll find some kernel loader code thats based on a DEC station?
Definitly try look at the prom folder here, cmdline.c contains the kernel boot argumment, handy to edit, recompile and see a different init loaded, delay kernel boot messages and specify the boot device if we ever get some working. 
I think i got ``boot_delay=10000`` working, maybe not. 
Also here seems to be some hardcoded memory size code? 2Mb, maybe this is also where to edit when you mod your playstation for more ram

Youll find actual drivers for the video inside linux/drivers/video/psxcon.c

Inside Config_Addon, ``CONFIG_USB_SL811H=y`` is defined. the driver is here: linux/drivers/usb/sl811h.c (and .h). Might be cool to get this or some similair usb host ic working on the Playstation. not yet sure how this is connected to the PIO (they also have PIO hardware drivers)

# Copyright and licenses
Linux kernel Linus Torvalds, GNU General Public License version 2.x, retrieved from the Runix site sofar,
might change in the future.

elf2psx: Converts ELFs to PlayStation executables
Copyright (C) GreaseMonkey, 2017, licensed under Creative Commons Zero:
<https://creativecommons.org/publicdomain/zero/1.0/>
(a newer version? might be: <https://github.com/ChenThread/fromage>, license changed tho. still an awesome game for the PS1)

NO$PSX Version 2.0, Copyright 2008-2017 Martin Korth  <https://problemkaputt.de/psx.htm>
Ive tried 2.2, but somehow it likes to crash when I specify the kernel to boot. It works when loaded manually. we are using Wine

build.sh and generated files that are included
29-12-2019 2020 2023 CodeAsm <https://www.codeasm.com>

