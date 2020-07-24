# Linux for the Playstation 1

Ongoing project to bring Linux back to the sony Playstation 1, One.
Based on the original work by the wonderfull rusian Runix team.

## Plans

Reverseengineer or atleast figure out how they intended to store a filesystem on the memoryunits.
change the videodriver a bit.
figure out how to connect their usb host ic to the PIO.
bring a modern mipsel crosscompiler to the game

## How to

Tobedone, its not realy that hard when you got the right tool to convert the elf kernel into PS-EXE.
Can be compiled on a x86_64 host using crosscompilers.
Ive included the crosscompiler from the original runix team in this branch/latest commit. 
I might be removing it for a modern solution later.

The main thread where talks be made was here: <http://www.psxdev.net/forum/viewtopic.php?f=55&t=152>
I suggest we create a new thread if any progress will be made or questions from new people tho.

# Copyright and licenses
Linux kernel Linus Torvalds, GNU General Public License version 2.x, retrieved from the Runix site sofar,
might change in the future.

elf2psx: Converts ELFs to PlayStation executables
Copyright (C) GreaseMonkey, 2017, licensed under Creative Commons Zero:
<https://creativecommons.org/publicdomain/zero/1.0/>
(a newer version? might be: <https://github.com/ChenThread/fromage>, license changed tho. still an awesome game for the PS1)

NO$PSX Version 2.0, Copyright 2008-2017 Martin Korth  <https://problemkaputt.de/psx.htm>

build.sh and generated files that are included
29-12-2019 2020 CodeAsm <https://www.codeasm.com>

