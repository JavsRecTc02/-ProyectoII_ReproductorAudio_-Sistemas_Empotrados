#ifndef LCD_I2C_H
#define LCD_I2C_H

#include <stdint.h>

/*
 * Driver LCD 16x2 con interfaz I2C via PCF8574
 * Direccion I2C: 0x27 (A0=A1=A2=1 por pull-up)
 *
 * Mapeo del PCF8574 a la LCD HD44780:
 * P7=D7, P6=D6, P5=D5, P4=D4
 * P3=BL (backlight), P2=EN, P1=RW, P0=RS
 */

#define LCD_I2C_ADDR    0x27
#define LCD_COLS        16
#define LCD_ROWS        2

/* Inicializa la LCD */
void lcd_init(void *lw_bridge_base);

/* Limpia la pantalla */
void lcd_clear(void);

/* Posiciona el cursor */
void lcd_set_cursor(uint8_t col, uint8_t row);

/* Escribe un string en la posicion actual */
void lcd_print(const char *str);

/* Escribe un string centrado en una fila */
void lcd_print_centered(uint8_t row, const char *str);

/* Apaga/enciende backlight */
void lcd_backlight(uint8_t on);

#endif /* LCD_I2C_H */