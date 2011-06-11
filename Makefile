PREFIX ?= /usr/local
CFLAGS ?= -O3 -g -Wall

VERSION=0.73.00
# variables for RPM creation
TOPDIR=$(HOME)/rpmbuild
SPECFILE=pktriggercord.spec
RPM_BUILD_ROOT ?=

GUI_LDFLAGS=$(shell pkg-config --libs gtk+-2.0 libglade-2.0)
GUI_CFLAGS=$(shell pkg-config --cflags gtk+-2.0 libglade-2.0)

default: cli pktriggercord
all: srczip rpm win
cli: pktriggercord-cli
install: install-app

OBJS = pslr.o pslr_scsi.o
WIN_DLLS_DIR=win_dlls
SOURCE_PACKAGE_FILES = Makefile Changelog COPYING INSTALL BUGS pentax.rules samsung.rules pslr_scsi.h pslr_scsi.c pslr_scsi_linux.c pslr_scsi_win.c pslr.h pslr.c pktriggercord.c pktriggercord-cli.c pktriggercord.glade $(SPECFILE) $(WIN_DLLS_DIR)
TARDIR = pktriggercord-$(VERSION)
SRCZIP = pkTriggerCord-$(VERSION).src.tar.gz

WINGCC=i586-pc-mingw32-gcc
WINMINGW=/usr/i586-pc-mingw32/sys-root/mingw
WINDIR=$(TARDIR)-win

pslr.o: pslr_scsi.o pslr.c pslr.h

pktriggercord-cli: pktriggercord-cli.c $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -DVERSION='"$(VERSION)"' -o $@ -L. 

debug: CFLAGS+=-DDEBUG
debug: GUI_CFLAGS+=-DDEBUG

debug: cli pktriggercord

%.o : %.c %.h
	$(CC) $(CFLAGS) -fPIC -c $<

pktriggercord: pktriggercord.c $(OBJS)
	$(CC) $(GUI_CFLAGS) $(GUI_LDFLAGS) -DDATADIR=\"$(PREFIX)/share/pktriggercord\" $? $(LDFLAGS) -o $@ -L. 

install-app:
	install -d $(PREFIX)/bin
	install -s -m 0755 pktriggercord-cli $(PREFIX)/bin/
	install -d $(RPM_BUILD_ROOT)/etc/udev/rules.d
	install -m 0644 pentax.rules $(RPM_BUILD_ROOT)/etc/udev/
	install -m 0644 samsung.rules $(RPM_BUILD_ROOT)/etc/udev/
	cd $(RPM_BUILD_ROOT)/etc/udev/rules.d;\
	ln -sf ../pentax.rules 025_pentax.rules;\
	ln -sf ../samsung.rules 025_samsung.rules
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

# Windows cross-compile
win: clean
	$(WINGCC) $(CFLAGS) -c pslr_scsi.c
	$(WINGCC) $(CFLAGS) -c pslr.c
	$(WINGCC) $(CFLAGS) $(OBJS) -DVERSION='"$(VERSION)"' -o pktriggercord-cli.exe pktriggercord-cli.c
	$(WINGCC) -mms-bitfields -DDATADIR=\".\" -O3 pktriggercord.c -g -Wall pslr.o pslr_scsi.o -o pktriggercord.exe -lgtk-win32-2.0 -lgdk-win32-2.0 -lgdk_pixbuf-2.0 -lgobject-2.0 -lglib-2.0 -lgio-2.0 -I$(WINMINGW)/include/gtk-2.0/ -I$(WINMINGW)/lib/gtk-2.0/include/ -I$(WINMINGW)/include/atk-1.0/ -I$(WINMINGW)/include/cairo/ -I$(WINMINGW)/include/gdk-pixbuf-2.0/ -I$(WINMINGW)/include/glib-2.0 -I$(WINMINGW)/lib/glib-2.0/include -I$(WINMINGW)/include/pango-1.0/ -I$(WINMINGW)/include/libglade-2.0/ -lglade-2.0 -L.
	mkdir -p $(WINDIR)
	cp pktriggercord.exe pktriggercord-cli.exe pktriggercord.glade $(WINDIR)
	cp $(WIN_DLLS_DIR)/*.dll $(WINDIR)
	rm -f $(WINDIR).zip
	zip -rj $(WINDIR).zip $(WINDIR)
	rm -r $(WINDIR)
