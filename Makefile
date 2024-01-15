# Originally taken from btop

override TIMESTAMP := $(shell date +%s 2>/dev/null || echo "0")
override DATE_CMD := date

ifeq ($(VERBOSE),true)
	override VERBOSE := false
else
	override VERBOSE := true
endif

#? Pull in platform specific source files and get thread count
THREADS	:= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)

#? Use all CPU cores (will only be set if using Make 4.3+)
MAKEFLAGS := --jobs=$(THREADS)
ifeq ($(THREADS),1)
	override THREADS := auto
endif

#? The Directories, Source, Includes, Objects and Binary
SRCDIR		:= src
TESTDIR		:= tests
OBJDIR		:= obj
TESTOBJDIR	:= tests/obj
SRCEXT		:= cpp
DEPEXT		:= d
OBJEXT		:= o

PROGNAME := tMus
#? Flags, Libraries and Includes
override REQFLAGS   := -std=c++23
WARNFLAGS			:= -Wall -Wextra -pedantic -Werror=uninitialized -Werror=shadow -Wnon-virtual-dtor -Wdouble-promotion -Wunused -Wduplicated-cond -Wduplicated-branches -Wnull-dereference -Wconversion -Wno-sign-conversion -Werror -Wdeprecated -Wdeprecated-copy-dtor
OPTFLAGS			:= -march=native -O2 -D_FORTIFY_SOURCE=3 -fno-omit-frame-pointer -ftree-loop-vectorize
LDCXXFLAGS			:= -lnotcurses++ -lnotcurses-core -lfmt -lSDL2 -lavutil -lavformat -lavcodec -lswresample -lavdevice -lswscale -D_GLIBCXX_ASSERTIONS $(ADDFLAGS)
override CXXFLAGS	+= $(REQFLAGS) $(LDCXXFLAGS) $(OPTFLAGS) $(WARNFLAGS)
override LDFLAGS	+= $(LDCXXFLAGS) $(OPTFLAGS) $(WARNFLAGS)

ifdef DEBUG
	override OPTFLAGS := -DDEBUG -O0 -g3 -fno-omit-frame-pointer -fno-inline
endif

SOURCES := $(wildcard $(SRCDIR)/*.cpp $(SRCDIR)/*.hpp)

CPPFILES 	 := $(wildcard $(SRCDIR)/*.cpp)
TESTCPPFILES := $(wildcard $(TESTDIR)/*.cpp)

#? Setup percentage progress
OBJECTS		:= $(patsubst $(SRCDIR)/%,$(OBJDIR)/%,$(CPPFILES:.$(SRCEXT)=.$(OBJEXT)))
TESTOBJECTS := $(patsubst $(TESTDIR)/%,$(TESTOBJDIR)/%,$(TESTCPPFILES:.$(SRCEXT)=.$(OBJEXT)))

TEST=tests
TESTS=$(wildcard $(TEST)/*.cpp)
TESTBINS=$(patsubst $(TEST)/%.cpp,$(TEST)/bin/%,$(TESTS))

TESTOBJ := $(filter-out $(OBJDIR)/main.o,$(OBJECTS))

all: directories objs

#? Pull in dependency info for *existing* .o files
-include $(OBJECTS:.$(OBJEXT)=.$(DEPEXT))
-include $(TESTOBJECTS:.$(OBJEXT)=.$(DEPEXT))

# $(TEST)/bin:
# 	@mkdir -p $@

# $(TEST)/obj:
# 	@mkdir -p $@

$(TEST)/obj/%.$(OBJEXT): $(TEST)/%.cpp
	@printf "\033[1;97mCompiling $@\033[0m\n"
	@$(CXX) $(CXXFLAGS) $< -MMD -c -g -o $@ -Isrc

.ONESHELL:
$(TEST)/bin/%: $(TEST)/obj/%.o $(TESTOBJ)
	@printf "\033[1;97mLinking $@\033[0m\n"
	@$(VERBOSE) || @printf "$(CXX) $(CXXFLAGS) -c -o $<\n"
	@$(CXX) $< $(TESTOBJ) $(LDFLAGS) -o $@
	@$(VERBOSE) || @printf "\033[1;92m\033[0G-> \033[1;37m$@ \033[100D\033[38C\033[1;92m(\033[1;97m$$(du -ah $@ | cut -f1)iB\033[1;92m)\033[0m\n"

.ONESHELL:
test: directories $(TESTBINS) $(SOURCES)
	@for test in $(TESTBINS) ; do ./$$test || exit 1; done

#? Make the Directories
directories:
	@mkdir -p $(OBJDIR)
	@mkdir -p $(TEST)/bin
	@mkdir -p $(TEST)/obj

clean-test:
	@printf "\033[1;91mRemoving: \033[1;97mtest build objects\033[0m\n"
	@rm -rf $(TEST)/bin
	@rm -rf $(TEST)/obj

#? Clean only Objects
clean: clean-test
	@printf "\033[1;91mRemoving: \033[1;97mbuilt objects...\033[0m\n"
	@rm -rf $(PROGNAME)
	@rm -rf "tests/bin"
	@rm -rf $(OBJDIR)

.ONESHELL:
objs: $(OBJECTS)
	@TSTAMP=$$(date +%s 2>/dev/null || echo "0")
	@printf "\033[1;92mLinking\033[37m...\033[0m\n"
	@$(VERBOSE) || printf "$(CXX) -o $(PROGNAME) $^ $(LDFLAGS)\n"
	@$(CXX) -o $(PROGNAME) $^ $(LDFLAGS) || exit 1
	@printf "> \033[1;37mtMus \033[100D\033[38C\033[1;93m(\033[1;97m$$(du -ah $(PROGNAME) | cut -f1)iB\033[1;93m) \033[92m\n"
	@printf "\n\033[1;92mBuild complete in \033[92m(\033[97m$$($(DATE_CMD) -d @$$(expr $$(date +%s 2>/dev/null || echo "0") - \$(TIMESTAMP) 2>/dev/null) -u +%Mm:%Ss 2>/dev/null | sed 's/^00m://' || echo "unknown")\033[92m)\033[0m\n"

#? Compile
.ONESHELL:
$(OBJDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT)
	@printf "\033[1;97mCompiling $<\033[0m\n"
	@$(VERBOSE) || printf "$(CXX) $(CXXFLAGS) -MMD -c -o $@ $<\n"
	@$(CXX) $(CXXFLAGS) -MMD -c -o $@ $< || exit 1
	@printf "\033[1;92m\033[0G-> \033[1;37m$@ \033[100D\033[38C\033[1;92m(\033[1;97m$$(du -ah $@ | cut -f1)iB\033[1;92m)\033[0m\n"

#? Non-File Targets
.PHONY: all directories
.PRECIOUS: $(TEST)/obj/%.o # Don't remove object files
