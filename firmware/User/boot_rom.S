    .section .rodata
    .align  2
    .global boot_rom
boot_rom:
    .incbin "../driver/tfpcmcia.rom"
