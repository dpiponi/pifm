I took the code at https://github.com/Emerica/pifm and tried to minimize it and add
documentation so I could understand how it works.

Compile with `gcc -lm -std=c99 -g pifm.c -o pifm`

The original code improved the accuracy of the modulation by using a
pseudo-random number generator to dither the timing of the clock.
I deleted that. It sacrifices some quality but the code is easier to read.


Status
------
Update:
Access to register via bitfields was generating byte loads and stores.
Fixed by using temporary 32-bit intermediate.
Reading assembly language it looks suspicious still but code seems to work.

Update:
Fixed problem mentioned below though I don't understand how.
Maybe someone could look at the code where I set GPFSEL0.

Update:
Looks like it exploits some persistent state left over form the original code so
this code won't work unless you run the original once first.
Working on it...
