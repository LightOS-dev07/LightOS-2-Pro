ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot_header : { KEEP(*(.multiboot)) }
    .text    : { *(.text*)   }
    .rodata  : { *(.rodata*) }
    .data    : { *(.data*)   }

    .bss : {
        __bss_start = .;
        *(.bss*) *(COMMON)
        __bss_end = .;
    }
}
