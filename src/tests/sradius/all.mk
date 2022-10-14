#
#	Test name
#
TEST  := tests.sradius
FILES := $(subst $(DIR)/,,$(wildcard $(DIR)/*.txt))

BUILD_PATH      := $(top_builddir)/build
TEST_PATH       := $(top_builddir)/src/tests/sradius
BIN_PATH        := $(BUILD_PATH)/bin/local
LIB_PATH        := $(BUILD_PATH)/lib/.libs/
RADDB_PATH      := $(top_builddir)/raddb

ifneq "$(OPENSSL_LIBS)" ""

# Port defines for proxy and sradius
port_radiusd_auth 	:= 12340
port_sradiusd_auth 	:= 12341

export port_radiusd_auth port_sradiusd_auth TEST_PATH LIB_PATH

#
#  Run the full tests
#
$(eval $(call TEST_BOOTSTRAP))

#
#  Generic rules to start / stop the radius service.
#
#  TODO: We need to fix the src/tests/radiusd.mk
#  to be able spin up multiple instances
#$(eval $(call RADIUSD_SERVICE,radiusd,$(OUTPUT)))
define START_SERVER
	$(eval RADIUSD_RUN := $(BIN_PATH)/radiusd -xxx -l $(TEST_PATH)/$(1).log -D $(top_builddir)/share -d $(TEST_PATH) -n $(1))
	$(Q)printf "SRADIUS-TEST Starting $(1) server... "
	$(Q)if ! $(RADIUSD_RUN); then \
		echo "failed"; \
		echo "Last log entries were:"; \
		tail -n 20 "$(TEST_PATH)/$(1).log"; \
	else \
		echo "ok"; \
	fi
endef

define STOP_SERVER
	$(Q)if [ -f $(TEST_PATH)/$(1).pid ]; then \
		if ! ps `cat $(TEST_PATH)/$(1).pid` >/dev/null 2>&1; then \
			rm -f $(TEST_PATH)/$(1).pid; \
			echo "FreeRADIUS terminated during test"; \
			echo "GDB output was:"; \
			cat "$(TEST_PATH)/gdb-$(1).log"; \
			echo "Last log entries were:"; \
			tail -n 20 $(TEST_PATH)/$(1).log; \
		fi; \
		if ! kill -TERM `cat $(TEST_PATH)/$(1).pid` >/dev/null 2>&1; then \
			echo "Cannot kill $(TEST_PATH)/$(1).pid"; \
		fi; \
	fi
	$(Q)rm -f $(TEST_PATH)/$(1).pid
endef

#
#  Ensure that the certificate files are created.
#
$(RADDB_PATH)/certs/server.crt: $(RADDB_PATH)/certs/server.cnf
	@$(MAKE) -C $(RADDB_PATH)/certs

# radiusd
test.sradius.start_server: $(RADDB_PATH)/certs/server.crt
	$(call START_SERVER,radiusd)
	$(call START_SERVER,sradiusd)

test.sradius.stop_server:
	$(call STOP_SERVER,radiusd)
	$(call STOP_SERVER,sradiusd)

#
# kill the server (if it's running)
# start the server
# run the tests
# kill the server
#
$(OUTPUT)/%: $(DIR)/% test.sradius.stop_server test.sradius.start_server
	$(Q)echo "SRADIUS-TEST Authenticating"
	$(Q)mkdir -p $(dir $@)
	$(Q)if ! $(TESTBIN)/radclient -xxxf $(TEST_PATH)/auth.txt -D share/ 127.0.0.1:$(port_radiusd_auth) auth $(SECRET) 1> $(TEST_PATH)/radclient.log 2>&1; then \
		echo "FAILED";                                         \
		rm -f $(BUILD_DIR)/tests/test.sradius;                 \
		$(MAKE) --no-print-directory test.sradius.stop_server; \
		echo ==============================;                   \
		tail -10 $(TEST_PATH)/radclient.log;                   \
		echo ==============================;                   \
		echo "RADIUSD: $(RADIUSD_RUN)";                        \
		echo "SRADIUS: $(TESTBIN)/radclient -xf $(TEST_PATH)/auth.txt -D share/ 127.0.0.1:$(port_radiusd_auth) auth $(SECRET)"; \
		exit 1;                                                \
	fi

	$(Q)touch $@

$(TEST):
	$(Q)$(MAKE) --no-print-directory test.sradius.stop_server
	@touch $(BUILD_DIR)/tests/$@

else
#
#  No TLS
#
.PHONY: $(TEST)
$(TEST):
	@echo SRADIUS is not available in this executable
endif
