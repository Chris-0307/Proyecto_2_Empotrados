import math
import struct
import wave

# Tres notas cortas, WAV de 8 kHz para comprobar el remuestreo.
with wave.open('prueba.wav', 'wb') as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(8000)
    for f in [440, 550, 660]:
        samples = [int(10000 * math.sin(2 * math.pi * f * i / 8000)) for i in range(8000)]
        w.writeframes(struct.pack('<8000h', *samples))
