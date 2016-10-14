# 
# Copyright (C) 2007 Technical University of Liberec.  All rights reserved.
#
# Please make a following refer to Flow123d on your project site if you use the program for any purpose,
# especially for academic research:
# Flow123d, Research Centre: Advanced Remedial Technologies, Technical University of Liberec, Czech Republic
#
# This program is free software; you can redistribute it and/or modify it under the terms
# of the GNU General Public License version 3 as published by the Free Software Foundation.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this program; if not,
# write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 021110-1307, USA.
#
# $Id$
# $Revision$
# $LastChangedBy$
# $LastChangedDate$
#
# This makefile just provide main rules for: build, documentation and testing
# Build itself takes place in ../<branch>-build
#

# following depends on git_post_checkout_hook
# every target using var. BUILD_DIR has to depend on 'update-build-tree'
BUILD_DIR=$(shell cd -P ./build_tree && pwd)
SOURCE_DIR=$(shell pwd)

# reference manual directory
DOC_DIR=$(SOURCE_DIR)/doc/reference_manual


.PHONY : all
all: build-flow123d 



# Just build flow123d with existing configuration.
fast-flow123d:
#	./check_dates.sh
	@cd $(BUILD_DIR) && $(MAKE) bin/flow123d
	

# Build flow, update configuration and dependencies.
.PHONY : build-flow123d
build-flow123d: cmake
	@cd $(BUILD_DIR) && $(MAKE) bin/flow123d


# This target only configure the build process.
# Useful for building unit tests without actually build whole program.
.PHONY : cmake
cmake:  prepare-build
	@if [ ! -d "$(BUILD_DIR)" ]; then mkdir -p $(BUILD_DIR); fi
	@cd $(BUILD_DIR); cmake "$(SOURCE_DIR)"


# Save config.cmake from working dir to the build dir.
save-config: prepare-build
	cp -f $(SOURCE_DIR)/config.cmake $(BUILD_DIR)
	
# Restore config.cmake from build dir, possibly overwrite the current one.	
load-config: prepare-build
	cp -f $(BUILD_DIR)/config.cmake $(SOURCE_DIR)

	
# Remove all generated files
.PHONY: clean
clean: prepare-build cmake
	make -C $(BUILD_DIR) clean

# Remove all links in source and whole build tree
.PHONY: clean-all
clean-all: prepare-build
	# remove all symlinks in the source tree
	rm -f `find . -type l` 
	rm -rf $(BUILD_DIR)

# Make all tests
.PHONY: test-all
test-all: build-flow123d
	make -C tests test-all

# Make only certain test (e.g. "make 01.tst" will make first test)
%.tst : build-flow123d
	make -C tests $*.tst

# Clean test results using find command
.PHONY: clean-tests
clean-tests:
	find tests -maxdepth 2 -type d -name "test_results*" -exec echo " - {}" \;
	find tests -maxdepth 2 -type d -name "test_results*" -exec rm -rf {} \;


# Create html documentation
.PHONY: html-doc
html-doc: cmake update-build-tree
	make -C $(BUILD_DIR)/htmldoc htmldoc
	$(BUILD_DIR)/bin/flow123d --input_format "$(DOC_DIR)/input_reference.json"
	python $(SOURCE_DIR)/bin/python/ist_script.py --input=$(DOC_DIR)/input_reference.json --output=$(BUILD_DIR)/htmldoc/html/src/index.html --format=html


# Create doxygen documentation; use makefile generated by cmake
.PHONY: doxy-doc
doxy-doc: cmake update-build-tree
	make -C $(BUILD_DIR)/doc doxy-doc

# Create user manual using LaTeX sources and generated input reference; use makefile generated by cmake
# It does not generate new input reference file.
.PHONY: ref-doc
ref-doc:
	# generate json format specification (also contains flow123d open message text)
	# remove flow123d open message text by searching for character '['
	$(BUILD_DIR)/bin/flow123d --input_format "$(DOC_DIR)/input_reference.json"
	python $(SOURCE_DIR)/bin/python/ist_script.py --input=$(DOC_DIR)/input_reference.json --output=$(DOC_DIR)/input_reference.json.tex --format=tex
	cp $(DOC_DIR)/input_reference.json.tex $(DOC_DIR)/input_reference.tex 
	cd $(BUILD_DIR)/doc/reference_manual && cmake $(SOURCE_DIR)/doc/reference_manual
	make -C $(BUILD_DIR)/doc/reference_manual pdf

# call Flow123d and extract petsc command line arguments
.PHONY: petsc-doc
petsc-doc: #build-flow123d
	cd tests/02*; \
	mkdir output; \
	"$(BUILD_DIR)/bin/flow123d" -s flow_vtk.con -help --petsc_redirect "$(BUILD_DIR)/doc/petsc_help" >/dev/null

######################################################################################

# Prepare infrastructure for build:
# - copy post-checkout hook into .git/hooks
# - update build_tree link to current branch (call the post-checkout hook)
# - update submodules
.PHONY: prepare-build
prepare-build:  install-hooks update-build-tree update-submodules

	
# add post-checkout hook
.PHONY: install-hooks
install-hooks:
	if [ ! -e .git/hooks/post-checkout ];\
	then cp bin/git_post_checkout_hook .git/hooks/post-checkout;\
	fi	

# this is prerequisite for every target using BUILD_DIR variable
.PHONY: update-build-tree
update-build-tree:
	@-bin/git_post_checkout_hook	# do not print command, ignore return code


# initialize submodules in safe way
# check which kind of access use this repository use same type for submodules
# to this end one have to set key "https_url" in the .gitmodules to the alternative https URL.
.PHONY: initialize-submodules
initialize-submodules:
	git submodule init
	origin_url=$$( git config --get remote.origin.url ) ;\
	if [ "$${origin_url}" != "$${origin_url#https}" ]; \
	then \
		cp .gitmodules_https .gitmodules; \
	fi
	git submodule sync
	git checkout .gitmodules
	

# this target updates all submodules in this repository
.PHONY: update-submodules
update-submodules: initialize-submodules
	git submodule update
	

# Let every submodule checkout branch it track. 
# This may be useful to make some patches in submodule repository.
# Note, however, you have to make: git add <submodule> in order to change 
# the submodule's commit refered in the flow123d repository.
.PHONY: checkout-submodule-branches
checkout-submodule-branches:
	git submodule foreach -q --recursive 'branch="$$(git config -f $${toplevel}/.gitmodules submodule.$${name}.branch)"; git checkout $${branch}'
	

############################################################################################
#Input file generation.

# creates the file that defines additional information 
# for input description generated by flow123d to Latex format
# this file contains one replace rule per line in format
# \add_doc{<tag>}<replace>
# 
# this replace file is applied to input_reference.tex produced by flow123d
#

# call flow123d and make file flow_version.tex
#$(DOC_DIR)/flow_version.tex: update-build-tree build-flow123d
#	$(BUILD_DIR)/bin/flow123d --version | grep "This is Flow123d" | head -n1 | cut -d" " -f4-5 \
#	  > $(DOC_DIR)/flow_version.tex

# make empty file with replace rules if we do not have one
$(DOC_DIR)/add_to_ref_doc.txt: 
	touch $(DOC_DIR)/add_to_ref_doc.txt
	
# update file with replace rules according to acctual patterns appearing in input_refecence		
update_add_doc: $(DOC_DIR)/input_reference_raw.tex $(DOC_DIR)/add_to_ref_doc.txt
	cat $(DOC_DIR)/input_reference_raw.tex \
	| grep 'AddDoc' |sed 's/^.*\(\\AddDoc{[^}]*}\).*/\1/' \
	> $(DOC_DIR)/add_to_ref_doc.list
	$(DOC_DIR)/add_doc_replace.sh $(DOC_DIR)/add_to_ref_doc.txt $(DOC_DIR)/add_to_ref_doc.list	
	
# make final input_reference.tex, applying replace rules
#inputref: $(DOC_DIR)/flow_version.tex $(DOC_DIR)/input_reference_raw.tex update_add_doc
inputref: $(DOC_DIR)/input_reference_raw.tex update_add_doc
	$(DOC_DIR)/add_doc_replace.sh $(DOC_DIR)/add_to_ref_doc.txt $(DOC_DIR)/input_reference_raw.tex $(DOC_DIR)/input_reference.tex	


# target will run CPack in build_tree directory
package: all
	make -C build_tree package
.PHONY : package


# target will pack flow123d and create install image
PACKAGE_NAME=Flow123d-2.0.0-Linux.tar.gz
IMAGE_NAME=flow123d-image.tar.gz
create-install-image: package
	docker run    -tid --name flow_rel flow123d/install bash
	docker exec   flow_rel mkdir -p /opt/flow123d/flow123d
	docker cp     build_tree/$(PACKAGE_NAME) flow_rel:/tmp/$(PACKAGE_NAME)
	docker exec   flow_rel tar -xzf /tmp/$(PACKAGE_NAME) -C /opt/flow123d/flow123d --strip-components=1
	docker exec   flow_rel rm -rf /tmp/$(PACKAGE_NAME)
	docker export flow_rel > build_tree/$(IMAGE_NAME)
	docker stop   flow_rel
	docker rm     flow_rel
.PHONY : create-install-image


INSTALLER_NAME=flow123d-2.0.0.exe
create-windows-nsis: 
	docker run -tid --name nsis flow123d/nsis
	docker exec nsis mkdir -p /pack/build/libs/
	docker      cp build_tree/DockerToolbox-1.12.2.exe nsis:/pack/libs/
	docker      cp build_tree/flow123d-image.tar.gz    nsis:/pack/libs/
	
	docker exec nsis make all
	docker cp   nsis:/pack/build/Flow123d-0.1.1-Linux.exe build_tree/
	docker stop nsis
	docker rm   nsis
	mv build_tree/Flow123d-0.1.1-Linux.exe build_tree/$(INSTALLER_NAME)
.PHONY : create-windows-nsis
	
	
################################################################################################
# Help Target
help:
	@echo "The following are some of the valid targets for this Makefile:"
	@echo "... all (builds the whole library)"
	@echo "... cmake (configures the build process, useful for running unit_tests without building the whole library)"
	@echo "... test-all (runs all tests)"
	@echo "... %.test (runs selected test, e.g. 01.test)"
	@echo "... clean (removes generated files in build directory)"
	@echo "... clean-all (removes the whole build directory)"
	@echo "... clean-tests (cleans all generated files in tests, including results)"
	@echo "... doxy-doc (creates html documentation of the source using Doxygen)"
	@echo "... ref-doc (creates reference manual using LaTeX sources and generated input reference file)"
	@echo "... inputref (generates input reference file)"
	@echo "... package (runs CPack)"
#	@echo "packages:"
#	@echo "... linux-pack"
.PHONY : help

