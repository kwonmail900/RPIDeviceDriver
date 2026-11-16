obj-m += rpi_ledchar.o
KDIR ?= /lib/modules/$(shell uname -r)/build
CC_USER ?= gcc

.PHONY: all module app clean
all: module app

module:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

app: led_app

led_app: led_app.c
	$(CC_USER) -std=c11 -Wall -Wextra -Werror -O2 -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	rm -f led_app
