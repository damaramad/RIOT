PKG_NAME    = libpip
PKG_URL     = https://github.com/damaramad/libpip.git
PKG_VERSION = 06d9437c7208bf2a31d03635b289036e2236daad
PKG_LICENSE = CeCILL-2.1

include $(RIOTBASE)/pkg/pkg.mk

all:
# Create an empty directory in the BINDIR directory to avoid the
# following error message: "using unknown modules: libpip". This
# is because the directory is not created automatically, since
# there are, for the moment, only header files and no compiled
# object files.
	mkdir -p $(BINDIR)/libpip
