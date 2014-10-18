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
ALLSRCS = $(SRCS) src/main.cpp src/test.cpp

TESTRELOBJS = $(subst src,build/rel,$(TESTSRCS:.cpp=.o))
TESTDEBOBJS = $(subst src,build/deb,$(TESTSRCS:.cpp=.o))

MAINRELOBJS = $(subst src,build/rel,$(MAINSRCS:.cpp=.o))
MAINDEBOBJS = $(subst src,build/deb,$(MAINSRCS:.cpp=.o))
MAINPROFOBJS = $(subst src,build/prof,$(MAINSRCS:.cpp=.o))

ALLRELDEPS = $(subst src,build/rel,$(ALLSRCS:.cpp=.d))
ALLDEBDEPS = $(subst src,build/deb,$(ALLSRCS:.cpp=.d))
ALLPROFDEPS = $(subst src,build/prof,$(ALLSRCS:.cpp=.d))

dynamic: $(MAINRELOBJS)
	$(LD) $(MAINRELOBJS) $(RELFLAGS) $(LDLAGS) -o dynamic

dynamic-debug: $(MAINDEBOBJS)
	$(LD) $(MAINDEBOBJS) $(DEBFLAGS) $(LDLAGS) -o dynamic-debug

dynamic-prof: $(MAINPROFOBJS)
	$(LD) $(MAINPROFOBJS) $(PROFFLAGS) $(LDLAGS) -o dynamic-prof

tests: $(TESTRELOBJS)
	$(LD) $(TESTRELOBJS) $(RELFLAGS) $(LDLAGS) -o tests

tests-debug: $(TESTDEBOBJS)
	$(LD) $(TESTDEBOBJS) $(DEBFLAGS) $(LDLAGS) -o tests-debug

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
	rm -f build/*/* dynamic* tests*

-include $(ALLRELDEPS)
-include $(ALLDEBDEPS)
-include $(ALLPROFDEPS)

build/rel/%.o: src/%.cpp Makefile
	mkdir -p build/rel
	$(CPP) -c -MMD $(RELFLAGS) $(CPPFLAGS) $< -o $@

build/deb/%.o: src/%.cpp Makefile
	mkdir -p build/deb
	$(CPP) -c -MMD $(DEBFLAGS) $(CPPFLAGS) $< -o $@

build/prof/%.o: src/%.cpp Makefile
	mkdir -p build/prof
	$(CPP) -c -MMD $(PROFFLAGS) $(CPPFLAGS) $< -o $@
