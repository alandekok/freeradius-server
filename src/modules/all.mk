#
#  Changes the behaviour of autoconf.h to undef definitions that would conflict
#  with module config.h files.
#
CFLAGS += -DIS_MODULE=1

#
#  If we haven't run configure, ignore the modules which require it.
#  Otherwise, load in all of the module makefiles, including ones
#  which have not yet been configured.  We do the "sort" to remove
#  duplicates.
#
ifeq "$(CONFIGURE_ARGS)" ""
SUBMAKEFILES := $(wildcard ${top_srcdir}/src/modules/rlm_*/all.mk)
else
SUBMAKEFILES := $(sort $(wildcard ${top_srcdir}/src/modules/rlm_*/all.mk) \
		$(patsubst %.in,%,$(wildcard ${top_srcdir}/src/modules/rlm_*/all.mk.in)))
endif

SUBMAKEFILES += $(wildcard ${top_srcdir}/src/modules/proto_*/all.mk)

#
#  Remove CHAP and MS-CHAP, and Digest authentication.
#
#  For now, also remove EAP, until we get around to fixing it.
#
#  Also remove rlm_ippool, as it uses an MD5 hash to map keys to values.
#  That should be changed to SHA256 or something similar.
#
ifneq "$(SRADIUS_ONLY)" ""
SUBMAKEFILES := $(filter-out %chap/all.mk, $(SUBMAKEFILES))
SUBMAKEFILES := $(filter-out %rlm_digest/all.mk, $(SUBMAKEFILES))
SUBMAKEFILES := $(filter-out %/rlm_eap/all.mk, $(SUBMAKEFILES))
SUBMAKEFILES := $(filter-out %/rlm_ippool/all.mk, $(SUBMAKEFILES))
endif

ifeq "$(MAKECMDGOALS)" "reconfig"
src/modules/%/configure: src/modules/%/configure.ac $(wildcard $(top_builddir)/m4/*.m4)
	@echo AUTOCONF $(dir $@)
	@cd $(dir $@) && \
		$(ACLOCAL) --force -I $(top_builddir) -I $(top_builddir)/m4 && \
		$(AUTOCONF) --force
endif
