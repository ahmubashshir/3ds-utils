ifneq ($(DESTDIR),)
	PREFIX=$(DESTDIR)/usr
else
	ifeq ($(PREFIX),)
		PREFIX=/usr/local
	endif
endif
ifneq ($(DESTDIR),)
	CONFIGDIR=$(DESTDIR)/etc
else
	CONFIGDIR=/etc
endif
ifeq ($(INSTALL),)
	INSTALL=install
endif
all: ctrtool
ctrtool:
	make -C src/ctrtool
clean:
	make -C src/ctrtool clean
install:
	$(INSTALL) -m 755 -D src/3ds-thumbnailer $(PREFIX)/bin/3ds-thumbnailer
	$(INSTALL) -m 755 -D src/3ds-thumbnail $(PREFIX)/bin/3ds-thumbnail
	$(INSTALL) -m 755 -sD src/ctrtool/ctrtool $(PREFIX)/bin/ctrtool
	$(INSTALL) -m 644 -D src/3ds.thumbnailer $(PREFIX)/share/thumbnailers/3ds.thumbnailer
	$(INSTALL) -m 644 -D src/x-3ds-rom.xml $(PREFIX)/share/mime/packages/x-3ds-rom.xml
	
