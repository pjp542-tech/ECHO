# LCD functions

Rows: **0–3**. Columns: **0–19**. Include `lcd.h`.

| Example | What it does |
| --- | --- |
| `lcd_init(&lcd1);` | Initialize once in `setup()`. |
| `lcd_print(&lcd1, 0, "Hello");` | Print at the start of row 0. |
| `lcd_gotoxy(&lcd1, 5, 2);` | Move to column 5, row 2 (column first). |
| `lcd_puts(&lcd1, "Hello");` | Print text at the current cursor. |
| `lcd_putchar(&lcd1, 'A');` | Print one character at the cursor. |
| `lcd_clear(&lcd1);` | Clear the screen and return the cursor home. |
| `lcd_send_cmd(&lcd1, 0x0C);` | Send a raw command (here: display on, cursor off). |
| `lcd_send_data(&lcd1, 'A');` | Send a character directly. |

## Wiring

| LCD pin | Connect to |
| --- | --- |
| SDA | PB7 (D4) |
| SCL | PA15 (D5) |
| GND | Board GND |
| VCC | Supply matching the LCD module's rated voltage |
