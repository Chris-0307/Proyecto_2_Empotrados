# Arranque automatico del reproductor

El reproductor inicia automaticamente al encender la DE1-SoC,
sin necesidad de utilizar Quartus o una computadora externa.

## Configuracion utilizada

- DE1-SoC.
- Linux Yocto con kernel 3.12.0.
- MSEL configurado en 0x0A.
- Archivo RBF comprimido en modo FPP.
- Reproductor V4.
- Driver de botones por interrupciones.

## Archivos necesarios en la placa

Los siguientes archivos deben estar en /home/root:

- DE1_SoC_Reproductor.rbf
- player_v4
- buttons.ko
- pistas/ (directorio con los archivos WAV)

## Instalacion del script

Copiar el archivo reproductor a /etc/init.d/ de la placa.

Desde la DE1-SoC ejecutar:

```sh
chmod +x /etc/init.d/reproductor

ln -s ../init.d/reproductor /etc/rcS.d/S39reproductor
ln -s ../init.d/reproductor /etc/rc0.d/K08reproductor
ln -s ../init.d/reproductor /etc/rc6.d/K08reproductor
```

El script deshabilita los puentes HPS-FPGA, configura la FPGA,
comprueba su estado, habilita nuevamente los puentes, carga
el driver de botones e inicia el reproductor.

## Optimizacion del arranque

Se inicia el reproductor antes de la configuracion de red
para evitar la espera del DHCP.

Tambien se deshabilitaron los servicios de demostracion
que no son necesarios para el proyecto:

```sh
mv /etc/rc5.d/S70lighttpd /etc/rc5.d/disabled-S70lighttpd
mv /etc/rc5.d/S90gsrd_init.sh /etc/rc5.d/disabled-S90gsrd_init.sh
```

Se mantiene SSH para facilitar las pruebas y la transferencia
de archivos.

En U-Boot se redujo el tiempo de espera de 5 a 1 segundo:

```sh
setenv bootdelay 1
saveenv
```

## Resultados

| Configuracion | Tiempo de arranque |
|---|---|
| Arranque automatico inicial | 20 segundos |
| Optimizacion de servicios | 10-11 segundos |
| Optimizacion de U-Boot | 7 segundos |

En la ultima prueba se cumple el requisito de inicio
en menos de 10 segundos.

## Verificacion

Desde Linux se puede comprobar el funcionamiento con:

```sh
pidof player_v4
cat /sys/class/fpga/fpga0/status
lsmod
tail -15 /tmp/reproductor.log
```

El estado de la FPGA debe ser "user mode".
