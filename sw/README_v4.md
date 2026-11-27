# Reproductor V4

Usar el RBF/SOF integrado con Audio_FIR en 0xFF205000. El driver `buttons.ko` debe cargarse después de programar la FPGA. No reprogramar con el reproductor o el driver activos.

En Ubuntu, desde `sw`:

```sh
arm-linux-gnueabihf-gcc -Wall -O2 -static player.c -o player_v4 -lm
```

En la placa, con FPGA y driver ya configurados y sin otro reproductor activo:

```sh
cd /home/root
PLAYER_ADC=1 PLAYER_VGA=1 ./player_v4 44100 canciones/*.wav
```

El script automático utiliza esa misma configuración. SW1:SW0 selecciona 00 bypass, 01 FIR hardware, 10 pasa-altos software, 11 eco software. KEY0 play/pausa, KEY1 stop, KEY2 anterior, KEY3 siguiente.

Se remuestrean los WAV PCM de 8, 16 y 44.1 kHz a 44.1 kHz. Se leen metadatos RIFF LIST/INFO y el volumen se obtiene de ADC_IN0. La salida VGA usa memoria de caracteres de 80x60 con stride 128. El FIR se reinicia al seleccionarlo; se espera STATUS bit 0 antes de escribir y bit 1 antes de leer. Ante timeout se termina con error, no se sustituye el FIR por software.

La reproducción física, los botones, filtros, volumen y escritura en memoria de video se probaron en la DE1-SoC. Quedan pendientes la prueba visual VGA, las latencias extremo a extremo y la estabilidad prolongada. Ver [resultados](../docs/latencias.md).
