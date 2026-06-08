# Greeny Temp Controller — STC8H8K64U USB-direct
# No CH340 needed. USB CDC virtual COM port.
#
# Build:  make
# Flash:  make flash          (auto via USB CDC !ISP)
#         make flash-first    (blank chip: hold P3.2, plug USB)

CC      = sdcc
CFLAGS  = -mmcs51 --model-large --opt-code-size --std-c99
TARGET  = thermo
SRC     = src/main.c
IHX     = $(TARGET).ihx

.PHONY: all clean flash flash-first

all: $(IHX)

$(IHX): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET).ihx $(SRC)
	@echo "Build: $(IHX) done."

clean:
	rm -f *.ihx *.hex *.lk *.lst *.map *.mem *.rel *.rst *.sym *.asm

flash: $(IHX)
	./burn.sh

flash-first: $(IHX)
	./burn.sh --first
