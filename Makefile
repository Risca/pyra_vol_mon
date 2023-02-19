CROSS_COMPILE ?=
TARGET_CC := $(CROSS_COMPILE)gcc
TARGET_LD := $(CROSS_COMPILE)gcc

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

include tests/rules.mk

.PHONY: clean tags
clean:
	rm -rf $(TARGET) $(OBJDIR) $(DEPDIR)

tags:
	ctags $(SRCS)

$(DEPFILES):

include $(wildcard $(DEPFILES))
