/*
 * Single-File-Header for using the I2C peripheral in slave mode
 *
 * MIT License
 *
 * Copyright (c) 2024 Renze Nicolai
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#define __HIGH_CODE __attribute__((section(".highcode"), used))
#define __INTERRUPT __attribute__((interrupt))

#include "ch32fun.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

typedef void (*i2c_write_callback_t)(uint8_t reg, uint8_t length);
typedef void (*i2c_read_callback_t)(uint8_t reg);
typedef void (*i2c_update_callback_t)(void);

struct _i2c_slave_state {
    uint8_t first_write;
    uint8_t offset;
    uint8_t position;
    volatile uint8_t* volatile registers;
    volatile uint8_t size;
    volatile uint8_t magic_reg[4];
    volatile uint32_t magic;
    i2c_write_callback_t write_callback;
    i2c_read_callback_t read_callback;
    i2c_update_callback_t update_callback;
    bool writing;
} i2c_slave_state;

void SetupI2CSlave(uint8_t address, volatile uint8_t* registers, uint8_t size, uint32_t magic, i2c_write_callback_t write_callback, i2c_read_callback_t read_callback, i2c_update_callback_t update_callback) {
    i2c_slave_state.first_write = 1;
    i2c_slave_state.offset = 0;
    i2c_slave_state.position = 0;
    i2c_slave_state.registers = registers;
    i2c_slave_state.size = size;
    i2c_slave_state.magic = magic;
    i2c_slave_state.write_callback = write_callback;
    i2c_slave_state.read_callback = read_callback;
    i2c_slave_state.update_callback = update_callback;

    // Enable I2C1
    RCC->APB1PCENR |= RCC_APB1Periph_I2C1;

    // Reset I2C1 to init all regs
    RCC->APB1PRSTR |= RCC_APB1Periph_I2C1;
    RCC->APB1PRSTR &= ~RCC_APB1Periph_I2C1;

    I2C1->CTLR1 |= I2C_CTLR1_SWRST;
    I2C1->CTLR1 &= ~I2C_CTLR1_SWRST;

    // Set module clock frequency
    uint32_t prerate = 2000000; // I2C Logic clock rate, must be higher than the bus clock rate
    I2C1->CTLR2 |= (FUNCONF_SYSTEM_CORE_CLOCK/prerate) & I2C_CTLR2_FREQ;

    // Enable interrupts
    I2C1->CTLR2 |= I2C_CTLR2_ITBUFEN | I2C_CTLR2_ITEVTEN | I2C_CTLR2_ITERREN;

    NVIC_EnableIRQ(I2C1_EV_IRQn); // Event interrupt
    NVIC_SetPriority(I2C1_EV_IRQn, 2 << 4);
    NVIC_EnableIRQ(I2C1_ER_IRQn); // Error interrupt
    NVIC_SetPriority(I2C1_ER_IRQn, 2 << 4);

    // Set clock configuration
    uint32_t clockrate = 1000000; // I2C Bus clock rate, must be lower than the logic clock rate
    I2C1->CKCFGR = ((FUNCONF_SYSTEM_CORE_CLOCK/(3*clockrate))&I2C_CKCFGR_CCR) | I2C_CKCFGR_FS; // Fast mode 33% duty cycle
    //I2C1->CKCFGR = ((FUNCONF_SYSTEM_CORE_CLOCK/(25*clockrate))&I2C_CKCFGR_CCR) | I2C_CKCFGR_DUTY | I2C_CKCFGR_FS; // Fast mode 36% duty cycle
    //I2C1->CKCFGR = (FUNCONF_SYSTEM_CORE_CLOCK/(2*clockrate))&I2C_CKCFGR_CCR; // Standard mode good to 100kHz

    // Set I2C address
    I2C1->OADDR1 = address << 1;
    I2C1->OADDR2 = 0;

    // Enable I2C
    I2C1->CTLR1 |= I2C_CTLR1_PE;

    // Acknowledge bytes when they are received
    I2C1->CTLR1 |= I2C_CTLR1_ACK;
}

__HIGH_CODE
__INTERRUPT
void I2C1_EV_IRQHandler(void) {
    uint16_t STAR1, STAR2 __attribute__((unused));
    STAR1 = I2C1->STAR1;
    STAR2 = I2C1->STAR2;

    if (STAR1 & I2C_STAR1_ADDR) { // Start event
        i2c_slave_state.first_write = 1; // Next write will be the offset
        i2c_slave_state.position = i2c_slave_state.offset; // Reset position
    }

    if (STAR1 & I2C_STAR1_RXNE) { // Write event
        if (i2c_slave_state.first_write) { // First byte written, set the offset
            i2c_slave_state.offset = I2C1->DATAR;
            i2c_slave_state.position = i2c_slave_state.offset;
            i2c_slave_state.first_write = 0;
            i2c_slave_state.writing = false;
        } else { // Normal register write
            i2c_slave_state.writing = true;
            if (i2c_slave_state.position < i2c_slave_state.size) {
                i2c_slave_state.registers[i2c_slave_state.position] = I2C1->DATAR;
                i2c_slave_state.position++;
            } else if (i2c_slave_state.position >= 0xFC) {
                i2c_slave_state.magic_reg[i2c_slave_state.position - 0xFC] = I2C1->DATAR;
                i2c_slave_state.position++;
            }
        }
    }

    if (STAR1 & I2C_STAR1_TXE) { // Read event
        i2c_slave_state.writing = false;
        if (i2c_slave_state.position < i2c_slave_state.size) {
            I2C1->DATAR = i2c_slave_state.registers[i2c_slave_state.position];
            if (i2c_slave_state.read_callback != NULL) {
                i2c_slave_state.read_callback(i2c_slave_state.position);
            }
            i2c_slave_state.position++;
        } else {
            I2C1->DATAR = 0x00;
        }
    }

    if (STAR1 & I2C_STAR1_STOPF) { // Stop event
        I2C1->CTLR1 &= ~(I2C_CTLR1_STOP); // Clear stop
        if (i2c_slave_state.update_callback != NULL) {
            uint32_t a = (i2c_slave_state.magic_reg[3] | i2c_slave_state.magic_reg[2] << 8 | i2c_slave_state.magic_reg[1] << 16 | i2c_slave_state.magic_reg[0] << 24 );
            printf("%x %x %x %x\n", i2c_slave_state.magic_reg[0], i2c_slave_state.magic_reg[1], i2c_slave_state.magic_reg[2], i2c_slave_state.magic_reg[3]);
            printf("%lx %lx\n", i2c_slave_state.magic, a);
            if (a == i2c_slave_state.magic) {
                printf("yippe magic callback!\n");
            }
        }
        if (i2c_slave_state.write_callback != NULL && i2c_slave_state.offset < 0xFC) {
            i2c_slave_state.write_callback(i2c_slave_state.offset, i2c_slave_state.position - i2c_slave_state.offset);
        }
    }
}

void I2C1_ER_IRQHandler(void) __attribute__((interrupt));
void I2C1_ER_IRQHandler(void) {
    uint16_t STAR1 = I2C1->STAR1;

    if (STAR1 & I2C_STAR1_BERR) { // Bus error
        I2C1->STAR1 &= ~(I2C_STAR1_BERR); // Clear error
    }

    if (STAR1 & I2C_STAR1_ARLO) { // Arbitration lost error
        I2C1->STAR1 &= ~(I2C_STAR1_ARLO); // Clear error
    }

    if (STAR1 & I2C_STAR1_AF) { // Acknowledge failure
        I2C1->STAR1 &= ~(I2C_STAR1_AF); // Clear error
    }
}
