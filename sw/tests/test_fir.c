#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define FIR_BASE 0xFF205000

#define CONTROL  0
#define STATUS   1
#define DATA_IN  2
#define DATA_OUT 3

volatile uint32_t *fir;

/* Esperar un bit del registro STATUS */
void esperar(uint32_t mascara)
{
    int i;

    for (i = 0; i < 1000000; i++) {
        if (fir[STATUS] & mascara)
            return;
    }

    printf("ERROR: timeout, STATUS = 0x%X\n", fir[STATUS]);
    exit(1);
}

/* Enviar una muestra estereo y leer el resultado */
uint32_t procesar(int16_t izq, int16_t der)
{
    esperar(0x01);

    fir[DATA_IN] = ((uint32_t)(uint16_t)der << 16)
                   | (uint16_t)izq;

    esperar(0x02);

    return fir[DATA_OUT];
}

int main(void)
{
    int fd;
    int i;
    int errores = 0;
    uint32_t resultado;
    int16_t izq, der;
    int esperado_izq, esperado_der;

    int coef[31] = {
         7, -20, -64, -133, -219,
        -292, -298, -175, 130, 644,
        1350, 2180, 3028, 3762, 4262,
        4444,
        4262, 3762, 3028, 2180, 1350,
        644, 130, -175, -298, -292,
        -219, -133, -64, -20, 7
    };

    fd = open("/dev/mem", O_RDWR | O_SYNC);

    if (fd < 0) {
        perror("open");
        return 1;
    }

    fir = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
               MAP_SHARED, fd, FIR_BASE);

    if (fir == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    printf("FIR base: 0x%X\n", FIR_BASE);

    /* Reiniciar el historial */
    fir[CONTROL] = 1;

    esperar(0x01);

    printf("STATUS inicial: 0x%X\n", fir[STATUS]);

    printf("\nPrueba de respuesta al impulso\n");
    printf("Muestra | Izquierda | Derecha | Estado\n");

    for (i = 0; i < 32; i++) {

        if (i == 0)
            resultado = procesar(32767, -32768);
        else
            resultado = procesar(0, 0);

        izq = (int16_t)(resultado & 0xFFFF);
        der = (int16_t)(resultado >> 16);

        if (i < 31) {
            esperado_izq = coef[i];

            if (coef[i] > 0)
                esperado_izq--;

            esperado_der = -coef[i];
        } else {
            esperado_izq = 0;
            esperado_der = 0;
        }

        if (izq == esperado_izq && der == esperado_der) {
            printf("%7d | %9d | %7d | PASS\n",
                   i, izq, der);
        } else {
            printf("%7d | %9d | %7d | FAIL\n",
                   i, izq, der);

            errores++;
        }
    }

    /* Comprobar que CONTROL limpia el historial */
    procesar(32767, -32768);

    fir[CONTROL] = 1;

    resultado = procesar(0, 0);

    if (resultado == 0) {
        printf("\nPASS: reinicio del filtro\n");
    } else {
        printf("\nFAIL: reinicio del filtro\n");
        errores++;
    }

    printf("Errores totales: %d\n", errores);

    munmap((void *)fir, 4096);
    close(fd);

    return errores != 0;
}