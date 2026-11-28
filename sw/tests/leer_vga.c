/* Leer los textos del reproductor sin modificar la memoria VGA. */
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

int main(void)
{
    int fd = open("/dev/mem", O_RDONLY | O_SYNC);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    volatile uint8_t *vga = mmap(NULL, 0x2000, PROT_READ,
                                 MAP_SHARED, fd, (off_t)0xC9000000u);
    if (vga == MAP_FAILED) {
        perror("mmap VGA");
        close(fd);
        return 1;
    }

    int filas[] = {2, 5, 7, 8, 9, 12, 14, 15, 16, 20};
    for (int i = 0; i < 10; i++) {
        int fila = filas[i];
        char texto[81];
        for (int j = 0; j < 80; j++) {
            unsigned char c = vga[fila * 128 + j];
            texto[j] = (c >= 32 && c <= 126) ? c : '.';
        }
        int fin = 80;
        while (fin > 0 && texto[fin-1] == ' ') fin--;
        texto[fin] = '\0';
        printf("Fila %2d: %s\n", fila, texto);
    }
    munmap((void *)vga, 0x2000);
    close(fd);
    return 0;
}
