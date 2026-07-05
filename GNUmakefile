# GNUmakefile — awsim
#
# A wrapper that GNU make reads BEFORE the generated `Makefile`. It does NOT edit
# or replace the generated Makefile (which is marked DO NOT EDIT and is rewritten
# by GenerateProjectFiles) — it forwards the build to it, and additionally runs
# the awsim automation specs after the two build targets you use.
#
#   make awsim         build the game target,   then run the specs
#   make awsimEditor   build the editor target, then run the specs
#   make test          run the specs only (no build)
#   make <other>       forwarded to the generated Makefile unchanged (no tests)

# Engine path is read by Scripts/run-tests.sh from the generated Makefile (or
# $UNREALROOTPATH if you export it), so no personal path is hardcoded here.
TEST_FILTER ?= awsim.Simulation

.PHONY: awsim awsimEditor test

awsim:
	@$(MAKE) --no-print-directory -f Makefile awsim
	@$(MAKE) --no-print-directory test

awsimEditor:
	@$(MAKE) --no-print-directory -f Makefile awsimEditor
	@$(MAKE) --no-print-directory test

test:
	@"$(CURDIR)/Scripts/run-tests.sh" "$(TEST_FILTER)"

# Every other target/config is forwarded to the generated Makefile unchanged.
%:
	@$(MAKE) --no-print-directory -f Makefile $@
