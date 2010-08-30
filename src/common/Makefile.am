AM_CFLAGS = \
	$(PULSEAUDIO_CFLAGS)

modlibexec_LTLIBRARIES = libmeego-common.la

libmeego_common_la_SOURCES = \
	optimized.c optimized.h \
	pa-optimized.c pa-optimized.h \
	memory.c memory.h \
	parameter-hook.c parameter-hook.h \
	algorithm-hook.c algorithm-hook.h \
	proplist-meego.h

libmeego_common_la_LDFLAGS = -avoid-version
libmeego_common_la_LIBADD = $(PULSEAUDIO_LIBS)

if X86
libmeego_common_la_SOURCES += \
	src-8-to-48.c \
	src-48-to-8.c \
	src-16-to-48.c \
	src-48-to-16.c
else
libmeego_common_la_LIBADD += \
	src-8-to-48-neon.o \
	src-48-to-8-neon.o \
	src-16-to-48-neon.o \
	src-48-to-16-neon.o
if ARM_HARDFP
%-neon.o: speech_src_neon_hardfp.a
	ar x speech_src_neon_hardfp.a
else
%-neon.o: speech_src_neon.a
	ar x speech_src_neon.a
endif
endif

TESTS = check_common
check_PROGRAMS = check_common
check_common_SOURCES = tests.c
check_common_LDADD = libmeego-common.la $(CHECK_LIBS)
check_common_CFLAGS = $(CHECK_CFLAGS)

