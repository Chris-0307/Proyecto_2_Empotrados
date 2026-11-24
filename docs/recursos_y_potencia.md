# Recursos y potencia del SoC

Resultados de Quartus Prime Lite 25.1std sobre la DE1-SoC (5CSEMA5F31C6),
para el **SoC base** (commit `23db587` de `feat/soc-base`), todavía **sin el
filtro DSP propio**. Esta tabla se vuelve a llenar cuando el filtro se integre,
para comparar.

Cómo regenerar: `cd hw && make all && make power`.

## Utilización de recursos (sistema completo)

| Recurso | Usado | Disponible | % |
|---|---|---|---|
| Lógica (ALMs) | 4 187 | 32 070 | 13 % |
| Registros | 5 455 | — | — |
| Pines | 368 | 457 | 81 % |
| Memoria en bloque (bits) | 2 209 664 | 4 065 280 | 54 % |
| Bloques de RAM (M10K) | 276 | 397 | 70 % |
| Bloques DSP | 0 | 87 | 0 % |
| PLLs | 3 | 6 | 50 % |
| DLLs | 1 | 4 | 25 % |

Fuente: `hw/build/DE1_SoC_Reproductor.fit.summary`.

## Consumo de potencia (PowerPlay Power Analyzer)

| Componente | Potencia |
|---|---|
| Dinámica del núcleo | 368,91 mW |
| Estática del núcleo | 428,20 mW |
| E/S | 107,86 mW |
| **Total** | **904,98 mW** |

Fuente: `hw/build/DE1_SoC_Reproductor.pow.summary`.

**Confianza de la estimación: baja.** Quartus usó la tasa de conmutación por
defecto (12,5 %) porque no se le dio un archivo de actividad (VCD/SAF) de una
simulación del sistema. La cifra sirve para comparar versiones del diseño entre
sí, no como consumo real medido.

## Observaciones para el análisis de la partición HW/SW

- **La memoria es el recurso más cargado.** La mayor parte la ocupa el
  framebuffer de la VGA (`Pixel_Buffer`, 256 KB on-chip, 320×240 a 16 bits). Si
  la interfaz solo muestra texto con el *character buffer*, se puede reducir o
  quitar y liberar cerca de la mitad de la memoria en bloque.
- **Los 87 bloques DSP están libres.** El filtro en HDL los va a usar para sus
  multiplicadores; comparar el antes y el después de integrarlo muestra el costo
  real de mover un filtro de software a hardware.
- **El decodificador de 7 segmentos está en hardware** (`seg7_bcd.v`): el software
  solo escribe MM:SS en BCD (16 bits) y la FPGA hace la conversión a segmentos.
  Su costo en lógica es mínimo.
- **El reloj de audio es de 11,2896 MHz** para trabajar a 44,1 kHz exactos. El
  software configura el registro R8 del WM8731 en `0x20` al iniciar.

## Pendiente

- Reporte de recursos **por entidad** del módulo propio (filtro DSP), con
  *Fitter → Resource Section → Resource Utilization by Entity*, una vez integrado.
- Repetir la estimación de potencia con el filtro integrado.
