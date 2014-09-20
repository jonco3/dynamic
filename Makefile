CPP = ccache clang++
LD = ccache clang
#CPP = g++
#LD = gcc
FLAGS = -Wall -Werror --std=c++11 --stdlib=libc++
CPPFLAGS = $(FLAGS)  # -DUSE_READLINE -I/usr/include/gc
LFLAGS = $(FLAGS) -lstdc++ # -lm -lgc -lreadline
DEBFLAGS = -g -DDEBUG
RELFLAGS = -O3
PROFFLAGS = $(RELFLAGS) -pg

SRCS = \
	src/main.cpp \
	src/test.cpp \
	src/token.cpp \
	src/parser.cpp \
	src/syntax.cpp \
	src/block.cpp \
	src/layout.cpp \
	src/value.cpp \
	src/object.cpp \
	src/class.cpp \
	src/none.cpp \
	src/integer.cpp \
	src/frame.cpp \
	src/callable.cpp \
	src/interp.cpp

RELOBJS = $(subst src,build/rel,$(SRCS:.cpp=.o))
RELDEPS = $(subst src,build/rel,$(SRCS:.cpp=.d))
DEBOBJS = $(subst src,build/deb,$(SRCS:.cpp=.o))
DEBDEPS = $(subst src,build/deb,$(SRCS:.cpp=.d))
PROFOBJS = $(subst src,build/prof,$(SRCS:.cpp=.o))
PROFDEPS = $(subst src,build/prof,$(SRCS:.cpp=.d))

dynamic: $(RELOBJS)
	$(LD) $(RELOBJS) $(RELFLAGS) $(LFLAGS) -o dynamic

dynamic-debug: $(DEBOBJS)
	$(LD) $(DEBOBJS) $(DEBFLAGS) $(LFLAGS) -o dynamic-debug

dynamic-prof: $(PROFOBJS)
	$(LD) $(PROFOBJS) $(PROFFLAGS) $(LFLAGS) -o dynamic-prof

.PHONY: all
all: dynamic

WRAPPER = python test/debug-wrapper
.PHONY: test
test: dynamic-debug
	@echo "Entering directory \`src'"
	@$(WRAPPER) dynamic-debug -t

.PHONY: bench
bench: dynamic
	ruby benchmarks/run_benchmarks.rb

.PHONY: clean
clean:
	rm -f build/*/* dynamic*

-include $(RELDEPS)
-include $(DEBDEPS)
-include $(PROFDEPS)

build/rel/%.o: src/%.cpp Makefile
	mkdir -p build/rel
	$(CPP) -c -MMD $(RELFLAGS) $(CPPFLAGS) $< -o $@

build/deb/%.o: src/%.cpp Makefile
	mkdir -p build/deb
	$(CPP) -c -MMD $(DEBFLAGS) $(CPPFLAGS) $< -o $@

build/prof/%.o: src/%.cpp Makefile
	mkdir -p build/prof
	$(CPP) -c -MMD $(PROFFLAGS) $(CPPFLAGS) $< -o $@
