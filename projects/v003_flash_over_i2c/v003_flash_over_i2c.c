#include "ch32fun.h"
#include <stdio.h>
#include "i2c_slave.h"

#define PIN_BOB PD4
#define PIN_KEVIN PA1

#ifndef RUNNING_FROM_FLASH
#define RUNNING_FROM_FLASH 1
#endif

volatile uint8_t i2c_registers[16] = {0x00};


void onupdate(void)
{
    printf("update time!!\n");
}


void onread(uint8_t reg)
{
    (void)reg;
    printf("register #%d was read from the i2c buffer\n", reg);
}


void onwrite(uint8_t reg, uint8_t len)
{
    (void)reg;
    (void)len;
    printf("register #%d was written to (len: %d)\n", reg, len);
}

int main()
{
	SystemInit();

	funGpioInitAll();
	funPinMode( PIN_BOB,   GPIO_Speed_10MHz | GPIO_CNF_OUT_PP );
	funPinMode( PIN_KEVIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP );

	funDigitalWrite( PIN_BOB,   FUN_LOW );
	funDigitalWrite( PIN_KEVIN, FUN_LOW );



    funPinMode(PC1, GPIO_CFGLR_OUT_10Mhz_AF_OD); // SDA
    funPinMode(PC2, GPIO_CFGLR_OUT_10Mhz_AF_OD); // SCL
	SetupI2CSlave(0x69, i2c_registers, sizeof(i2c_registers), 0xDEADBEEF, &onwrite, &onread, &onupdate);

	while(1)
	{
		#if RUNNING_FROM_FLASH
		funDigitalWrite( PIN_BOB,   FUN_HIGH );
		#else
		funDigitalWrite( PIN_KEVIN, FUN_LOW  );
		#endif

		Delay_Ms( 500 );

		#if RUNNING_FROM_FLASH
		funDigitalWrite( PIN_BOB,   FUN_LOW  );
		#else
		funDigitalWrite( PIN_KEVIN, FUN_HIGH );
		#endif
		Delay_Ms( 500 );
		printf("beep\n");
	}
}
