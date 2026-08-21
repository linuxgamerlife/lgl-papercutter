NAME := lgl-papercutter
SPEC := packaging/$(NAME).spec
VERSION := $(shell awk '/^Version:/ { print $$2; exit }' $(SPEC))
SOURCE_ARCHIVE := $(NAME)-$(VERSION).tar.gz
.DEFAULT_GOAL := srpm

# COPR SCM passes its result directory as `outdir`. Local builds default to
# placing the SRPM in ./result.
outdir ?= $(CURDIR)/result
RPMBUILD ?= rpmbuild

.PHONY: source srpm clean

source:
	@test -n "$(VERSION)" || { echo "Could not read Version from $(SPEC)"; exit 1; }
	git archive --format=tar.gz \
		--prefix=$(NAME)-$(VERSION)/ \
		--output=$(SOURCE_ARCHIVE).tmp HEAD
	mv $(SOURCE_ARCHIVE).tmp $(SOURCE_ARCHIVE)

srpm: source
	mkdir -p $(outdir)
	$(RPMBUILD) -bs $(SPEC) \
		--define "_sourcedir $(CURDIR)" \
		--define "_srcrpmdir $(abspath $(outdir))"

clean:
	rm -f $(SOURCE_ARCHIVE) $(SOURCE_ARCHIVE).tmp
