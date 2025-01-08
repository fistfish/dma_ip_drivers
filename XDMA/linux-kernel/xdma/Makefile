SHELL = /bin/bash
#
# optional makefile parameters:
# - DEBUG=<0|1>,	enable verbose debug print-out in the driver
# - config_bar_num=,	xdma pci config bar number
# - xvc_bar_num=,	xvc pci bar #
# - xvc_bar_offset=,	xvc register base offset
# 
ifneq ($(xvc_bar_num),)
	XVC_FLAGS += -D__XVC_BAR_NUM__=$(xvc_bar_num)
endif

ifneq ($(xvc_bar_offset),)
	XVC_FLAGS += -D__XVC_BAR_OFFSET__=$(xvc_bar_offset)
endif

$(warning XVC_FLAGS: $(XVC_FLAGS).)

topdir := $(shell cd $(src)/.. && pwd)

TARGET_MODULE:=xdma

EXTRA_CFLAGS := -I$(topdir)/include $(XVC_FLAGS)
# Add kernel version checks for 5.15+ and 5.16+ compatibility
KERN_VER := $(shell uname -r | sed -r 's/^([0-9]+)\.([0-9]+).*/\1\2/')
ifeq ($(shell test $(KERN_VER) -ge 515; echo $$?),0)
	EXTRA_CFLAGS += -DHAS_KERNEL_5_15
endif
ifeq ($(shell test $(KERN_VER) -ge 516; echo $$?),0)
	EXTRA_CFLAGS += -DHAS_KERNEL_5_16
endif

ifeq ($(DEBUG),1)
	EXTRA_CFLAGS += -D__LIBXDMA_DEBUG__
endif
ifneq ($(config_bar_num),)
	EXTRA_CFLAGS += -DXDMA_CONFIG_BAR_NUM=$(config_bar_num)
endif
#EXTRA_CFLAGS += -DINTERNAL_TESTING

ifneq ($(KERNELRELEASE),)
	$(TARGET_MODULE)-objs := libxdma.o xdma_cdev.o cdev_ctrl.o cdev_events.o cdev_sgdma.o cdev_xvc.o cdev_bypass.o xdma_mod.o xdma_thread.o
	obj-m := $(TARGET_MODULE).o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	BUILDSYSTEM_DIR := $(KERNELDIR)
	PWD:=$(shell pwd)
all :
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) modules

clean:
	@if [ -d "$(BUILDSYSTEM_DIR)" ]; then \
		$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) clean; \
	fi
	@/bin/rm -f *.ko modules.order *.mod.c *.o *.o.ur-safe .*.o.cmd

install: all
	@rm -f $(shell dirname $(KERNELDIR))/extra/xdma.ko
	@echo "installing kernel modules to $(shell dirname $(KERNELDIR))/xdma ..."
	@mkdir -p -m 755 $(shell dirname $(KERNELDIR))/xdma
	@install -v -m 644 *.ko $(shell dirname $(KERNELDIR))/xdma
	@depmod -a || true

uninstall:
	@echo "Un-installing $(shell dirname $(KERNELDIR))/xdma ..."
	@/bin/rm -rf $(shell dirname $(KERNELDIR))/xdma
	@depmod -a




endif
