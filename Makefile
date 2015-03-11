#CPP = ccache clang++
#LD = ccache clang
CPP = ccache g++
LD = ccache gcc
FLAGS = -Wall -Werror --std=c++11 # --stdlib=libc++
CPPFLAGS = $(FLAGS)
LDLAGS = $(FLAGS) -lstdc++ -lm
DEBFLAGS = -g -DDEBUG
RELFLAGS = -O3 -DNDEBUG
PROFFLAGS = $(RELFLAGS) -pg

TESTCPPFLAGS = -DBUILD_TESTS
TESTLDFLAGS = 
MAINCPPFLAGS = -DUSE_READLINE
MAINLDFLAGS = -lreadline

SRCS = \
	src/token.cpp \
	src/parser.cpp \
	src/gc.cpp \
	src/block.cpp \
	src/layout.cpp \
	src/value.cpp \
	src/object.cpp \
	src/class.cpp \
	src/singletons.cpp \
	src/bool.cpp \
	src/frame.cpp \
	src/callable.cpp \
	src/interp.cpp \
	src/common.cpp \
	src/exception.cpp \
	src/integer.cpp \
	src/string.cpp \
	src/list.cpp \
	src/dict.cpp \
	src/builtin.cpp \
	src/instr.cpp \
	src/specials.cpp \
	src/generator.cpp

TESTSRCS = src/test.cpp $(SRCS)
MAINSRCS = src/main.cpp $(SRCS)

TESTRELOBJS = $(subst src,build/test/rel,$(TESTSRCS:.cpp=.o))
TESTDEBOBJS = $(subst src,build/test/deb,$(TESTSRCS:.cpp=.o))

TESTRELDEPS = $(subst src,build/test/rel,$(TESTSRCS:.cpp=.d))
TESTDEBDEPS = $(subst src,build/test/deb,$(TESTSRCS:.cpp=.d))

MAINRELOBJS = $(subst src,build/main/rel,$(MAINSRCS:.cpp=.o))
MAINDEBOBJS = $(subst src,build/main/deb,$(MAINSRCS:.cpp=.o))
MAINPROFOBJS = $(subst src,build/main/prof,$(MAINSRCS:.cpp=.o))

MAINRELDEPS = $(subst src,build/main/rel,$(MAINSRCS:.cpp=.d))
MAINDEBDEPS = $(subst src,build/main/deb,$(MAINSRCS:.cpp=.d))
MAINPROFDEPS = $(subst src,build/main/prof,$(MAINSRCS:.cpp=.d))

.PHONY: all
all: dynamic tests-debug

tests: $(TESTRELOBJS) Makefile
	$(LD) $(TESTRELOBJS) $(RELFLAGS) $(LDLAGS) $(TESTLDLAGS) -o tests

tests-debug: $(TESTDEBOBJS) Makefile
	$(LD) $(TESTDEBOBJS) $(DEBFLAGS) $(LDLAGS) $(TESTLDLAGS) -o tests-debug

dynamic: $(MAINRELOBJS) Makefile
	$(LD) $(MAINRELOBJS) $(RELFLAGS) $(LDLAGS) $(MAINLDFLAGS) -o dynamic

dynamic-debug: $(MAINDEBOBJS) Makefile
	$(LD) $(MAINDEBOBJS) $(DEBFLAGS) $(LDLAGS) $(MAINLDFLAGS) -o dynamic-debug

dynamic-prof: $(MAINPROFOBJS) Makefile
	$(LD) $(MAINPROFOBJS) $(PROFFLAGS) $(LDLAGS) $(MAINLDFLAGS) -o dynamic-prof

WRAPPER = python test/debug-wrapper
TEST-RUNNER = python test/run-tests
.PHONY: test
test: tests-debug dynamic-debug
	@echo "Entering directory \`src'"
	@$(WRAPPER) tests-debug
	@$(TEST-RUNNER) dynamic-debug

.PHONY: bench
bench: dynamic
	python benchmarks/run_benchmarks.py

.PHONY: clean
clean:
	rm -f build/*/*/* dynamic* tests*

-include $(TESTRELDEPS)
-include $(TESTDEBDEPS)
-include $(MAINRELDEPS)
-include $(MAINDEBDEPS)
-include $(MAINPROFDEPS)

build/test/rel/%.o: src/%.cpp Makefile
	mkdir -p build/test/rel
	$(CPP) -c -MMD $(RELFLAGS) $(CPPFLAGS) $(TESTCPPFLAGS) $< -o $@

build/test/deb/%.o: src/%.cpp Makefile
	mkdir -p build/test/deb
	$(CPP) -c -MMD $(DEBFLAGS) $(CPPFLAGS) $(TESTCPPFLAGS) $< -o $@

build/main/rel/%.o: src/%.cpp Makefile
	mkdir -p build/main/rel
	$(CPP) -c -MMD $(RELFLAGS) $(CPPFLAGS) $(MAINCPPFLAGS) $< -o $@

build/main/deb/%.o: src/%.cpp Makefile
	mkdir -p build/main/deb
	$(CPP) -c -MMD $(DEBFLAGS) $(CPPFLAGS) $(MAINCPPFLAGS) $< -o $@

build/main/prof/%.o: src/%.cpp Makefile
	mkdir -p build/main/prof
	$(CPP) -c -MMD $(PROFFLAGS) $(CPPFLAGS) $(MAINCPPFLAGS) $< -o $@
