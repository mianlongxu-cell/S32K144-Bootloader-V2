    .section .rodata.flash_driver_blob, "a", %progbits
    .balign 4
    .global __flash_driver_blob_start
    .global __flash_driver_blob_end
__flash_driver_blob_start:
    .incbin "../FlashDriver/build/flash_driver.bin"
__flash_driver_blob_end:

