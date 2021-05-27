CONTIKI_PROJECT = twofaced-node twofaced-root
all: $(CONTIKI_PROJECT)

PLATFORMS_ONLY = cooja zoul
BOARDS_ONLY = firefly firefly-reva remote-reva remote-revb

CONTIKI = ../..

ifneq ($(TARGET),cooja)
  MAKE_MAC = MAKE_MAC_TWOFACED
else
  MAKE_MAC = MAKE_MAC_CSMA
endif
MAKE_NET = MAKE_NET_IPV6
MAKE_ROUTING = MAKE_ROUTING_RPL_CLASSIC

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell

# Only add following relative modules for non-cooja targets
ifneq ($(TARGET),cooja)
  MODULES_REL += $(TARGET)/$(BOARD)
  MODULES_REL += $(TARGET)/dev/twofaced-rf
  MODULES_REL += /dev/radio/twofaced-rf
endif

ifeq ($(MAKE_MAC),MAKE_MAC_TWOFACED)
  MODULES_REL += net/mac/twofaced-mac
  CFLAGS += -DMAC_CONF_WITH_TWOFACED=1
endif

# PROJECT_SOURCEFILES +=

WERROR = 0

include $(CONTIKI)/Makefile.include
