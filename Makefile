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
deb:
	@echo "Building Debian Packages"
	@echo "->Generating Changelog from commit log"
	@gbp dch
	@sed -i -e 's|Mubashshir Ahmad Hasan <mubashshir@mubashshir-mint>|Ahmad Hasan Mubashshir <ahmubashshir@gmail.com>|g' debian/changelog
	@git add .
	@git commit -m "Packaging Now"
	@echo "->Started Building packages"
	@gbp buildpackage --git-tag --git-retag --git-sign-tags -kb5ac9a5e093f60a29aa1692f4cc9a8013a65e7d4
clean:
	make -C src/ctrtool clean
install:
	$(INSTALL) -m 755 -D src/3ds-thumbnailer $(PREFIX)/bin/3ds-thumbnailer
	$(INSTALL) -m 755 -D src/3ds-thumbnail $(PREFIX)/bin/3ds-thumbnail
	$(INSTALL) -m 755 -sD src/ctrtool/ctrtool $(PREFIX)/bin/ctrtool
	$(INSTALL) -m 644 -D src/3ds.thumbnailer $(PREFIX)/share/thumbnailers/3ds.thumbnailer
	$(INSTALL) -m 644 -D src/x-3ds-rom.xml $(PREFIX)/share/mime/packages/x-3ds-rom.xml
	
