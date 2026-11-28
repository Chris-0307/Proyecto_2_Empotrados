# Mediciones de latencia

Se utilizó la versión instrumentada `player_latency.c` y diez WAV de prueba de 3 segundos. Los tiempos se obtuvieron con `clock_gettime(CLOCK_MONOTONIC)` y se imprimieron en milisegundos. Para botones se mide desde el instante previo a la consulta que detecta el evento del driver hasta la actualización de estado/indicadores. Para cambio de filtro se mide desde la lectura de switches que detecta el cambio hasta la escritura del primer frame procesado a la FIFO. Para cambio de pista se mide desde la detección del botón mientras se reproduce hasta el primer frame de la nueva pista enviado a la FIFO.

Los resultados de botones combinan los dos logs; no se eliminaron valores altos.

| Evento | N | Mínimo (ms) | Promedio (ms) | Máximo (ms) |
|---|---:|---:|---:|---:|
| KEY0 | 11 | 0.020 | 0.022 | 0.029 |
| KEY1 | 10 | 0.138 | 0.144 | 0.158 |
| KEY2 | 20 | 0.145 | 0.473 | 1.526 |
| KEY3 | 20 | 0.138 | 0.165 | 0.194 |
| Bypass | 24 | 0.014 | 0.021 | 0.030 |
| FIR | 16 | 0.017 | 0.027 | 0.034 |
| Pasa-altos | 18 | 0.021 | 0.025 | 0.030 |
| Eco | 11 | 0.015 | 0.019 | 0.021 |
| Cambio de pista | 20 | 0.185 | 0.554 | 1.786 |

Total: 150 registros. Los valores de KEY2 en la segunda prueba son mayores que en la primera, por lo que se conservaron ambos conjuntos. El log de cambio de pista contiene 10 cambios hacia atrás y 10 hacia adelante.

Las latencias internas son menores a los límites de 200 ms (botones), 100 ms (filtro) y 2000 ms (pista). No incluyen toda la espera desde la pulsación física/IRQ, la cola de audio y el codec hasta el sonido analógico. Tampoco son pruebas repetidas de la configuración final con VGA y ADC activos. Se requiere una prueba extremo a extremo para verificar completamente estos límites.

Archivos: [latencias.txt](evidencias/latencias.txt), [latencias_pistas.txt](evidencias/latencias_pistas.txt).
