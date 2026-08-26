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

.PHONY: clean install uninstall distclean

# Both of these delegate into src/, which has no makefile of its own until a
# platform one has been symlinked there by "all" above. Without these guards
# make reports "No rule to make target `install'" from a directory the user
# never asked about, which says nothing about what to do -- and installing
# before building fails later still, inside install(1), on a missing binary.
#
# Guarded here rather than in the top level so that "make install-mwand" there
# and "make install" run from this directory both get the same message.

install:
	@if [ ! -x src/mwand ]; then \
		echo "mWand is not built. Run 'make mwand' at the top level" && \
		echo "(or 'make' in mwand/) first." && exit 1; fi
	$(MAKE) -C src install

uninstall:
	@if [ ! -e src/Makefile ]; then \
		echo "No platform makefile selected, so the install paths are" && \
		echo "unknown. Run 'make mwand' at the top level first." && exit 1; fi
	$(MAKE) -C src uninstall

clean:
	$(MAKE) -C src clean

distclean:
	-$(MAKE) -C src clean
	-rm src/Makefile

.DEFAULT:
	@if [ -e src/Makefile ]; then rm src/Makefile; fi
	@if ! [ -f mf/Makefile.$@ ]; then \
		echo "Invalid target name: $@" && exit 1; fi
	ln -s ../mf/Makefile.$@ src/Makefile
	$(MAKE) -C src
