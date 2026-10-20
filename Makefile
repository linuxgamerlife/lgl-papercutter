NAME := lgl-papercutter
SPEC := packaging/$(NAME).spec
VERSION := $(shell awk '/^Version:/ { print $$2; exit }' $(SPEC))
RESULTS_DIR := $(CURDIR)/results
SOURCE_ARCHIVE := $(RESULTS_DIR)/$(NAME)-$(VERSION).tar.gz
SOURCE_FILES := .copr .github .gitignore CHANGELOG.md CMakeLists.txt HELP.md LICENSE Makefile README.md \
	data docs packaging src tests
.DEFAULT_GOAL := srpm

# COPR SCM passes its result directory as `outdir`. Local builds default to
# placing RPM artifacts in ./results.
outdir ?= $(RESULTS_DIR)
RPMBUILD ?= rpmbuild

.PHONY: source srpm clean

source:
	@test -n "$(VERSION)" || { echo "Could not read Version from $(SPEC)"; exit 1; }
	mkdir -p $(RESULTS_DIR)
	tar --create --gzip \
		--file=$(SOURCE_ARCHIVE).tmp \
		--sort=name \
		--owner=0 --group=0 --numeric-owner \
		--exclude='legacy' --exclude='build' --exclude='build-*' \
		--transform='s,^,$(NAME)-$(VERSION)/,' \
		$(SOURCE_FILES)
	mv $(SOURCE_ARCHIVE).tmp $(SOURCE_ARCHIVE)

srpm: source
	mkdir -p $(outdir)
	$(RPMBUILD) -bs $(SPEC) \
		--define "_sourcedir $(RESULTS_DIR)" \
		--define "_srcrpmdir $(abspath $(outdir))"

clean:
	rm -f $(SOURCE_ARCHIVE) $(SOURCE_ARCHIVE).tmp
