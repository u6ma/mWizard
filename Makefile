# Sub-makes inherit MAKEFLAGS automatically. Passing $(MAKEFLAGS)
# explicitly turns a flag like -n into a literal target ("n") and
# breaks dry runs, so it is deliberately absent below.

all:
	@if [ -e src/Makefile ]; then \
		$(MAKE) -C src; \
	else \
		if [ -e mf/Makefile.$$(uname) ]; then \
			ln -s ../mf/Makefile.$$(uname) src/Makefile && \
			$(MAKE) -C src; \
		else \
			echo "Run: make <target>" && \
			echo "Available targets are:" && \
			ls mf/ | sed 's/Makefile\.//g'; \
		fi \
	fi

.PHONY: clean install uninstall distclean \
	goodies install-goodies uninstall-goodies clean-goodies \
	mwand install-mwand uninstall-mwand clean-mwand

# Where the goodies get unpacked and built. These paths come from the
# platform makefile, so pull them in the same way src/ does.
GOODIES_SRC = goodies
GOODIES_BLD = goodies/build

# NOTE: every target here must be declared explicitly. The .DEFAULT rule at
# the bottom treats any unknown target as a platform name and fails.

install:
	$(MAKE) -C src install

uninstall:
	$(MAKE) -C src uninstall

clean:
	$(MAKE) -C src clean

distclean:
	-$(MAKE) -C src clean
	-rm src/Makefile
	-rm -rf $(GOODIES_BLD)
	-$(MAKE) -C mwand distclean

# --- goodies -------------------------------------------------------------
#
# Optional extras by alx@fastestcode.org, under CC BY-SA 3.0.
# See goodies/README; the attribution there is required by that license.

goodies:
	@mkdir -p $(GOODIES_BLD)
	@for a in $(GOODIES_SRC)/*.tar.xz; do \
		echo "unpacking $$a"; \
		tar xJf "$$a" -C $(GOODIES_BLD) || exit 1; \
	done
	@if [ -d $(GOODIES_BLD)/xc-core-hr ]; then \
		if command -v xcursorgen >/dev/null 2>&1; then \
			echo "building cursor theme"; \
			cd $(GOODIES_BLD)/xc-core-hr && sh ./build.sh core-hr; \
		else \
			echo "NOTE: xcursorgen not found, skipping the cursor theme."; \
			echo "      Install it and re-run 'make goodies' to build it."; \
		fi \
	fi
	@echo "Goodies unpacked into $(GOODIES_BLD). Run 'make install-goodies' as root."

install-goodies:
	@if [ ! -d $(GOODIES_BLD) ]; then \
		echo "Run 'make goodies' first." && exit 1; fi
	$(MAKE) -C src install-goodies GOODIES_BLD=../$(GOODIES_BLD)

uninstall-goodies:
	$(MAKE) -C src uninstall-goodies

clean-goodies:
	-rm -rf $(GOODIES_BLD)

# --- mWand ---------------------------------------------------------------
#
# The optional launcher. Built and installed separately; nothing in the
# window manager depends on it.

mwand:
	$(MAKE) -C mwand

install-mwand:
	$(MAKE) -C mwand install

uninstall-mwand:
	$(MAKE) -C mwand uninstall

clean-mwand:
	-$(MAKE) -C mwand distclean
	
.DEFAULT:
	@if [ -e src/Makefile ]; then rm src/Makefile; fi
	@if ! [ -f mf/Makefile.$@ ]; then \
		echo "Invalid target name: $@" && exit 1; fi
	ln -s ../mf/Makefile.$@ src/Makefile
	$(MAKE) -C src
