##############################################################################
# Start of test runner makefile include
#

UNITY           = deps/Unity/src/
FFF             = deps/fff/
TEST_PATH       = test/

include test/makefile.in

#
# End of test runner makefile include
##############################################################################


##############################################################################
# Start of DeadCom library variables
#

DCL2_SOURCE   = dcl2/lib/src
DCL2_INCLUDE  = dcl2/lib/inc
DCL2_SRC      = $(shell find $(DCL2_SOURCE) -type f -name '*.c')

#
# End of DeadCom library variables
##############################################################################


##############################################################################
# Start of Unit Testing with Unity rules
#

T_DCL2UNIT_INCDIR     = $(UNITY) $(FFF) $(DCL2_INCLUDE)
T_DCL2UNIT_INCPARAMS  = $(foreach d, $(T_DCL2UNIT_INCDIR), -I$d)
T_DCL2UNIT_SUB        = unit/
T_DCL2UNIT_CSRC       = $(shell find $(TEST_PATH)$(T_DCL2UNIT_SUB) -type f -regextype sed -regex '.*-test[0-9]*\.c')

$(TEST_OBJS)unit/%-under_test.o: %.c
	@echo 'Compiling $<'
	@mkdir -p `dirname $@`
	@$(TEST_CC) $(TEST_CFLAGS) -c $< -o $@
	@objcopy --weaken $@

.SECONDEXPANSION:
FILE_UNDER_TEST := sed -e 's|$(TEST_BUILD)\(.*\)-test[0-9]*\.out|$(TEST_OBJS)\1-under_test.o|'
$(TEST_BUILD)%.out: $$(shell echo $$@ | $$(FILE_UNDER_TEST)) $(TEST_OBJS)%.o $(TEST_OBJS)unity.o $(TEST_OBJS)%-runner.o
	@echo 'Linking test $@'
	@mkdir -p `dirname $@`
	@$(TEST_LD) $(TEST_LDFLAGS) -o $@ $^

T_DCL2_UNIT_RESULTS = $(patsubst $(TEST_PATH)%.c,$(TEST_RESULTS)%.result,$(T_DCL2UNIT_CSRC))
T_DCL2_UNIT_EXECS = $(patsubst $(TEST_RESULTS)%.result,$(TEST_BUILD)%.out,$(T_DCL2_UNIT_RESULTS))

run-dcl2unit-tests: TEST_CFLAGS = -I. $(T_DCL2UNIT_INCPARAMS) -DTEST -g -Wno-trampolines
run-dcl2unit-tests: $(TEST_BUILD_PATHS) $(T_DCL2_UNIT_EXECS) $(T_DCL2_UNIT_RESULTS) print-summary

#
# End of Unit Testing with Unity rules
##############################################################################


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

clean:
	rm -rf build/

clean-tests:
	@rm -rf $(TEST_BUILD) $(TEST_OBJS) $(TEST_RESULTS) $(TEST_RUNNERS)

test: run-dcl2unit-tests clean-tests
