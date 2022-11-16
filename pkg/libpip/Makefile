PKG_NAME    = libpip
PKG_URL     = https://github.com/damaramad/libpip.git
PKG_VERSION = 949f549620ecb2a8c961572be4bf06816cb99023
PKG_LICENSE = CeCILL-2.1

include $(RIOTBASE)/pkg/pkg.mk

all:
# Create an empty directory in the BINDIR directory to avoid the
# following error message: "using unknown modules: libpip". This
# is because the directory is not created automatically, since
# there are, for the moment, only header files and no compiled
# object files.
	$(Q)mkdir -p $(BINDIR)/libpip
