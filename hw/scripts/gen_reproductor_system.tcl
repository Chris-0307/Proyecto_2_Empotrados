###############################################################################
# Proyecto 2 - CE-1113 Sistemas Empotrados (TEC)
# SoC del reproductor de audio sobre DE1-SoC (HPS Cortex-A9)
#
# Basado en el DE1-SoC Computer de FPGAcademy (licencia MIT, ver
# LICENSE_FPGAcademy), recortado a solo lo que usa el reproductor:
# se quitaron Nios II, SDRAM, PS/2, IrDA, JTAG UARTs, video-in y GPIO de
# expansion, porque el enunciado no permite componentes sin funcion justificada.
#
# Mapa de memoria (vista desde el HPS):
#   Puente lightweight  (base 0xFF200000):
#     0xFF200000  LEDs             estado de reproduccion (play/pausa/stop)
#     0xFF200020  Time_BCD         MM:SS en BCD -> decodificador HDL -> HEX3..HEX0
#     0xFF200040  Slider_Switches  seleccion de filtro
#     0xFF200050  Pushbuttons      KEY[3:0], IRQ por flanco -> f2h_irq0[1]
#     0xFF202000  Interval_Timer   tick de tiempo, IRQ -> f2h_irq0[0]
#     0xFF202040  SysID            version del bitstream
#     0xFF203000  AV_Config        configuracion I2C del codec WM8731
#     0xFF203010  VGA rgb_slave
#     0xFF203020  Pixel DMA (control, via traductor de direcciones)
#     0xFF203030  Char buffer DMA (control, via traductor de direcciones)
#     0xFF203040  Audio            FIFOs del codec, IRQ -> f2h_irq0[6]
#     0xFF204000  ADC              AD7928 (potenciometro de volumen)
#   Puente HPS-to-FPGA (base 0xC0000000):
#     0xC8000000  Pixel_Buffer     on-chip, 320x240x16 bits para la VGA
#     0xC9000000  Char buffer      80x60 caracteres de texto (metadatos)
#
#     0xFF205000  Audio_FIR        Filtro FIR estereo personalizado
# En Linux, la IRQ n de f2h_irq0 corresponde a la GIC SPI 72+n
# (botones = SPI 73, timer = SPI 72, audio = SPI 78).
###############################################################################

###############################################################################
# Relojes
ifup_add_system_clock_source	100000000
ifup_add_vga_clock_source		25000000
#
# Procesador
ifup_add_arm_a9_hps
#
# Memoria del framebuffer de la VGA (tiene que agregarse antes de la VGA)
ifup_add_onchip_memory	Pixel_Buffer 262144 0x08000000
#
# GPIO
ifup_add_gpio	LEDs			leds			Output	false	RISING  10 0xFF200000 -1
ifup_add_gpio	Time_BCD		time_bcd		Output	false	RISING  16 0xFF200020 -1
ifup_add_gpio	Slider_Switches	slider_switches	Input	false	RISING  10 0xFF200040 -1
ifup_add_gpio	Pushbuttons		pushbuttons		Input	true	FALLING  4 0xFF200050  1
#
# Timer (solo lo ve el HPS)
ifup_add_interval_timer	Interval_Timer	0xFF202000 $ifup_flag_hps_h2f_lw_master 0 $ifup_flag_hps_arm_0_irq_receiver
#
# System ID
ifup_add_system_id
#
# Audio y video
ifup_add_av_config
ifup_add_adc 12.5
ifup_add_dma_address_translation	Pixel_DMA_Addr_Translation	0xFF203020 $ifup_flag_video_out_dma_addr_translator
ifup_add_dma_address_translation	Char_DMA_Addr_Translation	0xFF203030 $ifup_flag_char_buf_dma_addr_translator
ifup_add_vga_subsystem
ifup_add_audio_subsystem	6
# Filtro FIR personalizado
add_instance Audio_FIR fir_mmio 1.0

ifup_connect_system_clock Audio_FIR.clock
ifup_connect_reset_sources Audio_FIR.reset

ifup_connect_slave_to_mm_masters Audio_FIR.s1 0xFF205000 $ifup_flag_hps_h2f_lw_master

lock_avalon_base_address Audio_FIR.s1
###############################################################################


###############################################################################
# Audio a 44.1 kHz: el codec necesita MCLK = 11.2896 MHz para esa familia de
# frecuencias. Los WAV de 8 kHz y 16 kHz se remuestrean en software.
set_instance_parameter_value AV_Config sampling_rate	"48 kHz"
###############################################################################


###############################################################################
# Parametros del HPS propios de la DE1-SoC (DDR3 y pines), sin cambios
set_instance_parameter_value ARM_A9_HPS GPIO_Enable [list No No No No No No No No No Yes No No No No No No No No No No No No No No No No No No No No No No No No No Yes No No No No Yes Yes No No No No No No Yes No No No No Yes Yes No No No No No No Yes No No No No No No No No No No No No No No No No No No No No No No No No No No No No No No No No No No No No No No]
set_instance_parameter_value ARM_A9_HPS MEM_TCL								7
set_instance_parameter_value ARM_A9_HPS MEM_TFAW_NS							45.0
set_instance_parameter_value ARM_A9_HPS MEM_TRAS_NS							36.0
set_instance_parameter_value ARM_A9_HPS MEM_TRCD_NS							13.125
set_instance_parameter_value ARM_A9_HPS MEM_TRFC_NS							300.0
set_instance_parameter_value ARM_A9_HPS MEM_TRP_NS							13.125
set_instance_parameter_value ARM_A9_HPS MEM_VENDOR							"Micron"
set_instance_parameter_value ARM_A9_HPS QSPI_Mode							"1 SS"
set_instance_parameter_value ARM_A9_HPS QSPI_PinMuxing						"HPS I/O Set 0"
set_instance_parameter_value ARM_A9_HPS S2F_Width							3
set_instance_parameter_value ARM_A9_HPS TIMING_BOARD_AC_SKEW				0.02
set_instance_parameter_value ARM_A9_HPS TIMING_BOARD_AC_TO_CK_SKEW			0.01
set_instance_parameter_value ARM_A9_HPS TIMING_BOARD_DQ_TO_DQS_SKEW			0.05
set_instance_parameter_value ARM_A9_HPS TIMING_BOARD_MAX_CK_DELAY			0.03
set_instance_parameter_value ARM_A9_HPS TIMING_BOARD_MAX_DQS_DELAY			0.02
set_instance_parameter_value ARM_A9_HPS TIMING_BOARD_SKEW_BETWEEN_DIMMS		0.05
set_instance_parameter_value ARM_A9_HPS TIMING_BOARD_SKEW_BETWEEN_DQS		0.06
set_instance_parameter_value ARM_A9_HPS TIMING_BOARD_SKEW_CKDQS_DIMM_MAX	0.12
set_instance_parameter_value ARM_A9_HPS TIMING_BOARD_SKEW_CKDQS_DIMM_MIN	0.06
set_instance_parameter_value ARM_A9_HPS TIMING_BOARD_SKEW_WITHIN_DQS		0.01
set_instance_parameter_value ARM_A9_HPS TIMING_TDH							65
set_instance_parameter_value ARM_A9_HPS TIMING_TDQSCK						255
set_instance_parameter_value ARM_A9_HPS TIMING_TDQSQ						125
set_instance_parameter_value ARM_A9_HPS TIMING_TDS							30
set_instance_parameter_value ARM_A9_HPS TIMING_TIH							140
set_instance_parameter_value ARM_A9_HPS TIMING_TIS							190
set_instance_parameter_value ARM_A9_HPS TIMING_TQSH							0.4
###############################################################################
