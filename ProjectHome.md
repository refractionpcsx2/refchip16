# RefChip16 #

RefChip16 is a Chip16 Virtual Machine Emulator, a system designed by the folks on NGEmu, which can be read up on in the following 2 threads:

http://forums.ngemu.com/showthread.php?t=138170

http://forums.ngemu.com/showthread.php?t=145620

It currently has a simple SDL (Simple DirectMedia Layer) implementation for graphics and uses XAudio2 for sound with an ADSR filter.

Other features include a simple Dynamic Recompiler with a very basic implementation of Constant Propegation and Register Caching as examples of recompiler optimizations.

Currently this emulator supports Spec 1.1 and will hopefully continue to support future revisions.