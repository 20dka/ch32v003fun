all : flash

TARGET:=v003_flash_over_i2c
TARGET_MCU?=CH32V003


include ../../ch32fun/ch32fun.mk

build_alt : EXTRA_CFLAGS+=-DRUNNING_FROM_FLASH=0

build_alt : $(TARGET).bin

flash_over_i2c : build_alt
	minichlink -w $(TARGET).bin $(WRITE_SECTION) -b


flash : cv_flash
clean : cv_clean
