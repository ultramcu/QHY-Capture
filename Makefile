# SPDX-License-Identifier: MPL-2.0
#
# qhy_capture -- Makefile
#
# Build:    make
# Clean:    make clean
# Run:      ./qhy_capture [-e exposure_us] [-g gain] [-o out_file]

CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -Wpedantic -O2

# Override if your QHYCCD SDK lives outside /usr/local
# (e.g.  make QHY_PREFIX=/opt/qhyccd)
QHY_PREFIX ?= /usr/local

CPPFLAGS += -I$(QHY_PREFIX)/include
LDFLAGS  += -L$(QHY_PREFIX)/lib
LDLIBS   += -lqhyccd -lpthread -ldl

# --- macOS notes ----------------------------------------------------
# Older QHY SDK packages installed the dylib as `libqhy.dylib`.
# If linking fails with `-lqhyccd not found`, try:
#     make LDLIBS="-lqhy -lpthread -ldl"
# ---------------------------------------------------------------------

TARGET = qhy_capture
SRC    = qhy_capture.c

$(TARGET): $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) $(LDFLAGS) $(LDLIBS) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o frame.pgm frame.ppm

.PHONY: clean
