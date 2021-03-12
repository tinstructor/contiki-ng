CONTIKI_PROJECT = twofaced-node twofaced-root
all: $(CONTIKI_PROJECT)

PLATFORMS_ONLY = cooja zoul
BOARDS_ONLY = firefly firefly-reva remote-reva remote-revb

CONTIKI = ../..

MAKE_MAC = MAKE_MAC_CSMA
# MAKE_MAC = MAKE_MAC_OTHER
MAKE_NET = MAKE_NET_IPV6
MAKE_ROUTING = MAKE_ROUTING_RPL_CLASSIC

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell
MODULES_REL += $(TARGET)/$(BOARD)
MODULES_REL += /dev/radio/twofaced-rf
MODULES_REL += $(TARGET)/dev/twofaced-rf
MODULES_REL += net/mac/twofaced-mac

# PROJECT_SOURCEFILES +=

WERROR = 0

include $(CONTIKI)/Makefile.include