PREFIX?=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/man

all: semfsctl-sync

semfsctl-sync: semfsctl-sync.c
	cc -Wall -o semfsctl-sync semfsctl-sync.c

install:
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 semfsctl-sync $(DESTDIR)$(BINDIR)/semfsctl-sync

install-man:
	install -d $(DESTDIR)$(MANDIR)/man4 $(DESTDIR)$(MANDIR)/man8
	install -m 644 ../man/man4/semfs.4 $(DESTDIR)$(MANDIR)/man4/
	install -m 644 ../man/man8/semfsctl-sync.8 $(DESTDIR)$(MANDIR)/man8/