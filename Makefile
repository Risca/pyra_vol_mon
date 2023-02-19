CROSS_COMPILE ?=
TARGET_CC := $(CROSS_COMPILE)gcc
TARGET_LD := $(CROSS_COMPILE)gcc
TARGET_STRIP := $(CROSS_COMPILE)strip

OBJDIR := .objs
DEPDIR := .deps

CPPFLAGS := -D_GNU_SOURCE -I.
CFLAGS :=
LDFLAGS := -lc
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

TARGET := pyra_vol_mon
SRCS := pyra_vol_mon.c iio_utils.c config.c iio_event.c main.c
OBJS := $(addprefix $(OBJDIR)/,$(SRCS:.c=.o))
DEPFILES := $(SRCS:%.c=$(DEPDIR)/%.d)

$(TARGET): $(OBJS)
	$(TARGET_LD) $(LDFLAGS) $(OBJS) -o $@

$(OBJDIR)/%.o: %.c $(DEPDIR)/%.d Makefile | $(OBJDIR) $(DEPDIR)
	$(TARGET_CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(OBJDIR) $(DEPDIR): ; @mkdir -p $@

strip: $(TARGET)
	$(TARGET_STRIP) --strip-all $<

include tests/rules.mk

.PHONY: clean tags strip
clean:
	rm -rf $(TARGET) $(OBJDIR) $(DEPDIR)

tags:
	ctags $(SRCS)

$(DEPFILES):

include $(wildcard $(DEPFILES))
