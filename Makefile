JSONDIR=src/external/js0n

PREFIX ?= /usr/local
PKTDATADIR = $(PREFIX)/share/pktriggercord
CFLAGS ?= -O3 -g -Wall -fvisibility=hidden -I$(JSONDIR)
# -Wextra
LDFLAGS ?= -lm

MANDIR = $(PREFIX)/share/man
MAN1DIR = $(MANDIR)/man1

MAJORVERSION=0
VERSION=0.85.01
VERSIONCODE=$(shell echo $(VERSION) | sed s/\\.//g | sed s/^0// )
# variables for RPM creation
TOPDIR=$(HOME)/rpmbuild
SPECFILE=pktriggercord.spec

# variables for DEB creation
DEBEMAIL="andras.salamon@melda.info"
DEBFULLNAME="Andras Salamon"

# variables for RPM/DEB creation
DESTDIR ?=
ARCH ?= $(shell uname -m)

#variables for Android
ANDROID=android
ANDROID_DIR = android
ANDROID_PROJECT_NAME = PkTriggerCord
ANDROID_PACKAGE = info.melda.sala.pktriggercord
APK_FILE = $(PROJECT_NAME)-debug.apk

MANS = pktriggercord-cli.1 pktriggercord.1
SRCOBJNAMES = pslr pslr_enum pslr_scsi pslr_log pslr_lens pslr_model pktriggercord-servermode
OBJS = $(SRCOBJNAMES:=.o) $(JSONDIR)/js0n.o
# building lib requires recompilation, so we use different objets
LIB_OBJS=$(OBJS:.o=.ol)

WIN_DLLS_DIR=win_dlls
SOURCE_PACKAGE_FILES = Makefile Changelog COPYING INSTALL BUGS $(MANS) pentax_scsi_protocol.md pentax.rules samsung.rules $(SRCOBJNAMES:=.h) $(SRCOBJNAMES:=.c) pslr_scsi_linux.c pslr_scsi_win.c pslr_scsi_openbsd.c pslr_shared.h exiftool_pentax_lens.txt pktriggercord.c  pktriggercord-cli.c pktriggercord.ui pentax_settings.json $(SPECFILE) android_scsi_sg.h rad10/ src/
TARDIR = pktriggercord-$(VERSION)
SRCZIP = pkTriggerCord-$(VERSION).src.tar.gz

LOCALMINGW=i686-w64-mingw32
WINGCC=i686-w64-mingw32-gcc
WINDIR=$(TARDIR)-win

LOCAL_CFLAGS=$(CFLAGS) -DPK_LIB_STATIC
LOCAL_LDFLAGS=$(LDFLAGS)

CLI_CFLAGS=$(LOCAL_CFLAGS)
CLI_LDFLAGS=$(LOCAL_LDFLAGS)

GUI_CFLAGS=$(LOCAL_CFLAGS) $(shell pkg-config --cflags gtk+-2.0 gmodule-2.0) -DGTK_DISABLE_SINGLE_INCLUDES -DGSEAL_ENABLE
#-DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED
GUI_LDFLAGS=$(LOCAL_LDFLAGS) $(shell pkg-config --libs gtk+-2.0 gmodule-2.0)

CLI_TARGET=pktriggercord-cli
GUI_TARGET=pktriggercord
LIB_TARGET=libpktriggercord.so
LIB_FILE=libpktriggercord.so.$(VERSION)

#variables modification for Windows cross compilation
ifeq ($(ARCH),Win32)
	CC=i686-w64-mingw32-gcc
	AR=i686-w64-mingw32-ar

	LOCAL_CFLAGS+= -mms-bitfields

	GUI_CFLAGS=$(LOCAL_CFLAGS) -I$(LOCALMINGW)/include/gtk-2.0/ -I$(LOCALMINGW)/lib/gtk-2.0/include/ -I$(LOCALMINGW)/include/atk-1.0/ -I$(LOCALMINGW)/include/cairo/ -I$(LOCALMINGW)/include/gdk-pixbuf-2.0/ -I$(LOCALMINGW)/include/pango-1.0/ -I$(LOCALMINGW)/include/glib-2.0 -I$(LOCALMINGW)/lib/glib-2.0/include
	GUI_LDFLAGS=-L$(LOCALMINGW)/lib -lgtk-win32-2.0 -lgdk-win32-2.0 -lgdk_pixbuf-2.0 -lgobject-2.0 -lglib-2.0 -lgio-2.0

	#some build of MinGW enforce this. Some doesn't. Ensure consistent behaviour
	CLI_LDFLAGS+= -Wl,--force-exe-suffix
	GUI_LDFLAGS+= -Wl,--force-exe-suffix

	CLI_TARGET=pktriggercord-cli.exe
	GUI_TARGET=pktriggercord.exe
	LIB_TARGET=libpktriggercord-$(VERSION).dll
	LIB_FILE=$(LIB_TARGET)
endif

default: cli gui lib
ifneq ($(ARCH),Win32)
all: srczip rpm pktriggercord_commandline.html
endif
cli: $(CLI_TARGET)
gui: $(GUI_TARGET)

lib: LOCAL_CFLAGS += -DPK_LIB_EXPORTS
lib: $(LIB_TARGET)

pslr.o%: pslr_enum.o$* pslr_scsi.o$* pslr.c

pslr_scsi.o%: $(wildcard pslr_scsi_*.c)

libpktriggercord.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

libpktriggercord.so: libpktriggercord.so.$(MAJORVERSION)
	ln -s $< $@

libpktriggercord.so.$(MAJORVERSION): libpktriggercord.so.$(VERSION) 
	ln -s $< $@

$(LIB_FILE): $(LIB_OBJS)
	$(CC) $(LOCAL_CFLAGS) -shared -Wl,-soname,libpktriggercord.so.$(MAJORVERSION) $^ -o $@ $(LOCAL_LDFLAGS) -L.

$(CLI_TARGET): pktriggercord-cli.c libpktriggercord.a
	$(CC) $(CLI_CFLAGS) $< -DVERSION='"$(VERSION)"' -DPKTDATADIR=\"$(PKTDATADIR)\" -o $@ -Wl,libpktriggercord.a $(CLI_LDFLAGS) -L.

ifeq ($(ARCH),Win32)
$(GUI_TARGET): $(LOCALMINGW)/bin $(LOCALMINGW)/dll
endif

$(GUI_TARGET): pktriggercord.c libpktriggercord.a
	$(CC) $(GUI_CFLAGS) $< -DVERSION='"$(VERSION)"' -DPKTDATADIR=\"$(PKTDATADIR)\" -o $@ -Wl,libpktriggercord.a $(GUI_LDFLAGS) -L.

$(JSONDIR)/js0n.o: $(JSONDIR)/js0n.c $(JSONDIR)/js0n.h
	$(CC) $(LOCAL_CFLAGS) -fPIC -c $< -o $@

%.o: %.c %.h
	$(CC) $(LOCAL_CFLAGS) -DPKTDATADIR=\"$(PKTDATADIR)\" -fPIC -c $< -o $@

$(JSONDIR)/js0n.ol: $(JSONDIR)/js0n.c $(JSONDIR)/js0n.h
	$(CC) $(LOCAL_CFLAGS) -fPIC -c $< -o $@

%.ol: %.c %.h
	$(CC) $(LOCAL_CFLAGS) -DPKTDATADIR=\"$(PKTDATADIR)\" -fPIC -c $< -o $@

install: pktriggercord-cli pktriggercord
	install -d $(DESTDIR)/$(PREFIX)/bin
	install -s -m 0755 pktriggercord-cli $(DESTDIR)/$(PREFIX)/bin/
	(which setcap && setcap CAP_SYS_RAWIO+eip $(DESTDIR)/$(PREFIX)/bin/pktriggercord-cli) || true
	install -d $(DESTDIR)/etc/udev/rules.d
	install -m 0644 pentax.rules $(DESTDIR)/etc/udev/
	install -m 0644 samsung.rules $(DESTDIR)/etc/udev/
	cd $(DESTDIR)/etc/udev/rules.d;\
	ln -sf ../pentax.rules 95_pentax.rules;\
	ln -sf ../samsung.rules 95_samsung.rules
	install -d -m 0755 $(DESTDIR)/$(MAN1DIR)
	install -m 0644 $(MANS) $(DESTDIR)/$(MAN1DIR)
	if [ -e ./pktriggercord ] ; then \
	install -s -m 0755 pktriggercord $(DESTDIR)/$(PREFIX)/bin/; \
	(which setcap && setcap CAP_SYS_RAWIO+eip $(DESTDIR)/$(PREFIX)/bin/pktriggercord) || true; \
	install -d $(DESTDIR)/$(PREFIX)/share/pktriggercord/; \
	install -m 0644 pktriggercord.ui $(DESTDIR)/$(PREFIX)/share/pktriggercord/ ; \
	install -m 0644 pentax_settings.json $(DESTDIR)/$(PREFIX)/share/pktriggercord/ ; \
	fi

clean:
	rm -f *.o* *.a $(JSONDIR)/*.o*
	rm -f pktriggercord pktriggercord-cli *.so*
	rm -f pktriggercord.exe pktriggercord-cli.exe *.dll
	rm -f pktriggercord_commandline.html
	rm -f *.orig
	rm -f *.log

uninstall:
	rm -f $(PREFIX)/bin/pktriggercord $(PREFIX)/bin/pktriggercord-cli
	rm -rf $(PREFIX)/share/pktriggercord
	rm -f /etc/udev/pentax.rules
	rm -f /etc/udev/rules.d/95_pentax.rules
	rm -f /etc/udev/samsung.rules
	rm -f /etc/udev/rules.d/95_samsung.rules

srczip: clean
	mkdir -p $(TARDIR)
	cp -r $(SOURCE_PACKAGE_FILES) $(TARDIR)/
	mkdir -p $(TARDIR)/$(WIN_DLLS_DIR)
	cp -r $(WIN_DLLS_DIR)/*.dll $(TARDIR)/$(WIN_DLLS_DIR)/
	mkdir -p $(TARDIR)/debian
	cp -r debian/* $(TARDIR)/debian/
	mkdir -p $(TARDIR)/android/res
	cp -r $(ANDROID_DIR)/res/* $(TARDIR)/$(ANDROID_DIR)/res/
	mkdir -p $(TARDIR)/android/src
	cp -r $(ANDROID_DIR)/src/* $(TARDIR)/$(ANDROID_DIR)/src/
	mkdir -p $(TARDIR)/android/jni
	cp -r $(ANDROID_DIR)/jni/* $(TARDIR)/$(ANDROID_DIR)/jni/
	cp $(ANDROID_DIR)/build.gradle $(TARDIR)/$(ANDROID_DIR)/
	cp $(ANDROID_DIR)/project.properties $(TARDIR)/$(ANDROID_DIR)/
	cp $(ANDROID_DIR)/proguard-rules.pro $(TARDIR)/$(ANDROID_DIR)/
	cp $(ANDROID_DIR)/AndroidManifest.xml $(TARDIR)/$(ANDROID_DIR)/
	tar cf - $(TARDIR) | gzip > $(SRCZIP)
	rm -rf $(TARDIR)

ifneq ($(ARCH),Win32)
srcrpm: srczip
	install $(SPECFILE) $(TOPDIR)/SPECS/
	install $(SRCZIP) $(TOPDIR)/SOURCES/
	rpmbuild -bs $(SPECFILE)
	cp $(TOPDIR)/SRPMS/pktriggercord-$(VERSION)-1.src.rpm .

rpm: srcrpm
	rpmbuild -ba $(SPECFILE)
	cp $(TOPDIR)/RPMS/$(ARCH)/pktriggercord-$(VERSION)-1.$(ARCH).rpm .

deb: srczip
	rm -f pktriggercord*orig.tar.gz
	rm -f pktriggercord*debian.tar.gz
	rm -f pktriggercord*armhf*
	rm -rf pktriggercord-$(VERSION)
	tar xvfz pkTriggerCord-$(VERSION).src.tar.gz
	cd pktriggercord-$(VERSION);\
	dh_make -y --single -f ../pkTriggerCord-$(VERSION).src.tar.gz;\
	cp ../debian/* debian/;\
	find debian/ -size 0 | xargs rm -f;\
	dpkg-buildpackage -us -uc
	rm -rf pktriggercord-$(VERSION)

# Remote deb creation on Raspberry PI
# address, dir hardwired
remotedeb:
	git ls-files | tar Tzcf - - | ssh pi@raspberrypi "rm -rf /tmp/pktriggercord && cd /tmp && mkdir pktriggercord && tar xzfv - -C pktriggercord && cd pktriggercord && make clean deb"
	scp pi@raspberrypi:/tmp/pktriggercord/pktriggercord_*.deb .
endif


# converting lens info from exiftool
exiftool_pentax:
	git archive --remote=git://git.code.sf.net/p/exiftool/code HEAD:lib/Image/ExifTool Pentax.pm | tar -x
	cat Pentax.pm | sed -n '/%pentaxLensTypes\ =/,/%pentaxModelID/p' | grep -v '^\s*#' | sed -e "s/[ ]*'\([0-9]\) \([0-9]\{1,3\}\)' => '\(.*\)',.*/{\1, \2, \"\3\"},/g;tx;d;:x" > exiftool_pentax_lens.txt
	rm Pentax.pm

pktriggercord_commandline.html: pktriggercord-cli.1
	cat $< | sed s/\\\\-/-/g | groff -man -Thtml -mwww -P "-lr" > $@

# Windows cross-compile
$(LOCALMINGW)/download:
	mkdir -p $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.24/gtk+_2.24.10-1_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.24/gtk+-dev_2.24.10-1_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.28/glib-dev_2.28.8-1_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/atk/1.32/atk-dev_1.32.0-2_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/pango/1.29/pango-dev_1.29.4-1_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/gdk-pixbuf/2.24/gdk-pixbuf-dev_2.24.0-1_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/gdk-pixbuf/2.24/gdk-pixbuf_2.24.0-1_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/cairo-dev_1.10.2-2_win32.zip -P $(LOCALMINGW)/download

	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.28/glib_2.28.8-1_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/atk/1.32/atk_1.32.0-2_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/pango/1.29/pango_1.29.4-1_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/cairo_1.10.2-2_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/libffi_3.0.6-1_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/libpng_1.4.12-1_win32.zip -P $(LOCALMINGW)/download

$(LOCALMINGW)/bin: $(LOCALMINGW)/download
	unzip -o $(LOCALMINGW)/download/gtk+_2.24.10-1_win32.zip -d $(LOCALMINGW)
	unzip -o $(LOCALMINGW)/download/gtk+-dev_2.24.10-1_win32.zip -d $(LOCALMINGW)
	unzip -o $(LOCALMINGW)/download/glib-dev_2.28.8-1_win32.zip -d $(LOCALMINGW)
	unzip -o $(LOCALMINGW)/download/atk-dev_1.32.0-2_win32.zip -d $(LOCALMINGW)
	unzip -o $(LOCALMINGW)/download/pango-dev_1.29.4-1_win32.zip -d $(LOCALMINGW)
	unzip -o $(LOCALMINGW)/download/gdk-pixbuf-dev_2.24.0-1_win32.zip -d $(LOCALMINGW)
	unzip -o $(LOCALMINGW)/download/gdk-pixbuf_2.24.0-1_win32.zip -d $(LOCALMINGW)
	unzip -o $(LOCALMINGW)/download/cairo-dev_1.10.2-2_win32.zip -d $(LOCALMINGW)

$(LOCALMINGW)/dll: $(LOCALMINGW)/download $(LOCALMINGW)/bin
	unzip -j -o $(LOCALMINGW)/download/glib_2.28.8-1_win32.zip -d $(LOCALMINGW)/dll bin/libgio-2.0-0.dll bin/libglib-2.0-0.dll bin/libgmodule-2.0-0.dll bin/libgobject-2.0-0.dll
	unzip -j -o $(LOCALMINGW)/download/pango_1.29.4-1_win32.zip -d $(LOCALMINGW)/dll bin/libpango-1.0-0.dll bin/libpangowin32-1.0-0.dll bin/libpangocairo-1.0-0.dll
	unzip -j -o $(LOCALMINGW)/download/cairo_1.10.2-2_win32.zip -d $(LOCALMINGW)/dll bin/libcairo-2.dll
	unzip -j -o $(LOCALMINGW)/download/atk_1.32.0-2_win32.zip -d $(LOCALMINGW)/dll bin/libatk-1.0-0.dll
	unzip -j -o $(LOCALMINGW)/download/libffi_3.0.6-1_win32.zip -d $(LOCALMINGW)/dll bin/libffi-5.dll
	unzip -j -o $(LOCALMINGW)/download/libpng_1.4.12-1_win32.zip -d $(LOCALMINGW)/dll bin/libpng14-14.dll
	
	mkdir -p $(LOCALMINGW)/dll
	cp $(LOCALMINGW)/bin/libgtk-win32-2.0-0.dll $(LOCALMINGW)/dll/

ifeq ($(ARCH),Win32)
dist: pktriggercord_commandline.html cli gui lib
	rm -rf $(WINDIR)
	mkdir -p $(WINDIR)
	cp libpktriggercord-$(VERSION).dll pktriggercord-cli.exe pktriggercord.exe $(WINDIR)
	cp Changelog COPYING pktriggercord_commandline.html pktriggercord.ui pentax_settings.json $(WINDIR)
	cp $(LOCALMINGW)/dll/*.dll $(WINDIR)
	cp $(WIN_DLLS_DIR)/*.dll $(WINDIR)
	rm -f $(WINDIR).zip
	zip -rj $(WINDIR).zip $(WINDIR)
	rm -r $(WINDIR)
else
dist: rpm
endif

androidclean:
	cd $(ANDROID_DIR) && ./gradlew clean

androidver:
	sed -i s/versionName\ \".*\"/versionName\ \"$(VERSION)\"/ $(ANDROID_DIR)/build.gradle
	sed -i s/versionCode\ .*/versionCode\ $(VERSIONCODE)/ $(ANDROID_DIR)/build.gradle

android:
	cd $(ANDROID_DIR) && ./gradlew assembleDebug
	cp $(ANDROID_DIR)/build/outputs/apk/debug/$(ANDROID_PACKAGE).$(ANDROID_PROJECT_NAME)-$(VERSION)-debug.apk .
	echo "android build is EXPERIMENTAL. Use it at your own risk"

androidrelease:
	cd $(ANDROID_DIR) && ./gradlew assembleRelease --no-daemon
	cp $(ANDROID_DIR)/build/outputs/apk/release/$(ANDROID_PACKAGE).$(ANDROID_PROJECT_NAME)-$(VERSION)-release.apk .
	echo "android build is EXPERIMENTAL. Use it at your own risk"

astyle:
	astyle --options=astylerc *.h *.c

.PHONY: android androidrelease
