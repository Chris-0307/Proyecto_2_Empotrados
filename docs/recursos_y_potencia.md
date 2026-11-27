# Recursos y potencia del SoC

Quartus Prime Lite 25.1std, Cyclone V 5CSEMA5F31C6. Se compara el SoC base (reporte histórico del commit 23db587) con el integrado del 27/11/2026.

| Recurso | Base | Integrado | Disponible |
|---|---:|---:|---:|
| ALMs | 4187 | 4756 (15%) | 32070 |
| Registros | 5455 | 7021 | — |
| Memoria embebida (bits) | 2209664 | 2209664 (54%) | 4065280 |
| RAM blocks | 276 | 276 (70%) | 397 |
| DSP | 0 | 2 (2%) | 87 |
| Pines | 368 | 368 (81%) | 457 |
| PLL | 3 | 3 (50%) | 6 |
| DLL | 1 | 1 (25%) | 4 |

La diferencia entre compilaciones incluye cambios de integración y optimización del Fitter, por lo que no equivale exactamente al costo aislado del FIR.

## Hardware personalizado (Resource Utilization by Entity)

| Entidad | ALMs needed | Registros | Bits RAM | DSP |
|---|---:|---:|---:|---:|
| FIR izquierdo | 187.5 | 667 | 0 | 1 |
| FIR derecho | 188.5 | 658 | 0 | 1 |
| Control MMIO (sin submódulos) | 10.4 | 1 | 0 | 0 |
| FIR estéreo total | 386.4 | 1326 | 0 | 2 |

El filtro utiliza aproximadamente 8.1% de las ALMs del sistema integrado. Se reutiliza un multiplicador por canal durante los 31 taps en vez de implementar 31 multiplicadores en paralelo. El framebuffer de video explica una parte importante de la memoria ocupada (256 KiB reservados); se utiliza como fondo para la composición con caracteres.

## PowerPlay

| Componente | Base (mW) | Integrado (mW) |
|---|---:|---:|
| Núcleo dinámico | 368.91 | 376.08 |
| Núcleo estático | 428.20 | 428.25 |
| I/O | 107.86 | 107.61 |
| Total reportado | 904.98 | 911.94 |

La diferencia estimada del sistema es de 6.96 mW. PowerPlay indicó confianza baja por falta de información de actividad; no se trata de una medición eléctrica ni de una comparación aislada entre FIR en hardware y FIR en software. La estimación no incluye necesariamente todo el consumo de la placa.

## Timing Analyzer

| Comprobación | Peor slack reportado (ns) |
|---|---:|
| Setup System_PLL | +0.030 |
| Setup VGA_PLL interno | +35.295 |
| Setup clk_vga | -8.945 |
| Hold | +0.077 |
| Recovery | +2.398 |
| Removal | +0.460 |
| Minimum pulse width | +0.471 |

El reloj del sistema cumple los chequeos de setup reportados en las esquinas analizadas. La salida asociada a `clk_vga` presenta una violación, con TNS de -219.069 ns en la peor esquina. No se considera cerrado el timing de todo el SoC. Se conservó el bitstream probado y queda pendiente validar la imagen física y revisar la relación entre el reloj generado por el PLL y las restricciones de salida VGA si se requiere una corrección.

Regeneración de reportes: `cd hw && make all && make power`. Archivos `.fit.summary`, `.fit.rpt`, `.pow.summary`, `.sta.summary` en `hw/build/`.
