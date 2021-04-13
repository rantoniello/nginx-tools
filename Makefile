# Define scripting
SHELL:=/bin/bash

# General directories definitions
PROJECT_DIR = $(shell readlink -f $(shell pwd)/$(dir $(lastword $(MAKEFILE_LIST))))
BUILD_DIR = $(PROJECT_DIR)/build

# Variables for Installation Directories defaults (following the GNU Makefile conventions)
PREFIX = $(PROJECT_DIR)/_install_dir
EXEC_PREFIX = $(PREFIX)
BINDIR = $(EXEC_PREFIX)/bin
LOCALSTATEDIR = $(PREFIX)/var
RUNSTATEDIR = $(LOCALSTATEDIR)/run
INCLUDEDIR = $(PREFIX)/include
LIBDIR = $(EXEC_PREFIX)/lib
DATAROOTDIR = $(PREFIX)/share

# Non conventional:
# SRCDIRS: A list of SRCDIR's (in the same target we compile sources in 
#          several locations)

# Exports
export PATH := $(BINDIR):$(PATH)

# Common compiler options
CPPFLAGS = -Wall -O3 -fPIC -I$(INCLUDEDIR) -DPROJECT_DIR=\"$(PROJECT_DIR)\" -DPREFIX=\"$(PREFIX)\"
CPP = c++
LDLIBS = -lm -ldl

.PHONY: all clean .foldertree nginx curl openssl

all: nginx

clean: .nginx_clean
	@rm -rf $(BUILD_DIR)
	@rm -rf $(PREFIX)

#Note: Use dot as a "trick" to hide this target from shell when user applies "autocomplete"
.foldertree:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(LIBDIR)
	@mkdir -p $(INCLUDEDIR)
	@mkdir -p $(BINDIR)
	@mkdir -p $(PREFIX)/etc
	@mkdir -p $(PREFIX)/certs

##############################################################################
# Rule for 'test-rate-limiting' program
##############################################################################

test-rate-limiting: nginx curl
	@$(MAKE) test-rate-limiting-generic-build-install --no-print-directory \
SRCDIRS=$(PROJECT_DIR)/test-rate-limiting _BUILD_DIR=$(BUILD_DIR)/$@ TARGETFILE=$(BUILD_DIR)/$@/$@.bin \
CXXFLAGS='$(CPPFLAGS) -std=c++11' CFLAGS='$(CPPFLAGS)' \
LDFLAGS+='-L$(LIBDIR)' LDLIBS+='-lpthread -lssl -lcrypto' || exit 1

##############################################################################
# Rule for 'OpenSSL' library and apps.
##############################################################################

OPENSSL_SRCDIRS = $(PROJECT_DIR)/3rdplibs/openssl
.ONESHELL: # Note: ".ONESHELL" only applies to 'openssl' target
openssl: | .foldertree
	@$(eval _BUILD_DIR := $(BUILD_DIR)/$@)
	@mkdir -p "$(_BUILD_DIR)"
	@if [ ! -f "$(_BUILD_DIR)"/Makefile ] ; then \
		echo "Configuring $@..."; \
		cd "$(_BUILD_DIR)" && "$(OPENSSL_SRCDIRS)"/config --prefix="$(PREFIX)" --openssldir="$(PREFIX)" || exit 1; \
	fi
	@$(MAKE) -C "$(_BUILD_DIR)" -j1 install_sw || exit 1

##############################################################################
# Rule for 'curl' library
##############################################################################

CURL_SRCDIRS = $(PROJECT_DIR)/3rdplibs/curl
.ONESHELL:
curl: | .foldertree
	@$(eval _BUILD_DIR := $(BUILD_DIR)/$@)
	@mkdir -p "$(_BUILD_DIR)"
	@if [ ! -f "$(_BUILD_DIR)"/Makefile ] ; then \
		echo "Configuring $@..."; \
		cd "$(_BUILD_DIR)" && "$(CURL_SRCDIRS)"/configure --prefix="$(PREFIX)" --srcdir="$(CURL_SRCDIRS)" \
--enable-static=no || exit 1; \
	fi
	@$(MAKE) -C "$(_BUILD_DIR)" install || exit 1

##############################################################################
# Rule for 'nginx'.
##############################################################################

NGINX_SRCDIRS = $(PROJECT_DIR)/3rdplibs/nginx
.ONESHELL: # Note: ".ONESHELL" only applies to target immediately below
nginx: | .foldertree openssl
	@$(eval _BUILD_DIR := $(BUILD_DIR)/$@)
	@mkdir -p "$(_BUILD_DIR)"
	@if [ ! -f "$(_BUILD_DIR)"/Makefile ] ; then \
		echo "Configuring $@..."; \
		cd "$(NGINX_SRCDIRS)" && "$(NGINX_SRCDIRS)"/configure\
 --prefix="$(PREFIX)"\
 --builddir="$(_BUILD_DIR)"\
 --with-compat\
 --with-debug\
 --with-select_module\
 --with-poll_module\
 --with-threads\
 --with-http_ssl_module\
 --with-http_v2_module\
 --with-http_realip_module\
 --with-http_addition_module\
 --with-http_gunzip_module\
 --with-http_gzip_static_module\
 --with-http_auth_request_module\
 --with-http_random_index_module\
 --with-http_secure_link_module\
 --with-http_degradation_module\
 --with-http_slice_module\
 --with-http_stub_status_module\
 || exit 1; \
	fi
	@$(MAKE) -C "$(NGINX_SRCDIRS)" install || exit 1

.nginx_clean:
	@rm -rf "$(NGINX_SRCDIRS)"/Makefile

##############################################################################
# Generic rules to compile and install any library or app. from source
############################################################################## 
# Implementation note: 
# -> ’%’ below is used to write target with wildcards (e.g. ‘%-build’ matches all called targets with this pattern);
# -> ‘$*’ takes exactly the value of the substring matched by ‘%’ in the correspondent target itself. 
# For example, if the main Makefile performs 'make mylibname-build', ’%’ will match 'mylibname' and ‘$*’ will exactly 
# take the value 'mylibname'.

%-generic-build-install: %-generic-build-source-compile
	@echo Installing target "$(TARGETFILE)" to "$(DESTFILE)";
	@echo ====================================2
	@if [ ! -z "$(INCLUDEFILES)" ] ; then\
		mkdir -p $(INCLUDEDIR)/$*;\
		cp -f $(INCLUDEFILES) $(INCLUDEDIR)/$*/;\
	fi
	@echo ====================================3
	cp -f $(TARGETFILE) $(DESTFILE) || exit 1
	@echo ====================================4

find_cfiles = $(wildcard $(shell realpath --relative-to=$(PROJECT_DIR) $(dir)/*.c))
find_cppfiles = $(wildcard $(shell realpath --relative-to=$(PROJECT_DIR) $(dir)/*.cpp))
get_cfiles_objs = $(patsubst %.c,$(_BUILD_DIR)/%.o,$(find_cfiles))
get_cppfiles_objs = $(patsubst %.cpp,$(_BUILD_DIR)/%.oo,$(find_cppfiles))

%-generic-build-source-compile:
	@echo "Building target '$*' from sources... (make $@)"
	@echo "Target file $(TARGETFILE)";
	@echo "Source directories: $(SRCDIRS)";
	@mkdir -p "$(_BUILD_DIR)"
	$(eval cobjs := $(foreach dir,$(SRCDIRS),$(get_cfiles_objs)))
	$(eval cppobjs := $(foreach dir,$(SRCDIRS),$(get_cppfiles_objs)))
	$(MAKE) $(TARGETFILE) objs="$(cobjs) $(cppobjs)" --no-print-directory;
	@echo Finished building target '$*'

$(TARGETFILE): $(objs)
	$(CPP) -o $@ $^ $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)

.PRECIOUS: $(_BUILD_DIR)%/.
$(_BUILD_DIR)%/.:
	mkdir -p $@

.SECONDEXPANSION:

# Note: The "$(@D)" expands to the directory part of the target path. 
# We escape the $(@D) reference so that we expand it only during the second expansion.
$(_BUILD_DIR)/%.o: $(PROJECT_DIR)/%.c | $$(@D)/.
	$(CPP) -c -o $@ $< $(CFLAGS) $(LDFLAGS) $(LDLIBS)

$(_BUILD_DIR)/%.oo: $(PROJECT_DIR)/%.cpp | $$(@D)/.
	$(CPP) -c -o $@ $< $(CXXFLAGS) $(LDFLAGS) $(LDLIBS)
