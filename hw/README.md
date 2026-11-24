# Hardware del reproductor (DE1-SoC, Quartus Prime Lite 25.1std)

El sistema de Platform Designer **no se edita a mano**: se genera con los
scripts Tcl de `scripts/`. Así todos generan exactamente el mismo sistema y
las direcciones no cambian sin que se note en el repo.

## Compilar

```bash
cd hw
make all
```

Esto crea `build/DE1_SoC_Reproductor.sof`, `build/DE1_SoC_Reproductor.rbf` y
`../sw/include/hw_addrs.h`. Tarda de 15 a 25 minutos. Si algo falla, el log
está en `build/logs/`.

Para programar la placa por JTAG: `make program`.

## Qué tiene el sistema

| Dirección (HPS) | Componente | Uso |
|---|---|---|
| 0xFF200000 | LEDs | Estado: play / pausa / stop |
| 0xFF200020 | Time_BCD (16 bits) | MM:SS en BCD → `seg7_bcd.v` → HEX3..HEX0 |
| 0xFF200040 | Slider_Switches | Selección de filtro |
| 0xFF200050 | Pushbuttons | KEY[3:0], IRQ por flanco (f2h_irq0[1], GIC SPI 73) |
| 0xFF202000 | Interval_Timer | Tick de tiempo (f2h_irq0[0], GIC SPI 72) |
| 0xFF202040 | SysID | Versión del bitstream |
| 0xFF203000 | AV_Config | Configuración del codec WM8731 (44.1 kHz) |
| 0xFF203010 | VGA rgb_slave | |
| 0xFF203020 | Pixel DMA (control) | |
| 0xFF203030 | Char buffer DMA (control) | |
| 0xFF203040 | Audio | FIFOs de salida (f2h_irq0[6], GIC SPI 78) |
| 0xFF204000 | ADC | AD7928, potenciómetro de volumen |
| 0xC8000000 | Pixel_Buffer | Framebuffer VGA 320x240, 16 bits |
| 0xC9000000 | Char buffer | Texto 80x60 (metadatos) |

## Pendiente

- Integrar el filtro DSP propio (Navarro) como componente Avalon-MM en
  `scripts/gen_reproductor_system.tcl`.
- Los WAV de 8 y 16 kHz se remuestrean a 44.1 kHz en software.

Basado en el DE1-SoC Computer de FPGAcademy (licencia MIT,
`scripts/LICENSE_FPGAcademy`), recortado a lo que usa el reproductor.
