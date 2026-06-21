##############################################################################
# Velo — top-level Makefile
# Builds the Velo language runtime and places the binary in build/
##############################################################################

MAJVER = $(shell grep -E '^\s*#define\s+VELO_VERSION_MAJOR' src/velo_config.h | grep -oE '[0-9]+')
MINVER = $(shell grep -E '^\s*#define\s+VELO_VERSION_MINOR' src/velo_config.h | grep -oE '[0-9]+')
ABIVER = 5.1

export PREFIX  ?= /usr/local
export MULTILIB ?= lib

BUILDDIR = build

##############################################################################
# Primary targets
##############################################################################

.PHONY: all clean install uninstall

all: $(BUILDDIR)/velo

$(BUILDDIR)/velo: $(BUILDDIR)
	@echo "==== Building Velo $(MAJVER).$(MINVER) ===="
	$(MAKE) -C src
	@cp src/velo $(BUILDDIR)/velo
	@echo "==== Build complete: $(BUILDDIR)/velo ===="

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

clean:
	@echo "==== Cleaning ===="
	$(MAKE) -C src clean
	@rm -rf $(BUILDDIR)
	@echo "==== Clean complete ===="

##############################################################################
# Install / uninstall
##############################################################################

DPREFIX        = $(DESTDIR)$(PREFIX)
INSTALL_BIN    = $(DPREFIX)/bin
INSTALL_LIB    = $(DPREFIX)/$(MULTILIB)
INSTALL_INC    = $(DPREFIX)/include/velo
INSTALL_SHARE  = $(DPREFIX)/share
INSTALL_JITLIB = $(INSTALL_SHARE)/velo/jit

MMVERSION      = $(MAJVER).$(MINVER)
RELVER         = $(shell cat src/luajit_relver.txt 2>/dev/null || echo 0)
VERSION        = $(MMVERSION).$(RELVER)

FILES_INC      = lua.h lualib.h lauxlib.h luaconf.h lua.hpp luajit.h
FILES_JITLIB   = bc.lua bcsave.lua dump.lua p.lua v.lua zone.lua \
                 dis_x86.lua dis_x64.lua dis_arm.lua dis_arm64.lua \
                 dis_arm64be.lua dis_ppc.lua dis_mips.lua dis_mipsel.lua \
                 dis_mips64.lua dis_mips64el.lua \
                 dis_mips64r6.lua dis_mips64r6el.lua \
                 vmdef.lua

install: all
	@echo "==== Installing Velo $(VERSION) to $(PREFIX) ===="
	install -d $(INSTALL_BIN) $(INSTALL_LIB) $(INSTALL_INC) \
	           $(INSTALL_JITLIB)
	install -m 0755 $(BUILDDIR)/velo $(INSTALL_BIN)/velo
	install -m 0644 src/libluajit.a  $(INSTALL_LIB)/libvelo.a      2>/dev/null || :
	install -m 0755 src/libluajit.so $(INSTALL_LIB)/libvelo.so     2>/dev/null || :
	cd src && install -m 0644 $(FILES_INC) $(INSTALL_INC)
	cd src/jit && install -m 0644 $(FILES_JITLIB) $(INSTALL_JITLIB)
	@echo "==== Installed to $(PREFIX) ===="

uninstall:
	@echo "==== Uninstalling Velo from $(PREFIX) ===="
	rm -f $(INSTALL_BIN)/velo
	rm -f $(INSTALL_LIB)/libvelo.a $(INSTALL_LIB)/libvelo.so
	rm -rf $(INSTALL_INC) $(INSTALL_JITLIB)
	@echo "==== Uninstalled ===="

##############################################################################
