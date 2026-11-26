"""Enviar un archivo por la consola UART de la DE1-SoC. Ejecutar en Ubuntu.
Cerrar screen antes. La consola debe estar iniciada como root.
"""
import base64
import gzip
import pathlib
import re
import shlex
import sys
import time
import serial

if len(sys.argv) != 4:
    sys.exit('Uso: send_serial.py /dev/ttyUSB0 archivo_local /home/root/archivo')
port, local, remote = sys.argv[1:]
if not re.fullmatch(r'/home/root/[A-Za-z0-9_./-]+', remote) or '..' in remote.split('/'):
    sys.exit('Usar una ruta sencilla dentro de /home/root')
raw = pathlib.Path(local).read_bytes()
q = shlex.quote(remote)
tmp = shlex.quote(remote + '.upload')
seq = 0
with serial.Serial(port, 115200, timeout=0.2, write_timeout=5) as ser:
    def cmd(command, timeout=10):
        global seq
        seq += 1
        marker = f'__XFER_{seq:06d}__'
        line = command + "; r=$?; printf '\\n" + marker + ":%s\\n' \"$r\"\r"
        ser.write(line.encode('ascii'))
        ser.flush()
        data = b''
        deadline = time.monotonic() + timeout
        pattern = re.compile(rb'(?:^|[\r\n])' + marker.encode() + rb':([0-9]+)[\r\n]')
        while time.monotonic() < deadline:
            data += ser.read(ser.in_waiting or 1)
            m = pattern.search(data)
            if m:
                return int(m.group(1)), data.decode(errors='replace')
        raise RuntimeError('No hubo respuesta de la consola: ' + data[-500:].decode(errors='replace'))
    ser.write(b'\r')
    time.sleep(0.5)
    boot = ser.read(ser.in_waiting or 1)
    if b'login:' in boot:
        ser.write(b'root\r'); time.sleep(0.5)
    status, _ = cmd('true')
    if status: sys.exit('No se pudo abrir la consola')
    b64, _ = cmd('command -v base64 >/dev/null 2>&1')
    gz, _ = cmd('command -v gzip >/dev/null 2>&1')
    packed = gzip.compress(raw) if gz == 0 else raw
    if len(packed) >= len(raw): packed = raw
    compressed = packed is not raw
    def check(command, timeout=10):
        status, output = cmd(command, timeout)
        if status: raise RuntimeError(output)
        return output
    check('mkdir -p ' + shlex.quote(str(pathlib.PurePosixPath(remote).parent)))
    check(': > ' + tmp)
    print(f'Enviando {len(packed)} bytes (original {len(raw)})', flush=True)
    chunk = 600 if b64 == 0 else 200
    for i in range(0,len(packed),chunk):
        part = packed[i:i+chunk]
        if b64 == 0:
            encoded = base64.b64encode(part).decode()
            check(f"printf '%s' '{encoded}' | base64 -d >> {tmp}")
        else:
            encoded = ''.join(f'\\0{v:03o}' for v in part)
            check(f"printf '%b' '{encoded}' >> {tmp}")
        if i % (chunk*50) == 0: print(f'{i}/{len(packed)}', flush=True)
    check(f'test "$(wc -c < {tmp})" -eq {len(packed)}')
    if compressed:
        check(f'gzip -dc {tmp} > {q}.new && test "$(wc -c < {q}.new)" -eq {len(raw)} && mv {q}.new {q} && rm {tmp}',30)
    else:
        check(f'mv {tmp} {q}')
    # Comparar contenido si la placa tiene md5sum (BusyBox normalmente lo incluye).
    import hashlib
    status, output = cmd('command -v md5sum >/dev/null 2>&1')
    if status == 0:
        expected = hashlib.md5(raw).hexdigest()
        check(f'test "$(md5sum {q} | cut -d " " -f 1)" = {expected}')
    print('Archivo recibido:', remote)
