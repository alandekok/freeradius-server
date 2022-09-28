TARGET		:= radclient
SOURCES		:= radclient.c

ifeq "$(SRADIUS_ONLY)" ""
SOURCES		+= ${top_srcdir}/src/modules/rlm_mschap/smbdes.c \
		   ${top_srcdir}/src/modules/rlm_mschap/mschap.c
endif

TGT_PREREQS	:= libfreeradius-radius.a

SRC_CFLAGS	:= -I${top_srcdir}/src/modules/rlm_mschap
TGT_LDLIBS	:= $(LIBS)
