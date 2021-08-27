_objs := chr.o
_objs_convert := chrconvert.o
_objs_extract := chrextract.o stb_image.o
outdir := debug
build := debug
CC := gcc
CXX := g++
CFLAGS := -I. -std=c11
CXXFLAGS := -I. -std=c++20 \
			-Wall -Wextra -pipe -Wcast-align -Wcast-qual -Wpointer-arith \
		 	-Wformat=2 -Wmissing-include-dirs -Wno-unused-parameter \
		 	-fconcepts
flags_deps = -MMD -MP -MF $(@:.o=.d)
#libs :=
libs_extract := -lfmt
libs_convert := -L/usr/X11R6/lib -lm -lpthread -lX11 -lfmt

ifeq ($(build),debug)
    outdir := debug
    CFLAGS += -g -DDEBUG
    CXXFLAGS += -g -DDEBUG
else
    outdir := release
    CFLAGS += -O3
    CXXFLAGS += -O3
endif

objs := $(patsubst %,$(outdir)/%,$(_objs))
objs_convert := $(patsubst %,$(outdir)/%,$(_objs_convert))
objs_extract := $(patsubst %,$(outdir)/%,$(_objs_extract))

all: $(outdir)/chrconvert $(outdir)/chrextract

$(outdir)/chrconvert: $(outdir) $(objs) $(objs_convert)
	$(info Linking $@ ...)
	$(CXX) $(objs) $(objs_convert) -o $@ $(libs) $(libs_convert)

$(outdir)/chrextract: $(outdir) $(objs) $(objs_extract)
	$(info Linking $@ ...)
	$(CXX) $(objs) $(objs_extract) -o $@ $(libs) $(libs_extract)

$(outdir)/stb_image.o: stb_image.c
	$(info Compiling $< ...)
	@$(CC) $(CFLAGS) $(flags_deps) -c $< -o $@

$(outdir)/%.o: %.cpp
	$(info Compiling $< ...)
	@$(CXX) $(CXXFLAGS) $(flags_deps) -c $< -o $@

$(outdir):
	mkdir -p $(outdir)

.PHONY: clean tests

clean:
	rm -rf $(outdir)
