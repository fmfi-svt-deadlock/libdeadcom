DCL2_SOURCE   = dcl2/lib/src
DCL2_INCLUDE  = dcl2/lib/inc
DCL2_SRC      = $(shell find $(DCL2_SOURCE) -type f -name '*.c')

##############################################################################
# Start of pthread DeadCom Shared Object
#

DCL2_PTHREADS_SOURCE  = dcl2/helper-pthreads/src
DCL2_PTHREADS_INCLUDE = dcl2/helper-pthreads/inc
DCL2_PTHREADS_SRC     = $(shell find $(DCL2_PTHREADS_SOURCE) -type f -name '*.c')

DCL2_PTHREADS_TARGET  =
DCL2_PTHREADS_CC      = $(DCL2_PTHREADS_TARGET)gcc
DCL2_PTHREADS_CFLAGS  = -I$(DCL2_INCLUDE) -I$(DCL2_PTHREADS_INCLUDE) -lpthread -fpic -shared

build/dcl2-pthread.so: $(DCL2_SRC) $(DCL2_PTHREADS_SRC)
	$(DCL2_PTHREADS_CC) $(DCL2_PTHREADS_CFLAGS) $^ -o $@

#
# End of pthread DeadCom Shared Object
##############################################################################

##############################################################################
# Start of Unit Testing with Unity rules
#

UNITY         = deps/Unity/src/
FFF           = deps/fff/

TEST_BUILD    = build/test/unit/build/
TEST_OBJS     = build/test/unit/objs/
TEST_RESULTS  = build/test/unit/results/
TEST_RUNNERS  = build/test/unit/runners/
TEST_BUILD_PATHS = $(TEST_BUILD) $(TEST_OBJS) $(TEST_RESULTS) $(TEST_RUNNERS)
TEST_PATH     = test/unit/
TEST_CSRC     = $(shell find $(TEST_PATH) -type f -regextype sed -regex '.*-test[0-9]*\.c')

TEST_TRGT       =
TEST_CC         = $(TEST_TRGT)gcc
TEST_LD         = $(TEST_TRGT)gcc
TEST_INCDIR     = $(FFF) $(DCL2_INCLUDE)
TEST_INCPARAMS  = $(foreach d, $(TEST_INCDIR), -I$d)
TEST_CFLAGS     = -I. -I$(UNITY) $(TEST_INCPARAMS) -DTEST -g -Wno-trampolines


$(TEST_BUILD):
	@mkdir -p $(TEST_BUILD)

$(TEST_OBJS):
	@mkdir -p $(TEST_OBJS)

$(TEST_RESULTS):
	@mkdir -p $(TEST_RESULTS)

$(TEST_RUNNERS):
	@mkdir -p $(TEST_RUNNERS)

$(TEST_RUNNERS)%-runner.c:: $(TEST_PATH)%.c
	@echo 'Generating runner for $@'
	@mkdir -p `dirname $@`
	@ruby $(UNITY)../auto/generate_test_runner.rb $< $@

$(TEST_OBJS)unity.o:: $(UNITY)unity.c $(UNITY)unity.h
	@echo 'Compiling Unity'
	@$(TEST_CC) $(TEST_CFLAGS) -c $< -o $@

$(TEST_OBJS)%.o:: $(TEST_PATH)%.c
	@echo 'Compiling test $<'
	@mkdir -p `dirname $@`
	@$(TEST_CC) $(TEST_CFLAGS) -c $< -o $@

$(TEST_OBJS)%-runner.o: $(TEST_RUNNERS)%-runner.c
	@echo 'Compiling test runner $<'
	@mkdir -p `dirname $@`
	@$(TEST_CC) $(TEST_CFLAGS) -c $< -o $@

$(TEST_OBJS)%-under_test.o: $(TEST_ROOT)%.c
	@echo 'Compiling $<'
	@mkdir -p `dirname $@`
	@$(TEST_CC) $(TEST_CFLAGS) -c $< -o $@
	@objcopy --weaken $@

.SECONDEXPANSION:
FILE_UNDER_TEST := sed -e 's|$(TEST_BUILD)\(.*\)-test[0-9]*\.out|$(TEST_OBJS)\1-under_test.o|'
$(TEST_BUILD)%.out: $$(shell echo $$@ | $$(FILE_UNDER_TEST)) $(TEST_OBJS)%.o $(TEST_OBJS)unity.o $(TEST_OBJS)%-runner.o
	@echo 'Linking test $@'
	@mkdir -p `dirname $@`
	@$(TEST_LD) -o $@ $^

$(TEST_RESULTS)%.result: $(TEST_BUILD)%.out
	@echo
	@echo "******************************************************************************"
	@echo "*"
	@echo '* ----- Running test $<:'
	@mkdir -p `dirname $@`
	@-valgrind -q --track-origins=yes ./$< > $@ 2>&1
	@sed -e 's/^/* /' $@
	@echo "*"
	@echo "******************************************************************************"
	@echo

RESULTS = $(patsubst $(TEST_PATH)%.c,$(TEST_RESULTS)%.result,$(TEST_CSRC))
TEST_EXECS = $(patsubst $(TEST_RESULTS)%.result,$(TEST_BUILD)%.out,$(RESULTS))

run-tests: $(TEST_BUILD_PATHS) $(TEST_EXECS) $(RESULTS)
	@echo
	@echo "----- SUMMARY -----"
	@echo "PASS:   `for i in $(RESULTS); do grep -s :PASS $$i; done | wc -l`"
	@echo "IGNORE: `for i in $(RESULTS); do grep -s :IGNORE $$i; done | wc -l`"
	@echo "FAIL:   `for i in $(RESULTS); do grep -s :FAIL $$i; done | wc -l`"
	@echo
	@echo "DONE"
	@if [ "`for i in $(RESULTS); do grep -s FAIL $$i; done | wc -l`" != 0 ]; then \
	exit 1; \
	fi

clean-tests:
	@rm -rf $(TEST_BUILD) $(TEST_OBJS) $(TEST_RESULTS) $(TEST_RUNNERS)

test: run-tests clean-tests

print_tcsrc:
	echo $(TEST_CSRC)

#
# End of Unit Testing with Unity rules
##############################################################################

clean:
	rm -rf build/
