/*******************************************************************************
 * Environmental Comfort Controller - PIC18F45K22
 *
 * Materia: Electrónica Integrada
 * Microcontrolador: PIC18F45K22
 * Compilador: MPLAB XC8
 * Simulación: PROTEUS
 * Autor: Luis Alexis Calvillo Deveze
 * Fecha: 2026
 *
 * Descripción:
 * Controlador de confort ambiental que lee luz (LDR) y temperatura (LM35)
 * y controla lámpara, ventilador y calefactor.
 * Interfaz: RS232, LED indicadores, LCD, teclado matricial.
 *******************************************************************************/

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// CONFIGURATION BITS
#pragma config FOSC = INTIO67
#pragma config PLLCFG = OFF
#pragma config PRICLKEN = ON
#pragma config FCMEN = OFF
#pragma config IESO = OFF
#pragma config PWRTEN = ON
#pragma config BOREN = ON
#pragma config BORV = 190
#pragma config WDTEN = OFF
#pragma config WDTPS = 32768
#pragma config CCP2MX = PORTC1
#pragma config PBADEN = OFF
#pragma config CCP3MX = PORTB5
#pragma config HFOFST = ON
#pragma config T3CMX = PORTC0
#pragma config P2BMX = PORTB5
#pragma config MCLRE = EXTMCLR
#pragma config STVREN = ON
#pragma config LVP = OFF
#pragma config XINST = OFF
#pragma config DEBUG = OFF
#pragma config CP0 = OFF
#pragma config CP1 = OFF
#pragma config CP2 = OFF
#pragma config CP3 = OFF
#pragma config CPB = OFF
#pragma config CPD = OFF
#pragma config WRT0 = OFF
#pragma config WRT1 = OFF
#pragma config WRT2 = OFF
#pragma config WRT3 = OFF
#pragma config WRTC = OFF
#pragma config WRTB = OFF
#pragma config WRTD = OFF
#pragma config EBTR0 = OFF
#pragma config EBTR1 = OFF
#pragma config EBTR2 = OFF
#pragma config EBTR3 = OFF
#pragma config EBTRB = OFF

#define _XTAL_FREQ 8000000

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
#define LDR_ADC_CHANNEL      1   // AN1 - RA1
#define LM35_ADC_CHANNEL     0   // AN0 - RA0
#define LAMP_PWM_PIN         RC1 // CCP2
#define FAN_PWM_PIN          RC2 // CCP1
#define HEATER_PIN           RC0 // Digital output
#define LED_COMFORT          RB0 // Green LED - comfort zone
#define LED_WARNING          RB1 // Yellow LED - warning
#define LED_ALARM            RB2 // Red LED - alarm
#define LCD_RS               RB3
#define LCD_EN               RB4
#define LCD_D4               RB5
#define LCD_D5               RB6
#define LCD_D6               RB7
#define LCD_D7               RC3

// ============================================================================
// THRESHOLDS
// ============================================================================
#define LUX_COMFORT_MIN      300
#define LUX_COMFORT_MAX      800
#define TEMP_COMFORT_MIN     20.0
#define TEMP_COMFORT_MAX     26.0

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
volatile uint16_t adc_results[2] = {0, 0};
volatile uint8_t adc_channel = 0;
volatile float temperature_c = 0.0;
volatile float lux = 0.0;
volatile bool uart_tx_busy = false;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
void system_init(void);
void oscillator_init(void);
void adc_init(void);
void pwm_init(void);
void uart_init(void);
void gpio_init(void);
void lcd_init(void);
void timer0_init(void);
void interrupt_init(void);

float read_lux(void);
float read_temperature(void);
void update_outputs(void);
void lcd_update(void);
void uart_send_status(void);
void handle_uart_commands(void);

// LCD functions
void lcd_cmd(uint8_t cmd);
void lcd_data(uint8_t data);
void lcd_write_string(const char *str);
void lcd_set_cursor(uint8_t row, uint8_t col);
void lcd_clear(void);

// UART functions
void uart_write_char(char c);
void uart_write_string(const char *str);

// ============================================================================
// MAIN
// ============================================================================
void main(void) {
    system_init();

    lcd_clear();
    lcd_write_string("Comfort Ctrl");
    lcd_set_cursor(1, 0);
    lcd_write_string("Iniciando...");
    __delay_ms(1500);

    while (1) {
        // Read sensors (alternating channels)
        lux = read_lux();
        temperature_c = read_temperature();

        // Handle UART commands
        handle_uart_commands();

        // Update outputs based on readings
        update_outputs();

        // Update display
        lcd_update();

        // Send periodic status
        static uint16_t status_counter = 0;
        if (++status_counter >= 10000) {
            status_counter = 0;
            uart_send_status();
        }

        __delay_ms(5);
    }
}

// ============================================================================
// INTERRUPT SERVICE ROUTINE
// ============================================================================
void __interrupt() isr(void) {
    // ADC interrupt
    if (PIR1bits.ADIF) {
        adc_results[adc_channel] = ((uint16_t)ADRESH << 8) | ADRESL;
        PIR1bits.ADIF = 0;

        // Switch channel for next reading
        adc_channel = (adc_channel + 1) % 2;
        ADCON0bits.CHS = adc_channel;
    }

    // UART RX interrupt
    if (PIR1bits.RC1IF) {
        char received = RCREG1;
        // Simple command parser could go here
        if (received == 's' || received == 'S') {
            // Request status
        }
    }

    // UART TX interrupt
    if (PIR1bits.TX1IF && uart_tx_busy) {
        // Handled by polling in this simple version
    }

    // Timer0
    if (INTCONbits.TMR0IF) {
        TMR0H = 0x0B;
        TMR0L = 0xDC;
        INTCONbits.TMR0IF = 0;
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================
void system_init(void) {
    oscillator_init();
    adc_init();
    pwm_init();
    uart_init();
    gpio_init();
    lcd_init();
    timer0_init();
    interrupt_init();
}

void oscillator_init(void) {
    OSCCON = 0x70;  // 8 MHz
    while (!OSCCONbits.HFIOFS);
}

void adc_init(void) {
    ADCON0 = 0x00;
    ADCON1 = 0x00;  // VDD/VSS reference
    ADCON2 = 0x8A;  // Right justified, 12 TAD, FOSC/32

    TRISAbits.TRISA0 = 1; ANSELAbits.ANSA0 = 1;  // LM35
    TRISAbits.TRISA1 = 1; ANSELAbits.ANSA1 = 1;  // LDR

    ADCON0bits.CHS = 0;
    ADCON0bits.ADON = 1;
    PIE1bits.ADIE = 1;
}

void pwm_init(void) {
    TRISCbits.TRISC1 = 0;  // Lamp (CCP2)
    TRISCbits.TRISC2 = 0;  // Fan (CCP1)

    PR2 = 0xFF;
    T2CON = 0x06;  // Prescaler 1:16

    CCP1CON = 0x0C; CCPR1L = 0; CCP1CONbits.DC1B = 0;  // Fan
    CCP2CON = 0x0C; CCPR2L = 0; CCP2CONbits.DC2B = 0;  // Lamp
}

void uart_init(void) {
    TRISCbits.TRISC6 = 0; TRISCbits.TRISC7 = 1;
    TXSTA1 = 0x24; RCSTA1 = 0x90; BAUDCON1 = 0x08;
    SPBRG1 = 51;  // 9600 @ 8MHz
    PIE1bits.RCIE = 1;
}

void gpio_init(void) {
    TRISCbits.TRISC0 = 0;  // Heater
    TRISBbits.TRISB0 = 0;  // Green LED
    TRISBbits.TRISB1 = 0;  // Yellow LED
    TRISBbits.TRISB2 = 0;  // Red LED
    LATCbits.LATC0 = 0;
    LATBbits.LATB0 = 0;
    LATBbits.LATB1 = 0;
    LATBbits.LATB2 = 0;
}

void lcd_init(void) {
    TRISB &= 0x07; TRISCbits.TRISC3 = 0;
    __delay_ms(20);
    lcd_cmd(0x33); __delay_ms(5);
    lcd_cmd(0x32); __delay_ms(5);
    lcd_cmd(0x28); __delay_ms(1);
    lcd_cmd(0x0C); __delay_ms(1);
    lcd_cmd(0x06); __delay_ms(1);
    lcd_clear();
}

void timer0_init(void) {
    T0CON = 0x87;
    TMR0H = 0x0B; TMR0L = 0xDC;
    INTCONbits.TMR0IE = 1;
}

void interrupt_init(void) {
    INTCONbits.GIE = 1; INTCONbits.PEIE = 1;
}

// ============================================================================
// SENSOR READING
// ============================================================================
float read_lux(void) {
    // LDR in voltage divider: 10k fixed + LDR
    // ADC reads voltage across LDR
    // Lux calculation depends on specific LDR calibration
    // This is a simplified approximation
    float voltage = (adc_results[1] * 5.0) / 1024.0;
    // Approximate: higher voltage = more light = more lux
    // Calibrate based on your specific LDR
    return voltage * 200.0;  // Rough scaling
}

float read_temperature(void) {
    float voltage = (adc_results[0] * 5.0) / 1024.0;
    return voltage / 0.01;  // LM35: 10mV/°C
}

// ============================================================================
// OUTPUT CONTROL
// ============================================================================
void update_outputs(void) {
    bool temp_ok = (temperature_c >= TEMP_COMFORT_MIN && temperature_c <= TEMP_COMFORT_MAX);
    bool lux_ok = (lux >= LUX_COMFORT_MIN && lux <= LUX_COMFORT_MAX);
    bool comfort_zone = temp_ok && lux_ok;

    uint16_t lamp_duty = 0;
    uint16_t fan_duty = 0;
    bool heater_on = false;

    // Lamp control - supplement light if too dark
    if (lux < LUX_COMFORT_MIN) {
        lamp_duty = (uint16_t)((LUX_COMFORT_MIN - lux) / LUX_COMFORT_MIN * 1023);
        if (lamp_duty > 1023) lamp_duty = 1023;
    }

    // Fan control - cool if too hot
    if (temperature_c > TEMP_COMFORT_MAX) {
        fan_duty = (uint16_t)((temperature_c - TEMP_COMFORT_MAX) / 10.0 * 1023);
        if (fan_duty > 1023) fan_duty = 1023;
    }

    // Heater - warm if too cold
    if (temperature_c < TEMP_COMFORT_MIN) {
        heater_on = true;
    }

    // Apply PWM
    CCPR2L = (lamp_duty >> 2) & 0xFF;  // Lamp
    CCP2CONbits.DC2B = lamp_duty & 0x03;
    CCPR1L = (fan_duty >> 2) & 0xFF;   // Fan
    CCP1CONbits.DC1B = fan_duty & 0x03;
    LATCbits.LATC0 = heater_on;        // Heater

    // LED indicators
    LATBbits.LATB0 = comfort_zone;     // Green
    LATBbits.LATB1 = (!temp_ok || !lux_ok) && !(!temp_ok && !lux_ok);  // Yellow - one out of range
    LATBbits.LATB2 = (!temp_ok && !lux_ok);  // Red - both out of range
}

// ============================================================================
// LCD DISPLAY
// ============================================================================
void lcd_update(void) {
    char line1[17], line2[17];

    lcd_set_cursor(0, 0);
    sprintf(line1, "T:%.1fC L:%.0f lx", temperature_c, lux);
    lcd_write_string(line1);

    lcd_set_cursor(1, 0);
    if (temperature_c >= TEMP_COMFORT_MIN && temperature_c <= TEMP_COMFORT_MAX &&
        lux >= LUX_COMFORT_MIN && lux <= LUX_COMFORT_MAX) {
        sprintf(line2, "CONFORT OK");
    } else if (temperature_c < TEMP_COMFORT_MIN || lux < LUX_COMFORT_MIN) {
        sprintf(line2, "BAJO: Cal/ Luz");
    } else {
        sprintf(line2, "ALTO: Ven/ Osc");
    }
    lcd_write_string(line2);
}

// ============================================================================
// UART
// ============================================================================
void uart_write_char(char c) {
    while (!TXSTA1bits.TRMT);
    TXREG = c;
}

void uart_write_string(const char *str) {
    while (*str) uart_write_char(*str++);
}

void uart_send_status(void) {
    char status[128];
    sprintf(status, "STATUS: T=%.1fC, Lux=%.0f, Lamp=%d%%, Fan=%d%%, Heat=%s\n",
            temperature_c, lux,
            (CCPR2L * 4 + CCP2CONbits.DC2B) * 100 / 1023,
            (CCPR1L * 4 + CCP1CONbits.DC1B) * 100 / 1023,
            LATCbits.LATC0 ? "ON" : "OFF");
    uart_write_string(status);
}

void handle_uart_commands(void) {
    if (PIR1bits.RC1IF) {
        char cmd = RCREG1;
        switch (cmd) {
            case 's': case 'S': uart_send_status(); break;
            case 'r': case 'R':
                uart_write_string("RESET\n");
                RESET();
                break;
        }
    }
}

// ============================================================================
// LCD HELPERS
// ============================================================================
void lcd_cmd(uint8_t cmd) {
    LATBbits.LATB3 = 0;
    LATB = (LATB & 0x0F) | (cmd & 0xF0);
    LATBbits.LATB4 = 1; __delay_us(1); LATBbits.LATB4 = 0; __delay_us(1);
    LATB = (LATB & 0x0F) | ((cmd & 0x0F) << 4);
    LATBbits.LATB4 = 1; __delay_us(1); LATBbits.LATB4 = 0; __delay_ms(2);
}

void lcd_data(uint8_t data) {
    LATBbits.LATB3 = 1;
    LATB = (LATB & 0x0F) | (data & 0xF0);
    LATBbits.LATB4 = 1; __delay_us(1); LATBbits.LATB4 = 0; __delay_us(1);
    LATB = (LATB & 0x0F) | ((data & 0x0F) << 4);
    LATBbits.LATB4 = 1; __delay_us(1); LATBbits.LATB4 = 0; __delay_ms(2);
}

void lcd_write_string(const char *str) {
    while (*str) lcd_data(*str++);
}

void lcd_set_cursor(uint8_t row, uint8_t col) {
    lcd_cmd((row == 0 ? 0x80 : 0xC0) + col);
}

void lcd_clear(void) {
    lcd_cmd(0x01); __delay_ms(2);
}