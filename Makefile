JSONDIR=src/external/js0n

PREFIX ?= /usr/local
PKTDATADIR = $(PREFIX)/share/pktriggercord
CFLAGS ?= -O3 -g -Wall -I$(JSONDIR)
# -Wextra
LDFLAGS ?= -lm

MANDIR = $(PREFIX)/share/man
MAN1DIR = $(MANDIR)/man1

LIN_CFLAGS = $(CFLAGS)
LIN_LDFLAGS = $(LDFLAGS)

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
ARCH=$(shell uname -m)

#variables for Android
ANDROID=android
ANDROID_DIR = android
ANDROID_PROJECT_NAME = PkTriggerCord
ANDROID_PACKAGE = info.melda.sala.pktriggercord
APK_FILE = $(PROJECT_NAME)-debug.apk
ifndef ANDROID_NDK_HOME
NDK_BUILD=ndk-build
else
NDK_BUILD = $(ANDROID_NDK_HOME)/ndk-build
endif

LIN_GUI_LDFLAGS=$(shell pkg-config --libs gtk+-2.0 gmodule-2.0)
LIN_GUI_CFLAGS=$(CFLAGS) $(shell pkg-config --cflags gtk+-2.0 gmodule-2.0) -DGTK_DISABLE_SINGLE_INCLUDES -DGSEAL_ENABLE
#-DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED

default: cli pktriggercord
all: srczip rpm win pktriggercord_commandline.html
cli: pktriggercord-cli

MANS = pktriggercord-cli.1 pktriggercord.1
SRCOBJNAMES = pslr pslr_enum pslr_scsi pslr_lens pslr_model pktriggercord-servermode
OBJS = $(SRCOBJNAMES:=.o) $(JSONDIR)/js0n.o
WIN_DLLS_DIR=win_dlls
SOURCE_PACKAGE_FILES = Makefile Changelog COPYING INSTALL BUGS $(MANS) pentax_scsi_protocol.md pentax.rules samsung.rules $(SRCOBJNAMES:=.h) $(SRCOBJNAMES:=.c) pslr_scsi_linux.c pslr_scsi_win.c pslr_scsi_openbsd.c exiftool_pentax_lens.txt pktriggercord.c pktriggercord-cli.c pktriggercord.ui pentax_settings.json $(SPECFILE) android_scsi_sg.h src/
TARDIR = pktriggercord-$(VERSION)
SRCZIP = pkTriggerCord-$(VERSION).src.tar.gz

LOCALMINGW=i686-w64-mingw32
WINGCC=i686-w64-mingw32-gcc
WINMINGW=/usr/i686-w64-mingw32/sys-root/mingw
WINDIR=$(TARDIR)-win

pslr.o: pslr_enum.o pslr_scsi.o pslr.c pslr.h

pktriggercord-cli: pktriggercord-cli.c $(OBJS)
	$(CC) $(LIN_CFLAGS) $^ -DVERSION='"$(VERSION)"' -o $@ $(LIN_LDFLAGS) -L.

pslr_scsi.o: pslr_scsi_win.c pslr_scsi_linux.c pslr_scsi_openbsd.c

$(JSONDIR)/js0n.o : $(JSONDIR)/js0n.c $(JSONDIR)/js0n.h
	$(CC) $(LIN_CFLAGS) -fPIC -c $< -o $@

external: $(JSONDIR)/js0n.o

%.o : %.c %.h external
	$(CC) $(LIN_CFLAGS) -DPKTDATADIR=\"$(PKTDATADIR)\" -fPIC -c $< -o $@

pktriggercord: pktriggercord.c $(OBJS)
	$(CC) $(LIN_GUI_CFLAGS) -DVERSION='"$(VERSION)"' -DPKTDATADIR=\"$(PKTDATADIR)\" $^ $(LIN_LDFLAGS) -o $@ $(LIN_GUI_LDFLAGS) -L.

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
windownload:
	mkdir -p $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.24/gtk+_2.24.10-1_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.24/gtk+-dev_2.24.10-1_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.28/glib-dev_2.28.8-1_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/atk/1.32/atk-dev_1.32.0-2_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/pango/1.29/pango-dev_1.29.4-1_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/gdk-pixbuf/2.24/gdk-pixbuf-dev_2.24.0-1_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/gdk-pixbuf/2.24/gdk-pixbuf_2.24.0-1_win32.zip -P $(LOCALMINGW)/download
	wget -N http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/cairo-dev_1.10.2-2_win32.zip -P $(LOCALMINGW)/download
	unzip -o $(LOCALMINGW)/download/gtk+_2.24.10-1_win32.zip -d $(LOCALMINGW)
	unzip -o $(LOCALMINGW)/download/gtk+-dev_2.24.10-1_win32.zip -d $(LOCALMINGW)
	unzip -o $(LOCALMINGW)/download/glib-dev_2.28.8-1_win32.zip -d $(LOCALMINGW)
	unzip -o $(LOCALMINGW)/download/atk-dev_1.32.0-2_win32.zip -d $(LOCALMINGW)
	unzip -o $(LOCALMINGW)/download/pango-dev_1.29.4-1_win32.zip -d $(LOCALMINGW)
	unzip -o $(LOCALMINGW)/download/gdk-pixbuf-dev_2.24.0-1_win32.zip -d $(LOCALMINGW)
	unzip -o $(LOCALMINGW)/download/gdk-pixbuf_2.24.0-1_win32.zip -d $(LOCALMINGW)
	unzip -o $(LOCALMINGW)/download/cairo-dev_1.10.2-2_win32.zip -d $(LOCALMINGW)
	wget -N https://downloads.sourceforge.net/project/mingw-w64/Toolchains%20targetting%20Win32/Personal%20Builds/rubenvb/gcc-4.7-release/i686-w64-mingw32-gcc-4.7.4-release-linux64_rubenvb.tar.xz -P $(LOCALMINGW)/download
	tar xJf $(LOCALMINGW)/download/i686-w64-mingw32-gcc-4.7.4-release-linux64_rubenvb.tar.xz -C $(LOCALMINGW)

localwin: WINMINGW=$(LOCALMINGW)
localwin: WINGCC=$(LOCALMINGW)/mingw32/bin/i686-w64-mingw32-gcc

winobjs:$(SRCOBJNAMES:=.c) 
	$(foreach srcfile, $(SRCOBJNAMES:=.c), $(WINGCC) -DVERSION='"$(VERSION)"' -DPKTDATADIR=\".\" $(WIN_CFLAGS) -c $(srcfile);)

winexternal: $(JSONDIR)/js0n.c $(JSONDIR)/js0n.h
	$(WINGCC) $(WIN_CFLAGS) -c $< -o $(JSONDIR)/js0n.o

win-cli:winobjs winexternal pktriggercord-cli.c pktriggercord_commandline.html
	$(WINGCC) -mms-bitfields -DVERSION='"$(VERSION)"' pktriggercord-cli.c $(OBJS) -o pktriggercord-cli.exe $(WIN_CFLAGS) -L.
	mkdir -p $(WINDIR)
	cp pktriggercord-cli.exe Changelog COPYING pktriggercord_commandline.html $(WINDIR)
	cp $(WIN_DLLS_DIR)/*.dll $(WINDIR)

win-gui: winobjs
	$(WINGCC) -mms-bitfields -DVERSION='"$(VERSION)"' -DPKTDATADIR=\".\" pktriggercord.c $(OBJS) -o pktriggercord.exe $(WIN_GUI_CFLAGS) $(WIN_LDFLAGS) -L.
	mkdir -p $(WINDIR)
	cp pktriggercord.exe pktriggercord.ui pentax_settings.json Changelog COPYING $(WINDIR)
	cp $(WIN_DLLS_DIR)/*.dll $(WINDIR)

win: win-cli win-gui
	rm -f $(WINDIR).zip
	zip -rj $(WINDIR).zip $(WINDIR)
	rm -r $(WINDIR)

localwin: windownload win

androidcli:
	VERSION=$(VERSION) NDK_PROJECT_PATH=$(ANDROID_DIR) NDK_DEBUG=1 $(NDK_BUILD)

androidclean:
	VERSION=$(VERSION) NDK_PROJECT_PATH=$(ANDROID_DIR) NDK_DEBUG=1 $(NDK_BUILD) clean
	cd $(ANDROID_DIR) && ./gradlew clean

androidver:
	sed -i s/versionName\ \".*\"/versionName\ \"$(VERSION)\"/ $(ANDROID_DIR)/build.gradle
	sed -i s/versionCode\ .*/versionCode\ $(VERSIONCODE)/ $(ANDROID_DIR)/build.gradle

androidcommon: androidcli androidver
	mkdir -p $(ANDROID_DIR)/assets
	cp $(ANDROID_DIR)/libs/armeabi/pktriggercord-cli $(ANDROID_DIR)/assets

android: androidcommon
	cd $(ANDROID_DIR) && ./gradlew assembleDebug
	cp $(ANDROID_DIR)/build/outputs/apk/debug/$(ANDROID_PACKAGE).$(ANDROID_PROJECT_NAME)-$(VERSION)-debug.apk .
	echo "android build is EXPERIMENTAL. Use it at your own risk"

androidrelease: androidcommon
	cd $(ANDROID_DIR) && ./gradlew assembleRelease --no-daemon
	cp $(ANDROID_DIR)/build/outputs/apk/release/$(ANDROID_PACKAGE).$(ANDROID_PROJECT_NAME)-$(VERSION)-release.apk .
	echo "android build is EXPERIMENTAL. Use it at your own risk"

astyle:
	astyle --options=astylerc *.h *.c
