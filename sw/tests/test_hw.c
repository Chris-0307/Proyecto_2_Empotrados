/*
 * test_hw.c - Prueba rapida del SoC base del reproductor (DE1-SoC, HPS + Linux)
 *
 * Prueba, en orden: SysID, 7 segmentos (MM:SS), LEDs, switches, ADC,
 * botones (por edge capture, sin IRQ), audio (tono de 2 s) y VGA (texto).
 *
 * Compilar en la PC:
 *   arm-linux-gnueabihf-gcc -O2 -static -o test_hw test_hw.c
 * Correr en la placa (como root):
 *   ./test_hw
 */
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/* Puente lightweight: perifericos de la FPGA */
#define LW_BASE        0xFF200000u
#define LW_SPAN        0x00010000u
#define LEDS           0x0000
#define TIME_BCD       0x0020
#define SWITCHES       0x0040
#define KEYS           0x0050
#define SYSID          0x2040
#define AUDIO          0x3040
#define ADC            0x4000

/* Puente HPS-to-FPGA: memorias de video */
#define H2F_PIXEL      0xC8000000u
#define PIXEL_SPAN     0x00040000u
#define H2F_CHAR       0xC9000000u
#define CHAR_SPAN      0x00002000u

/* Registros del PIO */
#define PIO_DATA       0x00
#define PIO_EDGE       0x0C

/* Registros del Audio Core (University Program) */
#define AUD_CONTROL    0x00
#define AUD_FIFOSPACE  0x04
#define AUD_LEFT       0x08
#define AUD_RIGHT      0x0C

static volatile uint8_t *lw;

static inline uint32_t rd(uint32_t off)             { return *(volatile uint32_t *)(lw + off); }
static inline void     wr(uint32_t off, uint32_t v) { *(volatile uint32_t *)(lw + off) = v; }

static void *map(int fd, uint32_t base, uint32_t span)
{
    void *p = mmap(NULL, span, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base);
    if (p == MAP_FAILED) { perror("mmap"); exit(1); }
    return p;
}

static void sleep_ms(int ms)
{
    struct timespec t = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&t, NULL);
}

static uint32_t to_bcd(int mm, int ss)
{
    return ((mm / 10) << 12) | ((mm % 10) << 8) | ((ss / 10) << 4) | (ss % 10);
}

int main(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem (correr como root)"); return 1; }
    lw = map(fd, LW_BASE, LW_SPAN);

    /* 1. SysID */
    printf("[1] SysID = 0x%08X (si es 0xFFFFFFFF o 0, el puente no responde)\n", rd(SYSID));

    /* 2. 7 segmentos: 12:34 y luego cuenta 5 segundos */
    printf("[2] 7 segmentos: deberia verse 12:34 y luego contar 00:00 -> 00:05\n");
    wr(TIME_BCD, to_bcd(12, 34));
    sleep_ms(1500);
    for (int s = 0; s <= 5; s++) { wr(TIME_BCD, to_bcd(0, s)); sleep_ms(1000); }

    /* 3. LEDs: barrido */
    printf("[3] LEDs: barrido de izquierda a derecha\n");
    for (int i = 0; i < 10; i++) { wr(LEDS, 1u << (9 - i)); sleep_ms(100); }
    wr(LEDS, 0);

    /* 4. Switches */
    printf("[4] Switches = 0x%03X (mueva algunos y compare)\n", rd(SWITCHES) & 0x3FF);

    /* 5. ADC: activar auto-update y leer los 8 canales */
    wr(ADC + 4, 1);
    sleep_ms(50);
    printf("[5] ADC (0..4095), gire el potenciometro y vuelva a correr:\n    ");
    for (int ch = 0; ch < 8; ch++) printf("ch%d=%4u ", ch, rd(ADC + 4 * ch) & 0xFFF);
    printf("\n");

    /* 6. Botones: 5 segundos para presionar KEY0..KEY3 */
    printf("[6] Botones: presione KEY0..KEY3 durante 5 segundos\n");
    wr(KEYS + PIO_EDGE, 0xF);
    for (int i = 0; i < 50; i++) {
        uint32_t e = rd(KEYS + PIO_EDGE) & 0xF;
        if (e) { printf("    flanco detectado en KEY mascara 0x%X\n", e); wr(KEYS + PIO_EDGE, e); }
        sleep_ms(100);
    }

    /* 7. Audio: tono de 440 Hz por 2 segundos */
    printf("[7] Audio: tono de 440 Hz por 2 s en la salida LINE OUT (verde)\n");
    wr(AUDIO + AUD_CONTROL, 0x8);   /* limpiar FIFOs de salida */
    wr(AUDIO + AUD_CONTROL, 0x0);
    const double fs = 44100.0, f = 440.0;
    int n = (int)(2 * fs);
    for (int i = 0; i < n; ) {
        uint32_t space = rd(AUDIO + AUD_FIFOSPACE);
        int wsrc = (space >> 16) & 0xFF, wslc = (space >> 24) & 0xFF;
        int k = wsrc < wslc ? wsrc : wslc;
        for (; k > 0 && i < n; k--, i++) {
            int32_t s = (int32_t)(0.3 * 32767.0 * sin(2.0 * M_PI * f * i / fs));
            uint32_t v = (uint32_t)(s << 16);
            wr(AUDIO + AUD_LEFT, v);
            wr(AUDIO + AUD_RIGHT, v);
        }
    }

    /* 8. VGA: fondo negro y texto */
    printf("[8] VGA: pantalla negra con texto\n");
    volatile uint16_t *pix = map(fd, H2F_PIXEL, PIXEL_SPAN);
    for (uint32_t i = 0; i < PIXEL_SPAN / 2; i++) pix[i] = 0;
    volatile uint8_t *chr = map(fd, H2F_CHAR, CHAR_SPAN);
    for (uint32_t i = 0; i < CHAR_SPAN; i++) chr[i] = ' ';
    const char *l1 = "PROYECTO 2 - REPRODUCTOR DE AUDIO";
    const char *l2 = "SoC base OK";
    for (size_t x = 0; x < strlen(l1); x++) chr[(2 << 7) + 2 + x] = l1[x];
    for (size_t x = 0; x < strlen(l2); x++) chr[(4 << 7) + 2 + x] = l2[x];

    wr(TIME_BCD, to_bcd(0, 0));
    printf("Prueba terminada.\n");
    close(fd);
    return 0;
}
