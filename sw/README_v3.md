# Reproductor V3 (sin hilos)

Usa el driver propio buttons.ko para atender las IRQ de los botones.
El reproductor lee un byte de /dev/fpga_buttons con O_NONBLOCK cada 5 ms
aproximadamente, sin consultar los registros PIO de botones. El driver
maneja esos registros mediante MMIO. Los otros perifericos siguen usando
MMIO desde player.c. Se mantiene el antirrebote de 150 ms por boton.

Compilar con make dentro de sw. Con el SoC programado, cargar buttons.ko
(si no esta cargado) y ejecutar ./player 44100 cancion.wav prueba.wav.
KEY0 play/pausa, KEY1 stop, KEY2 anterior, KEY3 siguiente.
SW0/SW1 seleccionan filtros; pasa-bajos SW provisional hasta integrar FIR.
PLAYER_ADC y PLAYER_VGA habilitan las pruebas opcionales de ADC y VGA.
No ejecutar player V2 ni test_hw mientras este cargado buttons.ko.
Detener player y ejecutar rmmod buttons antes de reprogramar la FPGA.
Los eventos repetidos del mismo boton pueden agruparse en el driver.
V3 probada en DE1-SoC con interrupciones reales (IRQ 73).
Pendiente: FIR de hardware, ADC y VGA.
