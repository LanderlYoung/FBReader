include $(ROOTDIR)/makefiles/arch/unix.mk

DESTDIR ?= /Applications

INSTALLDIR = /FBReader.app
BINDIR = $(INSTALLDIR)/Contents/MacOS
SHAREDIR = $(INSTALLDIR)/Contents/Share
IMAGEDIR = $(SHAREDIR)/icons
APPIMAGEDIR = $(SHAREDIR)/icons

SHAREDIR_MACRO = ~~/Contents/Share
IMAGEDIR_MACRO = $(SHAREDIR_MACRO)/icons
APPIMAGEDIR_MACRO = $(SHAREDIR_MACRO)/icons

ZLSHARED = no

SYSROOT = $(xcode-select -p)
SYSROOT=/Applications/Xcode.app/Contents/Developer
SYSROOT=/
TOOLSPATH = $(SYSROOT)/usr/bin
CC = $(TOOLSPATH)/gcc
AR = /usr/bin/ar rsu
LD = $(TOOLSPATH)/g++

MACOS_VERSION = 10.14

#ARCH_FLAGS = -arch x86_64 -arch i386 -arch ppc7400 -arch ppc64
ARCH_FLAGS = -arch x86_64
CFLAGS_NOARCH = \
	-fmessage-length=0 -pipe -fpascal-strings -fasm-blocks \
	-mdynamic-no-pic -W -Wall \
	-isysroot $(SYSROOT) \
	-mmacosx-version-min=$(MACOS_VERSION) \
	-gdwarf-2 \
	-stdlib=libc++
	#-fvisibility=hidden -fvisibility-inlines-hidden \
CFLAGS = $(ARCH_FLAGS) $(CFLAGS_NOARCH)
LDFLAGS = $(ARCH_FLAGS) \
	-mmacosx-version-min=$(MACOS_VERSION) \
	-isysroot $(SYSROOT)
EXTERNAL_INCLUDE = -I/usr/local/include -I$(ROOTDIR)/libs/macosx/include
EXTERNAL_LIBS = -L$(ROOTDIR)/libs/macosx/lib    \
                -L/usr/lib                      \
                -liconv -llinebreak -lfribidi

UILIBS = -framework Cocoa $(ROOTDIR)/zlibrary/ui/src/cocoa/application/CocoaWindow.o $(ROOTDIR)/zlibrary/ui/src/cocoa/library/ZLCocoaAppDelegate.o

RM = rm -rvf
RM_QUIET = rm -rf
