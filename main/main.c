#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

// --- EKRAN AYARLARI ---
// Çizgileri yok etmek için bu değerlerle oyna (Genelde 2,3 veya 0,0 olur)
#define OFFSET_X  2
#define OFFSET_Y  3

#define LCD_WIDTH  128
#define LCD_HEIGHT 128

#define BUTTON_NEXT_PIN   1  // 4 Pinli buton
#define SWITCH_MODE_PIN   2  // 3 Pinli switch

// --- PİN AYARLARI ---
#define LCD_HOST    SPI2_HOST
#define PIN_NUM_MOSI 11
#define PIN_NUM_CLK  12
#define PIN_NUM_CS   10
#define PIN_NUM_DC   8
#define PIN_NUM_RST  9


spi_device_handle_t spi;


// Buton ayarları


// --- BASİT 5x7 FONT TANIMI (ASCII) ---
// Harflerin piksel haritası. Bunu elle yazmak zordur, standart bir font tablosudur.
const uint8_t simpleFont[][5] = {
    {0,0,0,0,0}, {0,0,253,0,0}, {0,96,0,96,0}, {20,127,20,127,20}, // Space, !, ", #
    {36,42,127,42,18}, {35,19,8,100,98}, {54,73,85,34,80}, {0,5,3,0,0}, // $, %, &, '
    {0,28,34,65,0}, {0,65,34,28,0}, {20,8,62,8,20}, {8,8,62,8,8}, // (, ), *, +
    {0,80,48,0,0}, {8,8,8,8,8}, {0,96,96,0,0}, {32,16,8,4,2}, // ,, -, ., /
    {62,81,73,69,62}, {0,66,127,64,0}, {66,97,81,73,70}, {33,65,69,75,49}, // 0-3
    {24,20,18,127,16}, {39,69,69,69,57}, {60,74,73,73,48}, {1,113,9,5,3}, // 4-7
    {54,73,73,73,54}, {6,73,73,41,30}, {0,54,54,0,0}, {0,86,54,0,0}, // 8-9, :, ;
    {8,20,34,65,0}, {20,20,20,20,20}, {0,65,34,20,8}, {2,1,81,9,6}, // <, =, >, ?
    {50,73,121,65,62}, {126,17,17,17,126}, {127,73,73,73,54}, {62,65,65,65,34}, // @, A, B, C
    {127,65,65,34,28}, {127,73,73,73,65}, {127,9,9,9,1}, {62,65,73,73,122}, // D, E, F, G
    {127,8,8,8,127}, {0,65,127,65,0}, {32,64,65,63,1}, {127,8,20,34,65}, // H, I, J, K
    {127,64,64,64,64}, {127,2,12,2,127}, {127,4,8,16,127}, {62,65,65,65,62}, // L, M, N, O
    {127,9,9,9,6}, {62,65,81,33,94}, {127,9,25,41,70}, {70,73,73,73,49}, // P, Q, R, S
    {1,1,127,1,1}, {63,64,64,64,63}, {31,32,64,32,31}, {63,64,56,64,63}, // T, U, V, W
    {99,20,8,20,99}, {7,8,112,8,7}, {97,81,73,69,67}, {0,127,65,65,0}, // X, Y, Z, [
    {2,4,8,16,32}, {0,65,65,127,0}, {4,2,1,2,4}, {64,64,64,64,64}, // \, ], ^, _
    {0,1,2,4,0}, {32,84,84,84,120}, {127,72,68,68,56}, {56,68,68,68,32}, // `, a, b, c
    {56,68,68,72,127}, {56,84,84,84,24}, {8,126,9,1,2}, {12,82,82,82,62}, // d, e, f, g
    {127,8,4,4,120}, {0,68,125,64,0}, {32,64,68,61,0}, {127,16,40,68,0}, // h, i, j, k
    {0,65,127,64,0}, {124,4,24,4,120}, {124,8,4,4,120}, {56,68,68,68,56}, // l, m, n, o
    {124,20,20,20,8}, {8,20,20,24,124}, {124,8,4,4,8}, {72,84,84,84,32}, // p, q, r, s
    {4,63,68,64,32}, {60,64,64,32,124}, {28,32,64,32,28}, {60,64,48,64,60}, // t, u, v, w
    {68,40,16,40,68}, {12,80,80,80,60}, {68,100,84,76,68}, {8,54,65,65,0}, // x, y, z, {
    {0,0,119,0,0}, {0,65,65,54,8}, {8,12,6,3,1} // |, }, ~
};

// --- SPI İŞLEMLERİ ---
void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8; t.tx_buffer = &cmd; t.user = (void*)0;
    ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);
}

void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len) {
    if (len==0) return;
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = len * 8; t.tx_buffer = data; t.user = (void*)1;
    ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);
}

void lcd_spi_pre_transfer_callback(spi_transaction_t *t) {
    int dc = (int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}

// --- EKRAN FONKSİYONLARI ---
void lcd_set_window(spi_device_handle_t spi, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // Burada OFSET değerlerini ekliyoruz
    x0 += OFFSET_X; x1 += OFFSET_X;
    y0 += OFFSET_Y; y1 += OFFSET_Y;

    uint8_t col[] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF};
    uint8_t row[] = {y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF};
    lcd_cmd(spi, 0x2A); lcd_data(spi, col, 4);
    lcd_cmd(spi, 0x2B); lcd_data(spi, row, 4);
    lcd_cmd(spi, 0x2C);
}

void lcd_init(spi_device_handle_t spi) {
    gpio_set_level(PIN_NUM_RST, 0); vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_NUM_RST, 1); vTaskDelay(pdMS_TO_TICKS(100));
    lcd_cmd(spi, 0x01); vTaskDelay(pdMS_TO_TICKS(150));
    lcd_cmd(spi, 0x11); vTaskDelay(pdMS_TO_TICKS(255));
    uint8_t color_mode[] = {0x05}; lcd_cmd(spi, 0x3A); lcd_data(spi, color_mode, 1);
    uint8_t madctl[] = {0xC0}; lcd_cmd(spi, 0x36); lcd_data(spi, madctl, 1);
    lcd_cmd(spi, 0x29);
}

void lcd_fill_screen(spi_device_handle_t spi, uint16_t color) {
    lcd_set_window(spi, 0, 0, LCD_WIDTH-1, LCD_HEIGHT-1);
    #define BUF_SIZE 128
    uint16_t buf[BUF_SIZE];
    uint8_t hi = color >> 8, lo = color & 0xFF;
    for(int i=0; i<BUF_SIZE; i++) buf[i] = (hi << 8) | lo;
    for(int i=0; i<LCD_WIDTH*LCD_HEIGHT/BUF_SIZE; i++) lcd_data(spi, (uint8_t*)buf, BUF_SIZE*2);
}

// --- KARAKTER ÇİZME FONKSİYONU ---
void draw_char(spi_device_handle_t spi, int x, int y, char c, uint16_t color, uint16_t bg) {
    if (c < 32 || c > 126) return; // Desteklenmeyen karakter
    const uint8_t *bitmap = simpleFont[c - 32];
    
    // Her karakter 5x7 (5 genişlik, 7 yükseklik)
    // Hız için piksel piksel değil, küçük bir buffer doldurup atıyoruz
    uint16_t buffer[5 * 8]; // 5x8 lik bir alan (biraz boşluk olsun)
    int index = 0;

    // Bitmap'i renkli piksellere çevir
    for(int row=0; row<8; row++) {
        for(int col=0; col<5; col++) {
            // Font yapısı sütun bazlı olduğu için bitleri okuyoruz
            // Bu basit fontta 0. bit en üsttedir
            int pixel_on = (bitmap[col] >> row) & 1;
            uint16_t p_color = pixel_on ? color : bg;
            // Little Endian çevrimi
            buffer[index++] = (p_color << 8) | (p_color >> 8);
        }
    }
    
    // Ekrana bas
    lcd_set_window(spi, x, y, x+4, y+7);
    lcd_data(spi, (uint8_t*)buffer, sizeof(buffer));
}

// --- YAZI YAZMA (OTOMATİK ALT SATIR) ---
void draw_text(spi_device_handle_t spi, int x, int y, const char *str, uint16_t color, uint16_t bg) {
    int cur_x = x;
    int cur_y = y;
    
    while(*str) {
        if(*str == '\n') { // Alt satır karakteri
            cur_y += 10;
            cur_x = x;
        } else {
            // Satır sonuna geldik mi?
            if(cur_x > LCD_WIDTH - 6) {
                cur_y += 10;
                cur_x = x;
            }
            draw_char(spi, cur_x, cur_y, *str, color, bg);
            cur_x += 6; // Bir sonraki harf için sağa kay
        }
        str++;
    }
}

void app_main(void) {
    // 1. EKRANIN DC VE RST PİNLERİNİ AYARLA
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    
    // 2. SPI OTOBANINI (BUS) KUR
    spi_bus_config_t buscfg = {
        .miso_io_num = -1, 
        .mosi_io_num = PIN_NUM_MOSI, 
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1, 
        .quadhd_io_num = -1, 
        .max_transfer_sz = 4096
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 20*1000*1000, 
        .mode = 0, 
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 7, 
        .pre_cb = lcd_spi_pre_transfer_callback,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &devcfg, &spi));

    // 3. EKRANI UYANDIR
    lcd_init(spi);

    // 4. BUTON VE SWITCH PİN AYARLARI
    gpio_config_t btn_config = {
        .pin_bit_mask = (1ULL << BUTTON_NEXT_PIN) | (1ULL << SWITCH_MODE_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,    // Basılmıyorken (boştayken) HIGH okur
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_config);

    // --- DEĞİŞKENLER ---
    uint16_t text_color = 0xFFFF; // Yazı rengi
    uint16_t bg_color = 0x0000;   // Karakterin kendi arka planı
    uint16_t background = 0x0000; // Ekranın genel temizleme rengi
    int page = 0;
    int last_switch_state = -1;   // Değişimi anlamak için önceki durum

    // 5. SONSUZ DÖNGÜ
    while(1) {
        // --- SWITCH (MOD) KONTROLÜ ---
        int current_switch_state = gpio_get_level(SWITCH_MODE_PIN);

        // Eğer switch'in konumu bir önceki döngüden farklıysa (yani çevrildiyse)
        if (current_switch_state != last_switch_state) {
            last_switch_state = current_switch_state;

            // Renkleri switch'e göre belirle
            if (current_switch_state == 0) {
                // KARANLIK MOD
                text_color = 0xFFFF; // Beyaz yazı
                bg_color = 0x0000;   // Siyah arka plan
                background = 0x0000;
            } else {
                // AYDINLIK MOD
                text_color = 0x0000; // Siyah yazı
                bg_color = 0xFFFF;   // Beyaz arka plan
                background = 0xFFFF;
            }

            // MOD DEĞİŞİNCE EKRANI HEMEN GÜNCELLE
            lcd_fill_screen(spi, background);
            if (page == 0) {
                draw_text(spi, 2, 10, "Sayfa 0:\nSiir basliyor...", text_color, bg_color);
            } else if (page == 1) {
                draw_text(spi, 2, 10, "Sayfa 1:\nKURT YEDIGI AYAZI\nUNUTMAZ!", text_color, bg_color);
            }
        }

        // --- BUTON (SAYFA) KONTROLÜ ---
        if (gpio_get_level(BUTTON_NEXT_PIN) == 0) {
            page++;
            if (page > 1) page = 0;

            // SAYFA DEĞİŞİNCE EKRANI GÜNCELLE
            lcd_fill_screen(spi, background);
            if (page == 0) {
                draw_text(spi, 2, 10, "Sayfa 0:\nSiir basliyor...", text_color, bg_color);
            } else if (page == 1) {
                draw_text(spi, 2, 10, "Sayfa 1:\nKURT YEDIGI AYAZI\nUNUTMAZ!", text_color, bg_color);
            }

            // Debounce: Elini çekene kadar bekle/gecikme yap
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        // İşlemciyi çok yormamak için kısa bir mola
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
