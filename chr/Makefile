_objs := chr.o chrconvert.o stb_image.o cmdline.o
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
libs := -lfmt -L/usr/X11R6/lib -lm -lpthread -lX11 -lfmt

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

all: $(outdir)/chrconvert

$(outdir)/chrconvert: $(outdir) $(objs) $(objs_convert)
	$(info Linking $@ ...)
	$(CXX) $(objs) -o $@ $(libs)

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
