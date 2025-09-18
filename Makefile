# -----------------------------------------------------------------------------
# Makefile for npctl - NeXT Laser Printer N2000 control utility
#
# Target: NeXTSTEP 3.3 (m68k)
# Description:
#   This Makefile builds the npctl utility from npctl.c and demo.c.
#   It provides targets for a standard build, a debug build, cleaning up
#   generated binaries, installing npctl to /usr/local/bin with its man page
#   and demo.ps, and uninstalling everything.
#
# Targets:
#   make         : Build the standard npctl executable
#   make debug   : Build npctl.debug with debugging symbols (-g)
#   make clean   : Remove generated binaries
#   make install : Install npctl, man page, and demo.ps (requires root)
#   make uninstall : Remove npctl, man page, and demo.ps (requires root)
# -----------------------------------------------------------------------------

CC = cc
CFLAGS = -Wall -g -O0
INSTALL_DIR = /usr/local/bin
MANDIR = /NextLibrary/Documentation/ManPages/man1

# Object files
OBJS = npctl.o demo.o

# Default target: build standard executable
npctl: $(OBJS)
	$(CC) $(CFLAGS) -o npctl $(OBJS)

# Compile npctl.c
npctl.o: npctl.c demo.h
	$(CC) $(CFLAGS) -c npctl.c

# Compile demo.c
demo.o: demo.c demo.h
	$(CC) $(CFLAGS) -c demo.c

# Debug build with debugging symbols
debug: $(OBJS)
	$(CC) $(CFLAGS) -DDEBUG -o npctl.debug $(OBJS)

# Clean up generated files
clean:
	rm -f npctl npctl.debug $(OBJS)

# Install the binary and man page
install: npctl
	cp npctl $(INSTALL_DIR)
	chmod 755 $(INSTALL_DIR)/npctl
	cp npctl.1 $(MANDIR)/
	chmod 644 $(MANDIR)/npctl.1
	@echo "npctl installed to $(INSTALL_DIR)"
	@echo "npctl.1 installed to $(MANDIR)"

# Uninstall the binary and man page
uninstall:
	rm -f $(INSTALL_DIR)/npctl
	rm -f $(MANDIR)/npctl.1
	@echo "npctl removed from $(INSTALL_DIR)"
	@echo "npctl.1 removed from $(MANDIR)"
