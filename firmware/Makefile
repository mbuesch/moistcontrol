# Project name
NAME			:= moistcontrol

# Project source files
SRCS			:= comm.c \
			   controller.c \
			   datetime.c \
			   log.c \
			   main.c \
			   ioext.c \
			   pcf8574.c \
			   rv3029.c \
			   sensor.c \
			   twi_master.c \
			   twi_master_sync.c \
			   util.c

# Project fuse bits
LFUSE			:= 0xA0
HFUSE			:= 0xD9

# CPU speed, in Hz
F_CPU			:= 16000000ul

# Architecture configuration
GCC_ARCH		:= atmega8
AVRDUDE_ARCH		:= m8
AVRDUDE_SPEED		:= 1
AVRDUDE_SLOW_SPEED	:= 100

# Programmer selection.
# Values can be:  avrisp2, mysmartusb
PROGRAMMER		:= avrisp2

# Additional compiler flags
CFLAGS			:= -DTWI_SCL_HZ=100000ul \
			   -DCOMM_BAUDRATE=19200ul \
			   -DCOMM_PAYLOAD_LEN=12
LDFLAGS			:=

# Additional "clean" and "distclean" target files
CLEAN_FILES		:=
DISTCLEAN_FILES		:=


# =============================================================================
# =============================================================================
# =============================================================================


BINEXT			:=
NODEPS			:=

# The toolchain definitions
CC			= avr-gcc$(BINEXT)
OBJCOPY			= avr-objcopy$(BINEXT)
OBJDUMP			= avr-objdump$(BINEXT)
SIZE			= avr-size$(BINEXT)
MKDIR			= mkdir$(BINEXT)
MV			= mv$(BINEXT)
RM			= rm$(BINEXT)
CP			= cp$(BINEXT)
ECHO			= echo$(BINEXT)
GREP			= grep$(BINEXT)
TRUE			= true$(BINEXT)
TEST			= test$(BINEXT)
AVRDUDE			= avrdude$(BINEXT)
MYSMARTUSB		= mysmartusb.py
DOXYGEN			= doxygen$(BINEXT)

V			:= @		# Verbose build:  make V=1
O			:= s		# Optimize flag
Q			:= $(V:1=)
QUIET_CC		= $(Q:@=@$(ECHO) '     CC       '$@;)$(CC)
QUIET_DEPEND		= $(Q:@=@$(ECHO) '     DEPEND   '$@;)$(CC)
QUIET_OBJCOPY		= $(Q:@=@$(ECHO) '     OBJCOPY  '$@;)$(OBJCOPY)
QUIET_SIZE		= $(Q:@=@$(ECHO) '     SIZE     '$@;)$(SIZE)

CFLAGS			+= -mmcu=$(GCC_ARCH) -std=c99 -g -O$(O) -Wall \
			  "-Dinline=inline __attribute__((__always_inline__))" \
			  -fshort-enums -DF_CPU=$(F_CPU)

BIN			:= $(NAME).bin
HEX			:= $(NAME).hex
EEP			:= $(NAME).eep.hex

.SUFFIXES:
.DEFAULT_GOAL := all

# Programmer parameters
ifeq ($(PROGRAMMER),mysmartusb)
AVRDUDE_PROGRAMMER	:= avr910
PROGPORT		:= /dev/ttyUSB0
endif
ifeq ($(PROGRAMMER),avrisp2)
AVRDUDE_PROGRAMMER	:= avrisp2
PROGPORT		:= usb
endif

ifeq ($(AVRDUDE_PROGRAMMER),)
$(error Invalid PROGRAMMER specified)
endif

PROGRAMMER_CMD_PWRCYCLE := \
	$(if $(filter mysmartusb,$(PROGRAMMER)), \
		$(MYSMARTUSB) -p0 $(PROGPORT) && \
		sleep 1 && \
		$(MYSMARTUSB) -p1 $(PROGPORT) \
	)

PROGRAMMER_CMD_PROG_ENTER := \
	$(if $(filter mysmartusb,$(PROGRAMMER)), \
		$(MYSMARTUSB) -mp $(PROGPORT) \
	)

PROGRAMMER_CMD_PROG_LEAVE := \
	$(if $(filter mysmartusb,$(PROGRAMMER)), \
		$(MYSMARTUSB) -md $(PROGPORT) \
	)

DEPS = $(sort $(patsubst %.c,dep/%.d,$(1)))
OBJS = $(sort $(patsubst %.c,obj/%.o,$(1)))

# Generate dependencies
$(call DEPS,$(SRCS)): dep/%.d: %.c 
	@$(MKDIR) -p $(dir $@)
	@$(MKDIR) -p obj
	$(QUIET_DEPEND) -o $@.tmp -MM -MT "$@ $(patsubst dep/%.d,obj/%.o,$@)" $(CFLAGS) $<
	@$(MV) -f $@.tmp $@

ifeq ($(NODEPS),)
-include $(call DEPS,$(SRCS))
endif

# Generate object files
$(call OBJS,$(SRCS)): obj/%.o: %.c
	@$(MKDIR) -p $(dir $@)
	$(QUIET_CC) -o $@ -c $(CFLAGS) $<

all: $(HEX)

%.s: %.c
	$(QUIET_CC) $(CFLAGS) -S $*.c

$(BIN): $(call OBJS,$(SRCS))
	$(QUIET_CC) $(CFLAGS) -o $(BIN) $(call OBJS,$(SRCS)) $(LDFLAGS) $(LDFLAGS)

$(HEX): $(BIN)
	$(QUIET_OBJCOPY) -R.eeprom -O ihex $(BIN) $(HEX)
	@$(if $(filter .exe,$(BINEXT)),$(TRUE), \
	$(OBJDUMP) -h $(BIN) | $(GREP) -qe .eeprom && \
	 $(OBJCOPY) -j.eeprom --set-section-flags=.eeprom="alloc,load" \
	 --change-section-lma .eeprom=0 -O ihex $(BIN) $(EEP) \
	 || $(TRUE))
	$(QUIET_SIZE) $(BIN)

avrdude:
	$(call PROGRAMMER_CMD_PROG_ENTER)
	$(AVRDUDE) -B $(AVRDUDE_SPEED) -p $(AVRDUDE_ARCH) \
	 -c $(AVRDUDE_PROGRAMMER) -P $(PROGPORT) -t
	$(call PROGRAMMER_CMD_PWRCYCLE)
	$(call PROGRAMMER_CMD_PROG_LEAVE)

install: all
	$(call PROGRAMMER_CMD_PROG_ENTER)
	$(AVRDUDE) -B $(AVRDUDE_SPEED) -p $(AVRDUDE_ARCH) \
	 -c $(AVRDUDE_PROGRAMMER) -P $(PROGPORT) \
	 -U flash:w:$(HEX)
	$(TEST) -r $(EEP) && ( \
	 $(AVRDUDE) -B $(AVRDUDE_SPEED) -p $(AVRDUDE_ARCH) \
	  -c $(AVRDUDE_PROGRAMMER) -P $(PROGPORT) \
	  -U eeprom:w:$(EEP) \
	) || $(TRUE)
	$(call PROGRAMMER_CMD_PWRCYCLE)
	$(call PROGRAMMER_CMD_PROG_LEAVE)

reset:
	$(call PROGRAMMER_CMD_PROG_ENTER)
	$(AVRDUDE) -B $(AVRDUDE_SLOW_SPEED) -p $(AVRDUDE_ARCH) \
	 -c $(AVRDUDE_PROGRAMMER) -P $(PROGPORT) \
	 -U signature:r:/dev/null:i -q -q
	$(call PROGRAMMER_CMD_PWRCYCLE)

writefuse:
	$(call PROGRAMMER_CMD_PROG_ENTER)
	$(AVRDUDE) -B $(AVRDUDE_SLOW_SPEED) -p $(AVRDUDE_ARCH) \
	 -c $(AVRDUDE_PROGRAMMER) -P $(PROGPORT) -q -q \
	 -U lfuse:w:$(LFUSE):m \
	 -U hfuse:w:$(HFUSE):m
	$(call PROGRAMMER_CMD_PWRCYCLE)
	$(call PROGRAMMER_CMD_PROG_LEAVE)

doxygen:
	$(DOXYGEN) Doxyfile

clean:
	-$(RM) -rf obj dep $(BIN) $(CLEAN_FILES)

distclean: clean
	-$(RM) -f $(HEX) $(EEP) $(DISTCLEAN_FILES)
	-$(RM) -f $(if $(filter .exe,$(BINEXT)),$(patsubst %.c,%.s,$(SRCS)),*.s)
