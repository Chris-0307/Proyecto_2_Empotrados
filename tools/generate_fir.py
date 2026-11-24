
import math
from pathlib import Path

# Parámetros provisionales del filtro FIR
FS = 44100        # Frecuencia de muestreo (Hz)
FC = 3000         # Frecuencia de corte (Hz)
NUM_TAPS = 31     # Número de coeficientes
Q = 15            # Formato de punto fijo Q1.15

coeffs = []
middle = (NUM_TAPS - 1) / 2

# Calcular coeficientes mediante sinc y ventana Hamming
for n in range(NUM_TAPS):
    k = n - middle

    if k == 0:
        h = 2 * FC / FS
    else:
        h = math.sin(2 * math.pi * FC * k / FS) / (math.pi * k)

    window = 0.54 - 0.46 * math.cos(
        2 * math.pi * n / (NUM_TAPS - 1)
    )

    coeffs.append(h * window)

# Normalizar ganancia de corriente continua a 1
total = sum(coeffs)
coeffs = [c / total for c in coeffs]

# Convertir a enteros con signo Q1.15
fixed = [round(c * (1 << Q)) for c in coeffs]

# Corregir error de redondeo de la suma
fixed[NUM_TAPS // 2] += (1 << Q) - sum(fixed)

assert all(-32768 <= c <= 32767 for c in fixed)
assert sum(fixed) == 32768

# Generar archivo de memoria para SystemVerilog
repo_root = Path(__file__).resolve().parent.parent
output = repo_root / "hw/ip/audio_dsp/fir_coeffs.mem"
output.parent.mkdir(parents=True, exist_ok=True)

with output.open("w") as f:
    for c in fixed:
        f.write(f"{c & 0xFFFF:04X}\n")

print(f"Generados {len(fixed)} coeficientes")
print(f"Archivo: {output}")
print(f"Suma Q1.15: {sum(fixed)}")
