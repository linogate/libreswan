BSD_VARIANT=netbsd
# sketch out pkgsrc
PKG_BASE ?= /usr/pkg
PKG_DBDIR ?= /var/db/pkg
PKG_PATH ?= /usr/pkgsrc/packages/All

USERLAND_CFLAGS += -DHAS_SUN_LEN
USERLAND_CFLAGS += -DNEED_SIN_LEN
USERLAND_CFLAGS += -DHAVE_NETINET6_IN_H

USERLAND_INCLUDES += -I$(PKG_BASE)/include

USERLAND_LDFLAGS += -L$(PKG_BASE)/lib -Wl,-rpath,$(PKG_BASE)/lib

# NSS includes more than needed in ldflags
NSS_LDFLAGS = -L$(PKG_BASE)/lib/nss -Wl,-rpath,$(PKG_BASE)/lib/nss -lnss3 -lfreebl3 -lssl3
NSPR_LDFLAGS = -L$(PKG_BASE)/lib/nspr -Wl,-rpath,$(PKG_BASE)/lib/nspr -lnspr4

CRYPT_LDFLAGS =

USE_BSDKAME = true
USE_LIBCAP_NG = false

INITSYSTEM=rc.d

# not /run/pluto
FINALRUNDIR=/var/run/pluto

# PREFIX = /usr/local from mk/config.mk
FINALSYSCONFDIR=$(PREFIX)/etc
FINALNSSDIR=$(PREFIX)/etc/ipsec.d
FINALEXAMPECONFDIR=$(PREFIX)/share/examples/libreswan
