#!/usr/bin/make -f
# add a new binary and allow to disable/enable it using the CONFIG_$binaryname
# make parameter
# only allow as value y (enable) and n (disable)
define add_command
  CONFIG_$(1):=$(2)
  ifneq ($$(CONFIG_$(1)),y)
    ifneq ($$(CONFIG_$(1)),n)
      $$(warning invalid value for parameter CONFIG_$(1): $$(CONFIG_$(1)))
    endif
  endif

  obj-$$(CONFIG_$(1)) += $(1)
endef # add_command

# using the make parameter CONFIG_* (e.g. CONFIG_fft_eval_sdl) with the value
# 'y' enables the related feature and 'n' disables it
$(eval $(call add_command,ss-analyser,y))
ss-analyser-y += ss-analyser.o

# fft_eval flags and options
CFLAGS += -Wall -W -std=gnu99 -fno-strict-aliasing -MD -MP
CPPFLAGS += -D_DEFAULT_SOURCE
LDLIBS += -lm

# disable verbose output
ifneq ($(findstring $(MAKEFLAGS),s),y)
ifndef V
	Q_CC = @echo '   ' CC $@;
	Q_LD = @echo '   ' LD $@;
	export Q_CC
	export Q_LD
endif
endif


RM ?= rm -f
INSTALL ?= install
MKDIR ?= mkdir -p
COMPILE.c = $(Q_CC)$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
LINK.o = $(Q_LD)$(CC) $(CFLAGS) $(LDFLAGS) $(TARGET_ARCH)

# standard install paths
PREFIX = /usr/local
BINDIR = $(PREFIX)/sbin

# default target
all: $(obj-y)

# standard build rules
.SUFFIXES: .o .c
.c.o:
	$(COMPILE.c) $(CFLAGS_$(@)) -o $@ $<

$(obj-y):
	$(LINK.o) $^ $(LDLIBS) $(LDLIBS_$(@)) -o $@

clean:
	$(RM) $(BINARY_NAMES) $(OBJ) $(DEP)

install: $(obj-y)
	$(MKDIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(obj-y) $(DESTDIR)$(BINDIR)


# load dependencies
BINARY_NAMES = $(foreach binary,$(obj-y) $(obj-n), $(binary))
OBJ = $(foreach obj, $(BINARY_NAMES), $($(obj)-y))
DEP = $(OBJ:.o=.d)
-include $(DEP)

# add dependency of binary to object
define binary_dependency
$(1): $(2)
endef
$(foreach binary, $(obj-y), $(eval $(call binary_dependency, $(binary), $($(binary)-y))))

.PHONY: all clean install 
