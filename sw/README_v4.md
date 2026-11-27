# Reproductor V4

Usar solamente con el SOF que contiene Audio_FIR en 0xFF205000.
El driver buttons.ko debe estar cargado despues de programar la FPGA.
No reprogramar mientras el reproductor o driver estan activos.

Compilar con `make` en sw y copiar el ejecutable al HPS.
Ejecutar `./player 44100 cancion.wav prueba.wav`.

SW1:SW0: 00 bypass, 01 pasa-bajos FIR hardware, 10 pasa-altos software, 11 eco software.
FIR MMIO: CONTROL 0x5000, STATUS 0x5004, DATA_IN 0x5008, DATA_OUT 0x500C (offsets LW).
Se reinicia el historial al seleccionar FIR, se espera STATUS bit 0, se escribe el par de muestras,
se espera bit 1 y se lee el resultado. Izquierdo bits 15:0, derecho bits 31:16.
Ante timeout termina la reproduccion con error; no usa un filtro software de respaldo.
Los WAV de 8, 16 y 44.1 kHz se remuestrean a 44.1 kHz. ADC y VGA siguen opcionales (PLAYER_ADC, PLAYER_VGA).

Pendiente: verificar reproduccion fisica de V4, latencias y pruebas completas.
