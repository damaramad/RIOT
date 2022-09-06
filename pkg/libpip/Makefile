PKG_NAME    = libpip
PKG_URL     = https://github.com/damaramad/libpip.git
PKG_VERSION = ceacc4e0d36bd8df20c68dbfde9021738aaa48ee
PKG_LICENSE = CeCILL-2.1

include $(RIOTBASE)/pkg/pkg.mk

all:
# Create an empty directory in the BINDIR directory to avoid the
# following error message: "using unknown modules: libpip". This
# is because the directory is not created automatically, since
# there are, for the moment, only header files and no compiled
# object files.
	mkdir -p $(BINDIR)/libpip
