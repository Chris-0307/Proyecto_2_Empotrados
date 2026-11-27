obj-m += test_module.o
obj-m += buttons.o

KDIR = $(HOME)/empotrados/kernel-de1/linux-socfpga-507abb469931ebf60cc12b48dbd13611c11d2af3

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) ARCH=arm modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean