JSONDIR=src/external/js0n

PREFIX ?= /usr/local
PKTDATADIR = $(PREFIX)/share/pktriggercord
CFLAGS ?= -O3 -g -Wall -I$(JSONDIR)
# -Wextra
LDFLAGS ?= -lm

MANDIR = $(PREFIX)/share/man
MAN1DIR = $(MANDIR)/man1

LOCAL_CFLAGS = $(CFLAGS)
LOCAL_LDFLAGS = $(LDFLAGS)

CLI_CFLAGS=$(LOCAL_CFLAGS)
CLI_LDFLAGS=$(LOCAL_LDFLAGS)

GUI_CFLAGS=$(LOCAL_CFLAGS) $(shell pkg-config --cflags gtk+-2.0 gmodule-2.0) -DGTK_DISABLE_SINGLE_INCLUDES -DGSEAL_ENABLE
#-DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED
GUI_LDFLAGS=$(LOCAL_LDFLAGS) $(shell pkg-config --libs gtk+-2.0 gmodule-2.0)

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

CLI_TARGET=pktriggercord-cli
GUI_TARGET=pktriggercord

#variables modification for Windows cross compilation
ifeq ($(ARCH),Win32)
	CC=i686-w64-mingw32-gcc
	AR=i686-w64-mingw32-ar

	LOCAL_CFLAGS+= -mms-bitfields

	GUI_CFLAGS=$(LOCAL_CFLAGS) \
		-I$(LOCALMINGW)/include/gtk-2.0/ \
		-I$(LOCALMINGW)/lib/gtk-2.0/include/ \
		-I$(LOCALMINGW)/include/atk-1.0/ \
		-I$(LOCALMINGW)/include/cairo/ \
		-I$(LOCALMINGW)/include/gdk-pixbuf-2.0/ \
		-I$(LOCALMINGW)/include/pango-1.0/ \
		-I$(LOCALMINGW)/include/glib-2.0 \
		-I$(LOCALMINGW)/lib/glib-2.0/include
	GUI_LDFLAGS=-L$(LOCALMINGW)/lib -lgtk-win32-2.0 -lgdk-win32-2.0 -lgdk_pixbuf-2.0 -lgobject-2.0 -lglib-2.0 -lgio-2.0

	#some build of MinGW enforce this. Some doesn't. Ensure consistent behaviour
	CLI_LDFLAGS+= -Wl,--force-exe-suffix
	GUI_LDFLAGS+= -Wl,--force-exe-suffix

	CLI_TARGET=pktriggercord-cli.exe
	GUI_TARGET=pktriggercord.exe
endif

default: cli gui
ifneq ($(ARCH),Win32)
all: srczip rpm win pktriggercord_commandline.html
endif
cli: $(CLI_TARGET)
gui: $(GUI_TARGET)

MANS = pktriggercord-cli.1 pktriggercord.1
SRCOBJNAMES = pslr pslr_enum pslr_scsi pslr_log pslr_lens pslr_model pktriggercord-servermode pslr_utils
OBJS = $(SRCOBJNAMES:=.o) $(JSONDIR)/js0n.o
WIN_DLLS_DIR=win_dlls
SOURCE_PACKAGE_FILES = Makefile Changelog COPYING INSTALL BUGS $(MANS) pentax_scsi_protocol.md pentax.rules samsung.rules $(SRCOBJNAMES:=.h) $(SRCOBJNAMES:=.c) pslr_scsi_linux.c pslr_scsi_win.c pslr_scsi_openbsd.c exiftool_pentax_lens.txt pktriggercord.c pktriggercord-cli.c pktriggercord.ui pentax_settings.json $(SPECFILE) android_scsi_sg.h rad10/ src/
TARDIR = pktriggercord-$(VERSION)
SRCZIP = pkTriggerCord-$(VERSION).src.tar.gz

LOCALMINGW=i686-w64-mingw32
WINGCC=i686-w64-mingw32-gcc
WINMINGW=/usr/i686-w64-mingw32/sys-root/mingw
WINDIR=$(TARDIR)-win
GTK_BUNDLE=gtk+-bundle_2.24.10-20120208_win32.zip

# List of all the dll required to run the GUI on Windows
GUI_WIN_DLLS = \
	$(LOCALMINGW)/bin/freetype6.dll \
	$(LOCALMINGW)/bin/libfontconfig-1.dll \
	$(LOCALMINGW)/bin/libgdk-win32-2.0-0.dll \
	$(LOCALMINGW)/bin/libgtk-win32-2.0-0.dll \
	$(LOCALMINGW)/bin/libgdk_pixbuf-2.0-0.dll \
	$(LOCALMINGW)/bin/libgthread-2.0-0.dll \
	$(LOCALMINGW)/bin/libpng14-14.dll \
	$(LOCALMINGW)/bin/libgio-2.0-0.dll \
	$(LOCALMINGW)/bin/libglib-2.0-0.dll \
	$(LOCALMINGW)/bin/libgmodule-2.0-0.dll \
	$(LOCALMINGW)/bin/libgobject-2.0-0.dll \
	$(LOCALMINGW)/bin/libpango-1.0-0.dll \
	$(LOCALMINGW)/bin/libpangowin32-1.0-0.dll \
	$(LOCALMINGW)/bin/libpangocairo-1.0-0.dll \
	$(LOCALMINGW)/bin/libpangoft2-1.0-0.dll \
	$(LOCALMINGW)/bin/libcairo-2.dll \
	$(LOCALMINGW)/bin/libatk-1.0-0.dll \
	$(LOCALMINGW)/bin/zlib1.dll \
	$(LOCALMINGW)/bin/intl.dll \
	$(LOCALMINGW)/bin/libexpat-1.dll

pslr.o: pslr_enum.o pslr_scsi.o pslr.c pslr.h

$(CLI_TARGET): pktriggercord-cli.c $(OBJS)
	$(CC) $(CLI_CFLAGS) $^ -DVERSION='"$(VERSION)"' -o $@ $(CLI_LDFLAGS) -L.

pslr_scsi.o: pslr_scsi_win.c pslr_scsi_linux.c pslr_scsi_openbsd.c

$(JSONDIR)/js0n.o: $(JSONDIR)/js0n.c $(JSONDIR)/js0n.h
	$(CC) $(LOCAL_CFLAGS) -fPIC -c $< -o $@

EXTERNAL=$(JSONDIR)/js0n.o

%.o: %.c %.h $(EXTERNAL)
	$(CC) $(LOCAL_CFLAGS) -DPKTDATADIR=\"$(PKTDATADIR)\" -fPIC -c $< -o $@

ifeq ($(ARCH),Win32)
$(GUI_TARGET): $(LOCALMINGW)/include $(LOCALMINGW)/lib
endif

$(GUI_TARGET): pktriggercord.c $(OBJS)
	$(CC) $(GUI_CFLAGS) -DVERSION='"$(VERSION)"' -DPKTDATADIR=\"$(PKTDATADIR)\" pktriggercord.c $(OBJS) -o $@ $(GUI_LDFLAGS) -L.

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
	rm -f pktriggercord pktriggercord-cli *.o $(JSONDIR)/*.o
	rm -f pktriggercord.exe pktriggercord-cli.exe
	rm -f *.orig

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

srcrpm: srczip
	install $(SPECFILE) $(TOPDIR)/SPECS/
	install $(SRCZIP) $(TOPDIR)/SOURCES/
	rpmbuild -bs $(SPECFILE)
	cp $(TOPDIR)/SRPMS/pktriggercord-$(VERSION)-1.src.rpm .

rpm: srcrpm
	rpmbuild -ba $(SPECFILE)
	cp $(TOPDIR)/RPMS/$(ARCH)/pktriggercord-$(VERSION)-1.$(ARCH).rpm .

WIN_CFLAGS=$(CFLAGS) -I$(WINMINGW)/include/gtk-2.0/ -I$(WINMINGW)/lib/gtk-2.0/include/ -I$(WINMINGW)/include/atk-1.0/ -I$(WINMINGW)/include/cairo/ -I$(WINMINGW)/include/gdk-pixbuf-2.0/ -I$(WINMINGW)/include/pango-1.0/
WIN_GUI_CFLAGS=$(WIN_CFLAGS) -I$(WINMINGW)/include/glib-2.0 -I$(WINMINGW)/lib/glib-2.0/include
WIN_LDFLAGS=-L$(WINMINGW)/lib -lgtk-win32-2.0 -lgdk-win32-2.0 -lgdk_pixbuf-2.0 -lgobject-2.0 -lglib-2.0 -lgio-2.0

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


# converting lens info from exiftool
exiftool_pentax:
	git archive --remote=git://git.code.sf.net/p/exiftool/code HEAD:lib/Image/ExifTool Pentax.pm | tar -x
	cat Pentax.pm | sed -n '/%pentaxLensTypes\ =/,/%pentaxModelID/p' | grep -v '^\s*#' | sed -e "s/[ ]*'\([0-9]\) \([0-9]\{1,3\}\)' => '\(.*\)',.*/{\1, \2, \"\3\"},/g;tx;d;:x" > exiftool_pentax_lens.txt
	rm Pentax.pm

pktriggercord_commandline.html: pktriggercord-cli.1
	cat $< | sed s/\\\\-/-/g | groff -man -Thtml -mwww -P "-lr" > $@

# Windows cross-compile
$(LOCALMINGW)/download: $(LOCALMINGW)/download/$(GTK_BUNDLE)

$(LOCALMINGW)/download/$(GTK_BUNDLE):
	mkdir -p $(LOCALMINGW)/download
	wget --no-check-certificate -N http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.24/$(GTK_BUNDLE) -P $(LOCALMINGW)/download

$(LOCALMINGW)/include $(LOCALMINGW)/lib: $(LOCALMINGW)/download
	unzip -o $(LOCALMINGW)/download/$(GTK_BUNDLE) -d $(LOCALMINGW) $(@:$(LOCALMINGW)/%=%)/**

# Extract all the required dlls at once with a grouped target
$(GUI_WIN_DLLS) &: $(LOCALMINGW)/download
	unzip -o $(LOCALMINGW)/download/$(GTK_BUNDLE) -d $(LOCALMINGW) $(GUI_WIN_DLLS:$(LOCALMINGW)/%=%)
	touch -r $^ $(GUI_WIN_DLLS)

ifeq ($(ARCH),Win32)
dist: pktriggercord_commandline.html $(GUI_WIN_DLLS) $(CLI_TARGET) $(GUI_TARGET)
	rm -rf $(WINDIR)
	mkdir -p $(WINDIR)
	cp $^ $(WINDIR)
	cp Changelog COPYING pktriggercord.ui pentax_settings.json $(WINDIR)
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
