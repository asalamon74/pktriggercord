PREFIX ?= /usr/local
CFLAGS ?= -O3 -g -Wall

GUI_LDFLAGS=$(shell pkg-config --libs gtk+-2.0 libglade-2.0)
GUI_CFLAGS=$(shell pkg-config --cflags gtk+-2.0 libglade-2.0)

all: cli pktriggercord
cli: pktriggercord-cli
install: install-app

OBJS = pslr.o pslr_scsi.o

pslr.o: pslr_scsi.o pslr.c pslr.h

pktriggercord-cli: pktriggercord-cli.c $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@ -L. 

debug: CFLAGS+=-DDEBUG 
GUI_CFLAGS+=-DDEBUG

debug: all

%.o : %.c %.h
	$(CC) $(CFLAGS) -fPIC -c $<

pktriggercord: pktriggercord.c $(OBJS)
	$(CC) $(GUI_CFLAGS) $(GUI_LDFLAGS) -DDATADIR=\"$(PREFIX)/share/pktriggercord\" $? $(LDFLAGS) -o $@ -L. 

install-app:
	install -d $(PREFIX)/bin
	install -s -m 0755 pktriggercord-cli $(PREFIX)/bin/
	install -m 0644 pentax.rules /etc/udev/
	install -m 0644 samsung.rules /etc/udev/
	cd /etc/udev/rules.d;\
	ln -sf ../pentax.rules 025_pentax.rules;\
	ln -sf ../samsung.rules 025_samsung.rules
	if [ -e ./pktriggercord ] ; then \
	install -s -m 0755 pktriggercord $(PREFIX)/bin/; \
	install -d $(PREFIX)/share/pktriggercord/; \
	install -m 0644 pktriggercord.glade $(PREFIX)/share/pktriggercord/ ; \
	fi

clean:
	rm -f pktriggercord pktriggercord-cli *.o

uninstall:
	rm -f $(PREFIX)/bin/pktriggercord $(PREFIX)/bin/pktriggercord-cli
	rm -rf $(PREFIX)/share/pktriggercord
	rm -f /etc/udev/pentax.rules
	rm -f /etc/udev/rules.d/025_pentax.rules
	rm -f /etc/udev/samsung.rules
	rm -f /etc/udev/rules.d/025_samsung.rules

