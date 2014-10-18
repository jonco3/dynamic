#CPP = ccache clang++
#LD = ccache clang
CPP = ccache g++
LD = ccache gcc
FLAGS = -Wall -Werror --std=c++11 # --stdlib=libc++
CPPFLAGS = $(FLAGS)  # -DUSE_READLINE -I/usr/include/gc
LDLAGS = $(FLAGS) -lstdc++ -lm # -lgc -lreadline
DEBFLAGS = -g -DDEBUG
RELFLAGS = -O3
PROFFLAGS = $(RELFLAGS) -pg

SRCS = \
	src/token.cpp \
	src/parser.cpp \
	src/block.cpp \
	src/layout.cpp \
	src/value.cpp \
	src/object.cpp \
	src/class.cpp \
	src/none.cpp \
	src/bool.cpp \
	src/integer.cpp \
	src/frame.cpp \
	src/callable.cpp \
	src/interp.cpp \
	src/gc.cpp \
	src/common.cpp

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

tests: $(TESTRELOBJS)
	$(LD) $(TESTRELOBJS) $(RELFLAGS) $(LDLAGS) -o tests

tests-debug: $(TESTDEBOBJS)
	$(LD) $(TESTDEBOBJS) $(DEBFLAGS) $(LDLAGS) -o tests-debug

dynamic: $(MAINRELOBJS)
	$(LD) $(MAINRELOBJS) $(RELFLAGS) $(LDLAGS) -o dynamic

dynamic-debug: $(MAINDEBOBJS)
	$(LD) $(MAINDEBOBJS) $(DEBFLAGS) $(LDLAGS) -o dynamic-debug

dynamic-prof: $(MAINPROFOBJS)
	$(LD) $(MAINPROFOBJS) $(PROFFLAGS) $(LDLAGS) -o dynamic-prof

.PHONY: all
all: dynamic tests-debug

WRAPPER = python test/debug-wrapper
.PHONY: test
test: tests-debug
	@echo "Entering directory \`src'"
	@$(WRAPPER) tests-debug -t

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
	$(CPP) -c -MMD $(RELFLAGS) $(CPPFLAGS) -DBUILD_TESTS $< -o $@

build/test/deb/%.o: src/%.cpp Makefile
	mkdir -p build/test/deb
	$(CPP) -c -MMD $(DEBFLAGS) $(CPPFLAGS) -DBUILD_TESTS $< -o $@

build/main/rel/%.o: src/%.cpp Makefile
	mkdir -p build/main/rel
	$(CPP) -c -MMD $(RELFLAGS) $(CPPFLAGS) $< -o $@

build/main/deb/%.o: src/%.cpp Makefile
	mkdir -p build/main/deb
	$(CPP) -c -MMD $(DEBFLAGS) $(CPPFLAGS) $< -o $@

build/main/prof/%.o: src/%.cpp Makefile
	mkdir -p build/main/prof
	$(CPP) -c -MMD $(PROFFLAGS) $(CPPFLAGS) $< -o $@
