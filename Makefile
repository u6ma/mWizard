all:
	@if [ -e src/Makefile ]; then \
		$(MAKE) -C src $(MAKEFLAGS); \
	else \
		if [ -e mf/Makefile.$$(uname) ]; then \
			ln -s ../mf/Makefile.$$(uname) src/Makefile && \
			$(MAKE) -C src $(MAKEFLAGS); \
		else \
			echo "Run: make <target>" && \
			echo "Available targets are:" && \
			ls mf/ | sed 's/Makefile\.//g'; \
		fi \
	fi

.PHONY: clean install uninstall distclean \
	goodies install-goodies uninstall-goodies clean-goodies

# Where the goodies get unpacked and built. These paths come from the
# platform makefile, so pull them in the same way src/ does.
GOODIES_SRC = goodies
GOODIES_BLD = goodies/build

# NOTE: every target here must be declared explicitly. The .DEFAULT rule at
# the bottom treats any unknown target as a platform name and fails.

install:
	$(MAKE) -C src $(MAKEFLAGS) install

uninstall:
	$(MAKE) -C src $(MAKEFLAGS) uninstall

clean:
	$(MAKE) -C src $(MAKEFLAGS) clean

distclean:
	-$(MAKE) -C src $(MAKEFLAGS) clean
	-rm src/Makefile
	-rm -rf $(GOODIES_BLD)

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
	$(MAKE) -C src $(MAKEFLAGS) install-goodies GOODIES_BLD=../$(GOODIES_BLD)

uninstall-goodies:
	$(MAKE) -C src $(MAKEFLAGS) uninstall-goodies

clean-goodies:
	-rm -rf $(GOODIES_BLD)
	
.DEFAULT:
	@if [ -e src/Makefile ]; then rm src/Makefile; fi
	@if ! [ -f mf/Makefile.$@ ]; then \
		echo "Invalid target name: $@" && exit 1; fi
	ln -s ../mf/Makefile.$@ src/Makefile
	$(MAKE) -C src $(MAKEFLAGS)
