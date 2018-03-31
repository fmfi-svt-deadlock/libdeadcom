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
# Start of Unity rules for DCL2 Unit tests
#

T_DCL2UNIT_INCDIR     = $(UNITY) $(FFF) $(DCL2_INCLUDE)
T_DCL2UNIT_INCPARAMS  = $(foreach d, $(T_DCL2UNIT_INCDIR), -I$d)
T_DCL2UNIT_SUB        = dcl2-unit/
T_DCL2UNIT_CSRC       = $(shell find $(TEST_PATH)$(T_DCL2UNIT_SUB) -type f -regextype sed -regex '.*-test[0-9]*\.c')

$(TEST_OBJS)$(T_DCL2UNIT_SUB)%-under_test.o: %.c
	@echo 'Compiling $<'
	@mkdir -p `dirname $@`
	@$(TEST_CC) $(TEST_CFLAGS) -c $< -o $@
	@objcopy --weaken $@

.SECONDEXPANSION:
FILE_UNDER_TEST := sed -e 's|$(TEST_BUILD)$(T_DCL2UNIT_SUB)\(.*\)-test[0-9]*\.out|$(TEST_OBJS)$(T_DCL2UNIT_SUB)\1-under_test.o|'
$(TEST_BUILD)$(T_DCL2UNIT_SUB)%.out: $$(shell echo $$@ | $$(FILE_UNDER_TEST)) $(TEST_OBJS)$(T_DCL2UNIT_SUB)%.o $(TEST_OBJS)unity.o $(TEST_OBJS)$(T_DCL2UNIT_SUB)%-runner.o
	@echo 'Linking test $@'
	@mkdir -p `dirname $@`
	@$(TEST_LD) $(TEST_LDFLAGS) -o $@ $^

T_DCL2_UNIT_RESULTS = $(patsubst $(TEST_PATH)%.c,$(TEST_RESULTS)%.testresults,$(T_DCL2UNIT_CSRC))
T_DCL2_UNIT_EXECS = $(patsubst $(TEST_RESULTS)%.testresults,$(TEST_BUILD)%.out,$(T_DCL2_UNIT_RESULTS))

run-dcl2unit-tests: TEST_CFLAGS = -I. $(T_DCL2UNIT_INCPARAMS) -DTEST -g -Wno-trampolines
run-dcl2unit-tests: $(TEST_BUILD_PATHS) $(T_DCL2_UNIT_EXECS) $(T_DCL2_UNIT_RESULTS) print-summary

#
# End of Unity rules for DCL2 Unit tests
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


##############################################################################
# Start of Leaky Pipe shared object
#

PIPE_SOURCE 	= deps/pipe
PIPE_INCLUDE	= deps/pipe

LP_SOURCE		= leaky-pipe/src
LP_INCLUDE  	= leaky-pipe/inc
LP_SRC      	= $(shell find $(LP_SOURCE) -type f -name '*.c')
LP_SRC 		   += $(shell find $(PIPE_SOURCE) -type f -name '*.c')

LP_TARGET       =
LP_CC           = $(LP_TARGET)gcc
LP_CFLAGS       = -I$(LP_INCLUDE) -I$(PIPE_INCLUDE) -lpthread -fpic -shared

build/leaky-pipe.so: $(LP_SRC)
	@mkdir -p `dirname $@`
	$(LP_CC) $(LP_CFLAGS) $^ -o $@

#
# End of Leaky Pipe shared object
##############################################################################


##############################################################################
# Start of Unity rules for DCL2 integration tests (backed by pthreads)
#

T_DCL2INTG_SUB        = dcl2-integration/
T_DCL2INTG_INCDIR     = $(UNITY) $(FFF) $(TEST_PATH)$(T_DCL2INTG_SUB) $(DCL2_INCLUDE) $(DCL2_PTHREADS_INCLUDE) $(LP_INCLUDE) $(PIPE_INCLUDE)
T_DCL2INTG_INCPARAMS  = $(foreach d, $(T_DCL2INTG_INCDIR), -I$d)
T_DCL2INTG_CSRC       = $(shell find $(TEST_PATH)$(T_DCL2INTG_SUB) -type f -regextype sed -name 'test_*.c')

$(TEST_BUILD)$(T_DCL2INTG_SUB)%.out: build/dcl2-pthread.so build/leaky-pipe.so $(TEST_OBJS)$(T_DCL2INTG_SUB)%.o $(TEST_OBJS)unity.o $(TEST_OBJS)$(T_DCL2INTG_SUB)%-runner.o $(TEST_OBJS)$(T_DCL2INTG_SUB)common.o
	@echo 'Linking test $@'
	@mkdir -p `dirname $@`
	@$(TEST_LD) $(TEST_OBJS)$(T_DCL2INTG_SUB)$*.o $(TEST_OBJS)unity.o $(TEST_OBJS)$(T_DCL2INTG_SUB)$*-runner.o $(TEST_OBJS)$(T_DCL2INTG_SUB)common.o -o $@ $(TEST_LDFLAGS)

T_DCL2INTG_RESULTS = $(patsubst $(TEST_PATH)%.c,$(TEST_RESULTS)%.testresults,$(T_DCL2INTG_CSRC))
T_DCL2INTG_EXECS = $(patsubst $(TEST_RESULTS)%.testresults,$(TEST_BUILD)%.out,$(T_DCL2INTG_RESULTS))

run-dcl2intg-tests: TEST_CFLAGS = -I. $(T_DCL2INTG_INCPARAMS) -DTEST -g -Wno-trampolines
run-dcl2intg-tests: TEST_LDFLAGS = -lpthread -L. -l:build/dcl2-pthread.so -l:build/leaky-pipe.so
run-dcl2intg-tests: DCL2_PTHREADS_CFLAGS += -g
run-dcl2intg-tests: $(TEST_BUILD_PATHS) $(T_DCL2INTG_EXECS) $(T_DCL2INTG_RESULTS) print-summary

#
# End of Unity rules for DCL2 integration tests (backed by pthreads)
##############################################################################


##############################################################################
# Start of Unity rules for leaky-pipes Unit tests
#

T_LPUNIT_INCDIR     = $(UNITY) $(FFF) $(LP_INCLUDE) $(PIPE_INCLUDE)
T_LPUNIT_INCPARAMS  = $(foreach d, $(T_LPUNIT_INCDIR), -I$d)
T_LPUNIT_SUB        = leaky-pipe-unit/
T_LPUNIT_CSRC       = $(shell find $(TEST_PATH)$(T_LPUNIT_SUB) -type f -regextype sed -regex '.*-test[0-9]*\.c')

$(TEST_BUILD)$(T_LPUNIT_SUB)%.out: build/leaky-pipe.so $(TEST_OBJS)$(T_LPUNIT_SUB)%.o $(TEST_OBJS)unity.o $(TEST_OBJS)$(T_LPUNIT_SUB)%-runner.o
	@echo 'Linking test $@'
	@mkdir -p `dirname $@`
	@$(TEST_LD) $(TEST_OBJS)$(T_LPUNIT_SUB)$*.o $(TEST_OBJS)unity.o $(TEST_OBJS)$(T_LPUNIT_SUB)$*-runner.o -o $@ $(TEST_LDFLAGS)

T_LPUNIT_RESULTS = $(patsubst $(TEST_PATH)%.c,$(TEST_RESULTS)%.testresults,$(T_LPUNIT_CSRC))
T_LPUNIT_EXECS = $(patsubst $(TEST_RESULTS)%.testresults,$(TEST_BUILD)%.out,$(T_LPUNIT_RESULTS))

run-lpunit-tests: TEST_CFLAGS = -I. $(T_LPUNIT_INCPARAMS) -DTEST -g -Wno-trampolines
run-lpunit-tests: TEST_LDFLAGS = -lpthread -L. -l:build/leaky-pipe.so
run-lpunit-tests: $(TEST_BUILD_PATHS) $(T_LPUNIT_EXECS) $(T_LPUNIT_RESULTS) print-summary

#
# End of Unity rules for leaky-pipe Unit tests
##############################################################################

clean:
	rm -rf build/

clean-tests:
	@rm -rf $(TEST_BUILD) $(TEST_OBJS) $(TEST_RESULTS) $(TEST_RUNNERS)

test: run-dcl2unit-tests run-dcl2intg-tests run-lpunit-tests
	@ruby deps/Unity/auto/unity_test_summary.rb build/test/results/
	@rm -rf build/test/results
