# Genera y compila el SoC del reproductor (Quartus Prime Lite 25.1std, Linux).
#
#   make check     revisa que Quartus y los IP del University Program esten
#   make qsys      crea los .qsys con los scripts Tcl de scripts/
#   make generate  genera el HDL del sistema con Platform Designer
#   make compile   compila el proyecto de Quartus -> .sof
#   make rbf       convierte el .sof a .rbf (para que U-Boot cargue la FPGA)
#   make header    genera ../sw/include/hw_addrs.h con las direcciones
#   make all       todo lo anterior en orden
#   make program   programa la placa por JTAG (placa conectada)
#   make clean     borra todo lo generado

QUARTUS_ROOTDIR ?= $(HOME)/altera_lite/25.1std/quartus
QBIN  := $(QUARTUS_ROOTDIR)/bin
SOPC  := $(QUARTUS_ROOTDIR)/sopc_builder/bin
UPIP  := $(QUARTUS_ROOTDIR)/../ip/altera/university_program

NAME  := DE1_SoC_Reproductor
BUILD := build
LOGS  := $(BUILD)/logs

SUBSYS := gen_audio_subsystem gen_char_buf_subsystem gen_vga_subsystem

.PHONY: all check qsys generate compile rbf header program clean

all: check qsys generate compile rbf header
	@echo ""
	@echo "Listo: $(BUILD)/$(NAME).sof, $(BUILD)/$(NAME).rbf y ../sw/include/hw_addrs.h"

check:
	@test -x $(QBIN)/quartus_sh || (echo "ERROR: no encuentro Quartus en $(QUARTUS_ROOTDIR)"; exit 1)
	@test -x $(SOPC)/qsys-script || (echo "ERROR: no encuentro qsys-script en $(SOPC)"; exit 1)
	@test -d $(UPIP) || (echo "ERROR: no encuentro los IP del University Program en $(UPIP)"; exit 1)
	@$(QBIN)/quartus_sh --version | head -2
	@echo "Frecuencias que acepta el Audio PLL en esta instalacion:"
	@grep -rh "audio_clk_freq" -A3 $(UPIP) --include="*audio_pll*hw.tcl" | grep -iE "allowed|11.2896|12.288" | head -5 || true
	@echo "OK"

qsys:
	@mkdir -p $(BUILD) $(LOGS)
	@cp scripts/*.tcl $(BUILD)/
	@for s in $(SUBSYS) gen_computer_system; do \
		echo "qsys-script $$s.tcl"; \
		(cd $(BUILD) && $(SOPC)/qsys-script --script=$$s.tcl > logs/$$s.txt 2>&1) || \
			(echo "ERROR en $$s.tcl, revisa $(LOGS)/$$s.txt"; tail -20 $(LOGS)/$$s.txt; exit 1); \
	done
	@grep -h "Error" $(LOGS)/gen_*.txt && exit 1 || echo "Sistemas .qsys creados en $(BUILD)/"

generate:
	@echo "qsys-generate Computer_System.qsys (tarda unos minutos)"
	@cd $(BUILD) && $(SOPC)/qsys-generate Computer_System.qsys --synthesis=VERILOG \
		> logs/qsys-generate.txt 2>&1 || (tail -30 logs/qsys-generate.txt; exit 1)
	@echo "HDL generado en $(BUILD)/Computer_System/synthesis/"

compile:
	@cp src/* $(BUILD)/
	@echo "quartus_sh --flow compile $(NAME) (tarda de 10 a 20 minutos)"
	@cd $(BUILD) && $(QBIN)/quartus_sh --flow compile $(NAME) \
		> logs/compile.txt 2>&1 || (grep -E "^Error|Error \(" logs/compile.txt | head -20; exit 1)
	@grep -E "Total logic elements|Logic utilization|Total registers|Total block memory|Total DSP" \
		$(BUILD)/$(NAME).fit.summary 2>/dev/null || true

rbf:
	@cd $(BUILD) && $(QBIN)/quartus_cpf -m FPP -o bitstream_compression=on \
		-c $(NAME).sof $(NAME).rbf > logs/rbf.txt 2>&1
	@echo "$(BUILD)/$(NAME).rbf generado"

header:
	@mkdir -p ../sw/include
	@cd $(BUILD) && $(SOPC)/sopc-create-header-files Computer_System.sopcinfo \
		--single hw_addrs.h --module ARM_A9_HPS > logs/header.txt 2>&1
	@cp $(BUILD)/hw_addrs.h ../sw/include/hw_addrs.h
	@echo "../sw/include/hw_addrs.h generado"

# En la DE1-SoC la FPGA es el dispositivo 2 de la cadena JTAG (el 1 es el HPS).
program:
	$(QBIN)/jtagconfig
	$(QBIN)/quartus_pgm -m jtag -o "p;$(BUILD)/$(NAME).sof@2"

clean:
	rm -rf $(BUILD)

power:
	@cd $(BUILD) && $(QBIN)/quartus_pow $(NAME) > logs/power.txt 2>&1 || (tail -20 $(BUILD)/logs/power.txt; exit 1)
	@cat $(BUILD)/$(NAME).pow.summary
