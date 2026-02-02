
logos_ROM_START as a linker symbol: the address of the symbol = 0x0025F0E0
logos_ROM_START as a u32 variable: the address of the variable = 0x08EF5028, the value inside = 0x0025F0E0

Original decomp code does (u32)logos_ROM_START expecting the address to be the value. Your u32 stubs give it the variable's address instead.
Sprites work because they're loaded via numeric addresses in data tables → hits vrom_find correctly. Logos fail because they're loaded via _ROM_START linker symbols → passes the variable address instead of the ROM address.
Fix: move _ROM_START/_ROM_END definitions into a linker script as absolute symbols, remove the u32 stubs from reimpl.c.
