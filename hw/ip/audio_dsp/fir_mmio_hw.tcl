package require -exact qsys 15.0

set_module_property NAME fir_mmio
set_module_property VERSION 1.0
set_module_property DISPLAY_NAME "FIR Stereo MMIO"
set_module_property GROUP "Proyecto 2"

# Archivos del modulo
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL fir_mmio

add_fileset_file fir_mmio.sv SYSTEM_VERILOG PATH fir_mmio.sv TOP_LEVEL_FILE
add_fileset_file fir_core.sv SYSTEM_VERILOG PATH fir_core.sv
add_fileset_file fir_coeffs.mem OTHER PATH fir_coeffs.mem

# Reloj
add_interface clock clock end
add_interface_port clock clk clk Input 1

# Reset
add_interface reset reset end
set_interface_property reset associatedClock clock
set_interface_property reset synchronousEdges DEASSERT
add_interface_port reset reset_n reset_n Input 1

# Interfaz Avalon-MM
add_interface s1 avalon end

set_interface_property s1 associatedClock clock
set_interface_property s1 associatedReset reset
set_interface_property s1 addressUnits WORDS
set_interface_property s1 readLatency 0
set_interface_property s1 readWaitTime 0
set_interface_property s1 writeWaitTime 0
set_interface_property s1 timingUnits Cycles

add_interface_port s1 address address Input 2
add_interface_port s1 read read Input 1
add_interface_port s1 write write Input 1
add_interface_port s1 writedata writedata Input 32
add_interface_port s1 readdata readdata Output 32