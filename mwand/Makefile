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

install:
	$(MAKE) -C src install

uninstall:
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
