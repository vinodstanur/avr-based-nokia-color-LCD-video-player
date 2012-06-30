/*
ATMEGA32 BASED VIDEO PLAYER on NOKIA 132x132 color LCD
author: Vinod S
email:  vinodstanur at gmail dot com
date:   25/6/2012
homepage:    http://blog.vinu.co.in
project page: http://blog.vinu.co.in/2012/06/avr-video-player-on-nokia-color-lcd.html
 
compiler: avr-gcc
Copyright (C) <2012>  <http://blog.vinu.co.in>
 
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
 
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
 
#include<avr/io.h>
#define F_CPU 16000000
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>
 
#define USART_BAUD 115200ul
#define baud (F_CPU/(8*USART_BAUD))-1
 
#define byte unsigned char
 
#define RC5_LED PB0
#define SD_CS PB4
#define LCD_CS PB3
#define CHIPSELECT(x) {PORTB |= (1<<LCD_CS)|(1<<SD_CS);PORTB &= ~(1<<x);}
#define SPI_OFF SPCR &= ~(1<<SPE)
#define SPI_ON SPCR |= (1<<SPE)
 
#define LEFT_SWITCH() ((PIND & (1<<2))==0)
#define RIGHT_SWITCH() ((PIND & (1<3))==0)
#define SWITCH_EVENT() ((PIND & ((1<<2)|(1<<3))) != 0b00001100)
#define SWITCH_ENABLE() do{DDRD &= ~((1<<PD2)|(1<<PD3));PORTD |= ((1<<PD2)|(1<<PD3));} while(0)
 
void setPixel(unsigned char r, unsigned char g, unsigned char b);
void lcd_init();
void LCD_COMMAND(unsigned char cmd);
void LCD_DATA(unsigned char cmd);
void shiftBits(unsigned char b);
void setPixel(unsigned char r, unsigned char g, unsigned char b);
void uart_send_byte(unsigned char c);
void lcd_init();
void display_black();
void display_pattern();
unsigned char readdata;
unsigned int count;
unsigned long int arg = 0;
unsigned char mmc_buf[512];
unsigned char mmc_audio_buffer[256];
unsigned int fat_start, dir_start, data_start;
unsigned char sect_per_clust;
unsigned int STARTING_CLUSTER;
void spi_init();
void spi_write(char);
unsigned char spi_read();
unsigned char command(unsigned char, unsigned long int, unsigned char);
char mmc_init();
void mmc_read_sector(unsigned long int);
void fat16_init();
void print_num(unsigned long int, char);
unsigned int scan_root_dir(unsigned char *, char[], char);
char display_cluster(unsigned int);
unsigned int find_next_cluster(unsigned int);
void pwm_init();
void mmc_read_to_buffer(unsigned long int, unsigned char[]);
void pwm_init();
volatile unsigned char stack = 0, point1 = 0;
volatile char acount = 0;
char NEXT_OR_PREVIOUS = 1;
 
ISR(TIMER1_COMPA_vect)
{
    OCR2 = mmc_audio_buffer[stack++];
}
 
void timer1_init()
{
    TCCR1B |= (1 << WGM12) | (1 << CS10);
    TCNT1 = 0;
    OCR1A = 1500;
    TIMSK |= (1 << OCIE1A);
}
 
void uart_init()
{
    UBRRH = (unsigned char)(baud >> 8);
    UBRRL = (unsigned char)baud;
    UCSRB = (1 << TXEN) | (1 << RXCIE);
    DDRD |= 1 << PD1;
    UCSRC = (1 << URSEL) | (3 << UCSZ0);
    UCSRA = 1 << U2X;
}
 
void uart_send(unsigned char c)
{
    while (!(UCSRA & (1 << UDRE))) ;
    UDR = c;
    _delay_ms(1);
}
 
void string(unsigned char *p, unsigned char c)
{
    while (*p)
    uart_send(*p++);
}
 
int main()
{
    unsigned char fname[12];
    unsigned int cluster;
    spi_init();
    CHIPSELECT(LCD_CS);
    lcd_init();
    timer1_init();
    uart_init();
    SWITCH_ENABLE();
    pwm_init();
    _delay_ms(50);
    CHIPSELECT(SD_CS);
    while (mmc_init()) ;
    fat16_init();
    while (1) {
        while ((cluster =
        scan_root_dir("VIN", fname, NEXT_OR_PREVIOUS)) == 0) {
            NEXT_OR_PREVIOUS = 1;
        }
        NEXT_OR_PREVIOUS = 1;
        string(fname, 1);
         
        CHIPSELECT(LCD_CS);
         
        display_pattern();
        _delay_ms(500);
        display_black();
         
        //Column Adress Set
        LCD_COMMAND(0x2A);
        LCD_DATA(0);
        LCD_DATA(131);
         
        //Page Adress Set
        LCD_COMMAND(0x2B);
        LCD_DATA(33);
        LCD_DATA(33 + 64);
         
        LCD_COMMAND(0x2C);
        CHIPSELECT(SD_CS);
        acount = 0;
        sei();
        while (cluster != 0xffff) {
            if (display_cluster(cluster))
            break;
            cluster = find_next_cluster(cluster);
        }
        cli();
        if (LEFT_SWITCH())
        NEXT_OR_PREVIOUS = 0;
        else if (RIGHT_SWITCH())
        NEXT_OR_PREVIOUS = 1;
    }
    return 0;
}
 
char display_cluster(unsigned int cluster)
{
    static unsigned char r, g, b, check = 0;
    static unsigned int pixel = 0;
    unsigned long int sector;
    int i, j;
    sector = ((unsigned long int)(cluster - 2) * sect_per_clust);
    sector += data_start;
    for (i = 0; i < sect_per_clust; i++) {
        mmc_read_sector(sector);
        CHIPSELECT(LCD_CS);
        for (j = 0; j < 512; j++) {
            if (acount == 14) {
                mmc_audio_buffer[point1++] = mmc_buf[j];
                acount = 0;
                } else {
                SPI_OFF;
                PORTB &= ~(1 << PB7);
                PORTB |= 1 << PB5;
                PORTB |= 1 << PB7;
                SPI_ON;
                SPDR = mmc_buf[j];
                if (SWITCH_EVENT()) {
                    while (!(SPSR & (1 << SPIF))) ;
                    CHIPSELECT(SD_CS);
                    return 1;
                }
                while (!(SPSR & (1 << SPIF))) ;
                acount++;
            }
        }
         
        CHIPSELECT(SD_CS);
        sector += 1;
    }
    return 0;
}
 
unsigned int find_next_cluster(unsigned int cluster)
{
    unsigned int cluster_index_in_buff;
    cluster_index_in_buff = (2 * (cluster % 256));
    mmc_read_sector(fat_start + cluster / 256);
    return ((mmc_buf[cluster_index_in_buff + 1] << 8) +
    mmc_buf[cluster_index_in_buff]);
}
 
void mmc_read_to_buffer(unsigned long int sector, unsigned char a[])
{
    int i;
    sector *= 512;
    if (command(17, sector, 0xff) != 0)
    while (spi_read() != 0) ;
    while (spi_read() != 0xfe) ;
    for (i = 0; i < 512; i++) {
        SPDR = 0xff;
        while (!(SPSR & (1 << SPIF))) ;
        a[i] = SPDR;
    }
    spi_write(0xff);
    spi_write(0xff);
}
 
unsigned int scan_root_dir(unsigned char *FILE_EXTENSION, char FNAME[],
char UP_DOWN)
{
    while (1) {
        unsigned int i;
        static unsigned char read_end = 0;
        static int base_count = -32, sect_plus = 0;
        if (UP_DOWN == 1) {
            base_count += 32;
            if (base_count == 512) {
                base_count = 0;
                sect_plus += 1;
            };
            } else {
            base_count -= 32;
            if (base_count == -32) {
                base_count = (512 - 32);
                sect_plus -= 1;
            }
            if (sect_plus < 0) {
                sect_plus = 0;
                base_count = 0;
            }
        }
        while (1) {
            mmc_read_sector(dir_start + sect_plus);
            while (base_count < 512) {
                if (mmc_buf[base_count] == 0) {
                    read_end = 1;
                    break;
                }
                if ((mmc_buf[1] != 0)
                && (mmc_buf[base_count + 2] != 0)
                && (mmc_buf[base_count] != 0xe5)
                && (mmc_buf[base_count] != 0x00)
                && ((mmc_buf[base_count + 11] & 0b00011110)
                == 0)
                &&
                (strncmp
                (mmc_buf + base_count + 8, FILE_EXTENSION,
                3)
                == 0)) {
                    for (i = 0; i < 11; i++)
                    FNAME[i] =
                    mmc_buf[base_count + i];
                    FNAME[11] = 0;
                    return (STARTING_CLUSTER =
                    (unsigned
                    int)((mmc_buf[27 + base_count]
                    << 8)
                    + mmc_buf[26 +
                    base_count]));
                }
                if (UP_DOWN)
                base_count += 32;
                else
                base_count -= 32;
            }
            base_count = 0;
            sect_plus++;
            if (read_end) {
                base_count = -32;
                sect_plus = 0;
                read_end = 0;
                return 0;
            }
        }
    }
}
 
void fat16_init()        //BOOT SECTOR SCANNING//
{
    mmc_read_sector(0);
     
    if ((mmc_buf[0x36] == 'F') && (mmc_buf[0x39] == '1')
    && (mmc_buf[0x3a] == '6'))
    string("FAT16 DETECTED", 1);
    else {
        string("NOT A FAT16", 1);
        while (1) ;
    }
    _delay_ms(500);
    fat_start = mmc_buf[0x0e];
    dir_start = (fat_start + (((mmc_buf[0x17] << 8) + mmc_buf[0x16]) * 2));
    data_start =
    (dir_start +
    ((((mmc_buf[0x12] << 8) + (mmc_buf[0x11])) * 32) / 512));
    sect_per_clust = mmc_buf[0x0d];
}
 
void mmc_read_sector(unsigned long int sector)
{
    int i;
     
    sector *= 512;
    if (command(17, sector, 0xff) != 0)
    while (spi_read() != 0) ;
    while (spi_read() != 0xfe) ;
    for (i = 0; i < 512; i++)
    mmc_buf[i] = spi_read();
    spi_write(0xff);
    spi_write(0xff);
}
 
char mmc_init()
{
    int u = 0;
    unsigned char ocr[10];
    PORTB |= 1 << SD_CS;
    for (u = 0; u < 50; u++) {
        spi_write(0xff);
    }
    PORTB &= ~(1 << SD_CS);
    _delay_ms(1);
    count = 0;
    while (command(0, 0, 0x95) != 1 && (count++ < 1000)) ;
    if (count > 900) {
        string("CARD ERROR-CMD0 ", 1);
        _delay_ms(500);
        return 1;
    }
    if (command(8, 0x1AA, 0x87) == 1) {    /* SDC ver 2.00 */
        for (u = 0; u < 4; u++)
        ocr[u] = spi_read();
        if (ocr[2] == 0x01 && ocr[3] == 0xAA) {    /* The card can work at vdd range of 2.7-3.6V */
            count = 0;
            do
            command(55, 0, 0xff);
            while (command(41, 1UL << 30, 0xff) && count++ < 1000);    /* ACMD41 with HCS bit */
            if (count > 900) {
                string("ERROR SDHC 41", 1);
                return 1;
            }
            count = 0;
            if (command(58, 0, 0xff) == 0 && count++ < 1000) {    /* Check CCS bit */
                for (u = 0; u < 4; u++)
                ocr[u] = spi_read();
            }
        }
        } else {
        command(55, 0, 0xff);
        if (command(41, 0, 0xff) > 1) {
            count = 0;
            while ((command(1, 0, 0xff) != 0) && (count++ < 1000)) ;
            if (count > 900) {
                string("CARD ERROR-CMD1 ", 1);
                _delay_ms(500);
                return 1;
            }
            } else {
            count = 0;
            do {
                command(55, 0, 0xff);
            }
            while (command(41, 0, 0xff) != 0 && count < 1000);
        }
         
        if (command(16, 512, 0xff) != 0) {
            string("CARD ERROR-CMD16 ", 1);
            _delay_ms(500);
            return 1;
        }
    }
    string("MMC INITIALIZED!", 1);
    _delay_ms(500);
    SPCR &= ~(1 << SPR1);    //increase SPI clock from f/32 to f/2
    return 0;
}
 
unsigned char command(unsigned char command,
unsigned long int fourbyte_arg, unsigned char CRCbits)
{
    unsigned char retvalue, n;
    spi_write(0xff);
    spi_write(0b01000000 | command);
    spi_write((unsigned char)(fourbyte_arg >> 24));
    spi_write((unsigned char)(fourbyte_arg >> 16));
    spi_write((unsigned char)(fourbyte_arg >> 8));
    spi_write((unsigned char)fourbyte_arg);
    spi_write(CRCbits);
    n = 10;
    do
    retvalue = spi_read();
    while ((retvalue & 0x80) && --n);
     
    return retvalue;
}
 
unsigned char spi_read()
{
    SPDR = 0xff;
    while (!(SPSR & (1 << SPIF))) ;
    return SPDR;
}
 
void spi_write(char cData)
{
    SPDR = cData;
    while (!(SPSR & (1 << SPIF))) ;
}
 
void spi_init()
{
    PORTC = 0;
    PORTB = (1 << PB4) | (1 << PB3);
    DDRC |= 1 << PC4;
    DDRB |= (1 << 5) | (1 << 7) | (1 << 4) | (1 << 3);
    _delay_ms(10);
    PORTB |= (1 << SD_CS) | (1 << LCD_CS);
    SPCR =
    (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << CPHA) | (1 << CPOL);
    SPSR = 1;
}
 
void lcd_init()
{
     
    SPI_OFF;
     
    PORTB &= ~(1 << PB7);
    PORTB &= ~(1 << PB5);
    PORTB |= 1 << PB7;
     
    //RESET
    PORTC |= 1 << PC4;
    PORTC &= ~(1 << PC4);
    PORTC |= 1 << PC4;
    PORTC &= ~(1 << PC4);
    PORTC |= 1 << PC4;
    PORTC &= ~(1 << PC4);
    PORTC |= 1 << PC4;
     
    SPI_ON;
     
    LCD_COMMAND(0x01);
     
    //Sleep Out
    LCD_COMMAND(0x11);
     
    //Booster ON
    LCD_COMMAND(0x02);
     
    _delay_ms(10);
     
    //Display On
    LCD_COMMAND(0x29);
     
    //Normal display mode
    // LCD_COMMAND(0x13);
     
    //Display inversion on
    //LCD_COMMAND(0x21);
     
    //Data order
    LCD_COMMAND(0xBA);
     
    //Memory data access control
    LCD_COMMAND(0x36);
     
    //LCD_DATA(8|64);   //rgb + MirrorX
    LCD_DATA(8 | 128);    //rgb + MirrorY
     
    LCD_COMMAND(0x3A);
    LCD_DATA(5);        //16-Bit per Pixel
     
    //Set Constrast
    LCD_COMMAND(10);
    LCD_DATA(50);
    display_black();
     
}
 
void display_black()
{
    int i, j;
    for (i = -66; i < 66; i++) {
        for (j = -66; j < 66; j++) {
            setPixel(0, 0, 0);
        }
    }
}
 
void display_pattern()
{
    //Column Adress Set
    LCD_COMMAND(0x2A);
    LCD_DATA(0);
    LCD_DATA(131);
     
    //Page Adress Set
    LCD_COMMAND(0x2B);
    LCD_DATA(0);
    LCD_DATA(131);
     
    //Memory Write
     
    LCD_COMMAND(0x2C);
    int16_t i, j;
    int32_t p;
     
    for (i = -66; i < 66; i++) {
        for (j = -66; j < 66; j++) {
            p = ((i) * (i)) + ((j) * (j));
            if (p >= 2000 && p < 3000)
            setPixel(255, 0, 0);
            else if (p >= 1000 && p < 2000)
            setPixel(0, 255, 0);
            else if (p < 1000)
            setPixel(0, 0, 255);
            else if (i % 5 == 0)
            setPixel(255, 255, 255);
            else
            setPixel(0, 0, 0);
        }
    }
}
 
//send data
void LCD_DATA(byte data)
{
     
    SPI_OFF;
    PORTB &= ~(1 << PB7);
    PORTB |= 1 << PB5;
    PORTB |= 1 << PB7;
    SPI_ON;
    SPDR = data;
    while (!(SPSR & (1 << SPIF))) ;
}
 
void LCD_COMMAND(byte data)
{
    SPI_OFF;
    PORTB &= ~(1 << PB7);
    PORTB &= ~(1 << PB5);
    PORTB |= 1 << PB7;
    SPI_ON;
    SPDR = data;
    while (!(SPSR & (1 << SPIF))) ;
}
 
void setPixel(byte r, byte g, byte b)
{
    LCD_DATA((b & 248) | g >> 5);
    LCD_DATA(((g << 3) & 0b11100000) | r >> 3);
}
 
void pwm_init()
{
    TCCR2 |= (1 << WGM20) | (1 << WGM21) | (1 << COM21) | (1 << CS20);
    DDRD |= (1 << PD7);
}
