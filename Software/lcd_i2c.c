#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "lcd_i2c.h"
#include "hps_0.h"
#include "socal/socal.h"

/*
 * El PIO bidir de 2 bits controla SDA y SCL.
 * Registros del PIO Avalon:
 *   offset 0: data register (leer/escribir)
 *   offset 1: direction register (1=output, 0=input)
 *   offset 2: interrupt mask
 *   offset 3: edge capture
 */
#define PIO_DATA_REG      0
#define PIO_DIR_REG       1

static volatile uint32_t *lcd_pio_addr = NULL;
static uint8_t backlight_state = 1;

/* ================================================================
 * Funciones de bajo nivel: control de pines SDA y SCL
 * El PCF8574 usa open-drain: para HIGH soltamos el pin (input),
 * para LOW lo manejamos como output en 0.
 * ================================================================ */

static void sda_high(void)
{
    /* Input = high impedancia, pull-up externo sube la linea */
    uint32_t dir = alt_read_word(lcd_pio_addr + PIO_DIR_REG);
    dir &= ~(1u << LCD_SDA_BIT);
    alt_write_word(lcd_pio_addr + PIO_DIR_REG, dir);
}

static void sda_low(void)
{
    /* Output 0 */
    uint32_t data = alt_read_word(lcd_pio_addr + PIO_DATA_REG);
    data &= ~(1u << LCD_SDA_BIT);
    alt_write_word(lcd_pio_addr + PIO_DATA_REG, data);

    uint32_t dir = alt_read_word(lcd_pio_addr + PIO_DIR_REG);
    dir |= (1u << LCD_SDA_BIT);
    alt_write_word(lcd_pio_addr + PIO_DIR_REG, dir);
}

static void scl_high(void)
{
    uint32_t dir = alt_read_word(lcd_pio_addr + PIO_DIR_REG);
    dir &= ~(1u << LCD_SCL_BIT);
    alt_write_word(lcd_pio_addr + PIO_DIR_REG, dir);
}

static void scl_low(void)
{
    uint32_t data = alt_read_word(lcd_pio_addr + PIO_DATA_REG);
    data &= ~(1u << LCD_SCL_BIT);
    alt_write_word(lcd_pio_addr + PIO_DATA_REG, data);

    uint32_t dir = alt_read_word(lcd_pio_addr + PIO_DIR_REG);
    dir |= (1u << LCD_SCL_BIT);
    alt_write_word(lcd_pio_addr + PIO_DIR_REG, dir);
}

static uint8_t sda_read(void)
{
    return (alt_read_word(lcd_pio_addr + PIO_DATA_REG) >> LCD_SDA_BIT) & 1;
}

static void i2c_delay(void)
{
    /* ~5us a frecuencias de Linux en ARM */
    usleep(5);
}

/* ================================================================
 * I2C bit-bang
 * ================================================================ */

static void i2c_start(void)
{
    sda_high(); i2c_delay();
    scl_high(); i2c_delay();
    sda_low();  i2c_delay();
    scl_low();  i2c_delay();
}

static void i2c_stop(void)
{
    sda_low();  i2c_delay();
    scl_high(); i2c_delay();
    sda_high(); i2c_delay();
}

static uint8_t i2c_write_byte(uint8_t byte)
{
    int i;
    uint8_t ack;

    for (i = 7; i >= 0; i--)
    {
        if ((byte >> i) & 1)
            sda_high();
        else
            sda_low();

        i2c_delay();
        scl_high();
        i2c_delay();
        scl_low();
        i2c_delay();
    }

    /* Leer ACK */
    sda_high();
    i2c_delay();
    scl_high();
    i2c_delay();
    ack = sda_read();
    scl_low();
    i2c_delay();

    return ack; /* 0 = ACK, 1 = NACK */
}

static void i2c_send(uint8_t data)
{
    i2c_start();
    i2c_write_byte(LCD_I2C_ADDR << 1); /* direccion + write bit */
    i2c_write_byte(data);
    i2c_stop();
}

/* ================================================================
 * LCD HD44780 via PCF8574
 * Mapeo: P7=D7 P6=D6 P5=D5 P4=D4 P3=BL P2=EN P1=RW P0=RS
 * ================================================================ */

#define LCD_RS  (1 << 0)
#define LCD_RW  (1 << 1)
#define LCD_EN  (1 << 2)
#define LCD_BL  (1 << 3)

static void lcd_pulse_enable(uint8_t data)
{
    i2c_send(data | LCD_EN);
    usleep(1);
    i2c_send(data & ~LCD_EN);
    usleep(50);
}

static void lcd_send_nibble(uint8_t nibble, uint8_t rs)
{
    uint8_t data = (nibble << 4) & 0xF0;
    if (rs) data |= LCD_RS;
    if (backlight_state) data |= LCD_BL;
    lcd_pulse_enable(data);
}

static void lcd_send_byte(uint8_t byte, uint8_t rs)
{
    lcd_send_nibble(byte >> 4, rs);   /* nibble alto primero */
    lcd_send_nibble(byte & 0x0F, rs); /* nibble bajo */
}

static void lcd_cmd(uint8_t cmd)
{
    lcd_send_byte(cmd, 0);
}

static void lcd_data(uint8_t data)
{
    lcd_send_byte(data, 1);
}

/* ================================================================
 * API publica
 * ================================================================ */

void lcd_init(void *lw_bridge_base)
{
    lcd_pio_addr = (volatile uint32_t *)(
        (uint8_t *)lw_bridge_base + LCD_PIO_BASE
    );

    /* Iniciar con ambos pines como output en alto */
    alt_write_word(lcd_pio_addr + PIO_DATA_REG, 0x3);
    alt_write_word(lcd_pio_addr + PIO_DIR_REG, 0x3);

    usleep(50000); /* esperar 50ms para que suba VCC de la LCD */

    /* Secuencia de inicializacion en modo 4 bits (HD44780) */
    lcd_send_nibble(0x3, 0); usleep(5000);
    lcd_send_nibble(0x3, 0); usleep(200);
    lcd_send_nibble(0x3, 0); usleep(200);
    lcd_send_nibble(0x2, 0); usleep(200); /* cambiar a 4 bits */

    lcd_cmd(0x28); /* 4 bits, 2 lineas, 5x8 */
    lcd_cmd(0x0C); /* display on, cursor off, blink off */
    lcd_cmd(0x06); /* entry mode: incrementar, no shift */
    lcd_cmd(0x01); /* clear display */
    usleep(2000);

    printf("[INFO] LCD initialized\n");
}

void lcd_clear(void)
{
    lcd_cmd(0x01);
    usleep(2000);
}

void lcd_set_cursor(uint8_t col, uint8_t row)
{
    uint8_t row_offsets[] = {0x00, 0x40};
    if (row >= LCD_ROWS) row = LCD_ROWS - 1;
    if (col >= LCD_COLS) col = LCD_COLS - 1;
    lcd_cmd(0x80 | (col + row_offsets[row]));
}

void lcd_print(const char *str)
{
    while (*str)
        lcd_data((uint8_t)*str++);
}

void lcd_print_centered(uint8_t row, const char *str)
{
    int len = strlen(str);
    int col = 0;

    if (len < LCD_COLS)
        col = (LCD_COLS - len) / 2;

    lcd_set_cursor(col, row);
    lcd_print(str);
}

void lcd_backlight(uint8_t on)
{
    backlight_state = on ? 1 : 0;
    /* Enviar un byte con solo el backlight para actualizar */
    uint8_t data = backlight_state ? LCD_BL : 0;
    i2c_send(data);
}