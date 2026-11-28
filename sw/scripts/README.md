# Arranque automático

Se utilizó DE1-SoC con Linux Yocto, kernel 3.12.0-00307-g507abb4-dirty, MSEL 0x0A y archivo RBF comprimido FPP. Verificar el modo MSEL de la revisión de placa antes de cambiar switches; hacer cambios físicos sin alimentación.

En `/home/root` se necesitan `DE1_SoC_Reproductor.rbf`, `buttons.ko`, `player_v4` y `canciones/*.wav`. El script final habilita `PLAYER_ADC=1` y `PLAYER_VGA=1`.

Copiar `reproductor` a `/etc/init.d/reproductor`. En la placa, desde Picocom o SSH, instalar los enlaces si aún no existen:

```sh
chmod +x /etc/init.d/reproductor
ln -s ../init.d/reproductor /etc/rcS.d/S39reproductor
ln -s ../init.d/reproductor /etc/rc0.d/K08reproductor
ln -s ../init.d/reproductor /etc/rc6.d/K08reproductor
```

Eliminar solamente enlaces de arranque anteriores del mismo reproductor si quedaron duplicados. El script verifica archivos, detiene el uso del driver, deshabilita los tres puentes, configura la FPGA, comprueba `user mode`, habilita los puentes, carga el driver y ejecuta el reproductor en segundo plano. No iniciarlo si existe otra prueba que accede a MMIO (por ejemplo player_latency o test_hw).

En la SD del proyecto se adelantó el inicio a rcS antes de DHCP, y se deshabilitaron los servicios de demostración lighttpd y gsrd_init.sh en rc5. Se dejó SSH habilitado. En U-Boot se cambió bootdelay de 5 a 1 segundo (`setenv bootdelay 1`, `saveenv`). Estos cambios ya están hechos en la tarjeta preparada.

| Versión | Tiempo aproximado |
|---|---:|
| Inicial | 20 s |
| Servicios optimizados | 10-11 s |
| U-Boot optimizado | 7 s |
| Canciones + ADC + VGA | 8 s |

Verificación en la placa:

```sh
pidof player_v4
cat /sys/class/fpga/fpga0/status
lsmod
tail -15 /tmp/reproductor.log
./leer_vga
```

La última instrucción requiere haber compilado y copiado `sw/tests/leer_vga.c`. Se verificó que tras reiniciar aparecen los metadatos en la memoria VGA. La imagen física aún debe comprobarse con un monitor.

Para detener la reproducción automática: `/etc/init.d/reproductor stop`. Comprobar que `pidof player_v4` no devuelve PID antes de ejecutar otra instancia. Para reiniciar usar el script `start`, únicamente sin otros procesos de prueba accediendo a FPGA.
