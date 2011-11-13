PREFIX ?= /usr/local
CFLAGS ?= -O3 -g -Wall

MANDIR = $(PREFIX)/share/man
MAN1DIR = $(MANDIR)/man1

LIN_CFLAGS = $(CFLAGS)
LIN_LDFLAGS = $(LDFLAGS)

VERSION=0.77.05
# variables for RPM creation
TOPDIR=$(HOME)/rpmbuild
SPECFILE=pktriggercord.spec
RPM_BUILD_ROOT ?=

LIN_GUI_LDFLAGS=$(shell pkg-config --libs gtk+-2.0 libglade-2.0)
LIN_GUI_CFLAGS=$(CFLAGS) $(shell pkg-config --cflags gtk+-2.0 libglade-2.0)

default: cli pktriggercord
all: srczip rpm win pktriggercord_commandline.html
cli: pktriggercord-cli
install: install-app

MANS = pktriggercord-cli.1 pktriggercord.1
OBJS = pslr.o pslr_enum.o pslr_scsi.o pslr_lens.o
WIN_DLLS_DIR=win_dlls
SOURCE_PACKAGE_FILES = Makefile Changelog COPYING INSTALL BUGS $(MANS) pentax.rules samsung.rules pslr_enum.h pslr_enum.c pslr_scsi.h pslr_scsi.c pslr_scsi_linux.c pslr_scsi_win.c pslr.h pslr.c exiftool_pentax_lens.txt pslr_lens.h pslr_lens.c pktriggercord.c pktriggercord-cli.c pktriggercord.glade $(SPECFILE) $(WIN_DLLS_DIR)
TARDIR = pktriggercord-$(VERSION)
SRCZIP = pkTriggerCord-$(VERSION).src.tar.gz

WINGCC=i586-pc-mingw32-gcc
WINMINGW=/usr/i586-pc-mingw32/sys-root/mingw
WINDIR=$(TARDIR)-win

pslr.o: pslr_enum.o pslr_scsi.o pslr.c pslr.h

pktriggercord-cli: pktriggercord-cli.c $(OBJS)
	$(CC) $(LIN_CFLAGS) $^ $(LIN_LDFLAGS) -DVERSION='"$(VERSION)"' -o $@ -L. 

%.o : %.c %.h
	$(CC) $(LIN_CFLAGS) -fPIC -c $<

pktriggercord: pktriggercord.c $(OBJS)
	$(CC) $(LIN_GUI_CFLAGS) $(LIN_GUI_LDFLAGS) -DVERSION='"$(VERSION)"' -DDATADIR=\"$(PREFIX)/share/pktriggercord\" $? $(LIN_LDFLAGS) -o $@ -L. 

install-app:
	install -d $(PREFIX)/bin
	install -s -m 0755 pktriggercord-cli $(PREFIX)/bin/
	install -d $(RPM_BUILD_ROOT)/etc/udev/rules.d
	install -m 0644 pentax.rules $(RPM_BUILD_ROOT)/etc/udev/
	install -m 0644 samsung.rules $(RPM_BUILD_ROOT)/etc/udev/
	cd $(RPM_BUILD_ROOT)/etc/udev/rules.d;\
	ln -sf ../pentax.rules 025_pentax.rules;\
	ln -sf ../samsung.rules 025_samsung.rules
	install -d -m 0755 $(MAN1DIR)
	install -m 0644 $(MANS) $(MAN1DIR)
	if [ -e ./pktriggercord ] ; then \
	install -s -m 0755 pktriggercord $(PREFIX)/bin/; \
	install -d $(PREFIX)/share/pktriggercord/; \
	install -m 0644 pktriggercord.glade $(PREFIX)/share/pktriggercord/ ; \
	fi

clean:
	rm -f pktriggercord pktriggercord-cli *.o
	rm -f pktriggercord.exe pktriggercord-cli.exe

uninstall:
	rm -f $(PREFIX)/bin/pktriggercord $(PREFIX)/bin/pktriggercord-cli
	rm -rf $(PREFIX)/share/pktriggercord
	rm -f /etc/udev/pentax.rules
	rm -f /etc/udev/rules.d/025_pentax.rules
	rm -f /etc/udev/samsung.rules
	rm -f /etc/udev/rules.d/025_samsung.rules

srczip: clean
	mkdir -p $(TARDIR)
	cp -r $(SOURCE_PACKAGE_FILES) $(TARDIR)/
	tar cf - $(TARDIR) | gzip > $(SRCZIP)
	rm -rf $(TARDIR)

srcrpm: srczip
	install $(SPECFILE) $(TOPDIR)/SPECS/
	install $(SRCZIP) $(TOPDIR)/SOURCES/
	rpmbuild -bs $(SPECFILE)
	cp $(TOPDIR)/SRPMS/pktriggercord-$(VERSION)-1.src.rpm .

rpm: srcrpm
	rpmbuild -ba $(SPECFILE)
	cp $(TOPDIR)/RPMS/i386/pktriggercord-$(VERSION)-1.i386.rpm .

WIN_CFLAGS=$(CFLAGS) -I$(WINMINGW)/include/gtk-2.0/ -I$(WINMINGW)/lib/gtk-2.0/include/ -I$(WINMINGW)/include/atk-1.0/ -I$(WINMINGW)/include/cairo/ -I$(WINMINGW)/include/gdk-pixbuf-2.0/ -I$(WINMINGW)/include/pango-1.0/ -I$(WINMINGW)/include/libglade-2.0/
WIN_GUI_CFLAGS=$(WIN_CFLAGS) -I$(WINMINGW)/include/glib-2.0 -I$(WINMINGW)/lib/glib-2.0/include 
WIN_LDFLAGS=-lgtk-win32-2.0 -lgdk-win32-2.0 -lgdk_pixbuf-2.0 -lgobject-2.0 -lglib-2.0 -lgio-2.0  -lglade-2.0

# converting lens info from exiftool
exiftool_pentax_lens.txt:
	cat /usr/lib/perl5/vendor_perl/5.12.3/Image/ExifTool/Pentax.pm | sed -n '/%pentaxLensTypes\ =/,/%pentaxModelID/p' | sed -e "s/[ ]*'\([0-9]\) \([0-9]\{1,3\}\)' => '\(.*\)',.*/{\1, \2, \"\3\"},/g;tx;d;:x" > $@

pktriggercord_commandline.html: pktriggercord-cli.1
	groff $< -man -Thtml -mwww -P "-lr" > $@

# Windows cross-compile
win: clean
	$(WINGCC) $(WIN_CFLAGS) -c pslr_lens.c
	$(WINGCC) $(WIN_CFLAGS) -c pslr_scsi.c
	$(WINGCC) $(WIN_CFLAGS) -c pslr_enum.c
	$(WINGCC) $(WIN_CFLAGS) -c pslr.c
	$(WINGCC) -mms-bitfields -DVERSION='"$(VERSION)"'  pktriggercord-cli.c $(OBJS) -o pktriggercord-cli.exe $(WIN_CFLAGS) $(WIN_LDFLAGS) -L.
	$(WINGCC) -mms-bitfields -DVERSION='"$(VERSION)"' -DDATADIR=\".\" pktriggercord.c $(OBJS) -o pktriggercord.exe $(WIN_GUI_CFLAGS) $(WIN_LDFLAGS) -L.
	mkdir -p $(WINDIR)
	cp pktriggercord.exe pktriggercord-cli.exe pktriggercord.glade Changelog COPYING $(WINDIR)
	cp $(WIN_DLLS_DIR)/*.dll $(WINDIR)
	rm -f $(WINDIR).zip
	zip -rj $(WINDIR).zip $(WINDIR)
	rm -r $(WINDIR)
