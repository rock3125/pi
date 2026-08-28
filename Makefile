# PI Prolog Interpreter - console build for linux
#
#   make            build the optimised interpreter into ./prolog
#   make debug      build with symbols, no optimisation and tracing enabled
#   make test       build and run the regression tests
#   make docs       rebuild docs/c4-model.html from the markdown
#   make pdf        print docs/c4-model.pdf from that html
#   make clean      remove all build output
#
# override the compiler with e.g.  make CXX=clang++
#
# the sources live in src/, all intermediate files (.o/.d) in build/, and
# the finished binary in the root of the repository.

# -Wno-sign-compare and -Wno-switch are the gcc equivalents of the
# "#pragma warning(disable:4018/4267)" the code already carried for MSVC:
# the engine deliberately switches on a subset of the tag enum, and indexes
# containers with ints throughout.
WARN     := -Wall -Wno-sign-compare -Wno-switch

CXX      ?= g++
CXXFLAGS ?= -std=c++17 $(WARN) -O2 -pthread
LDFLAGS  ?= -pthread

TARGET   := prolog
SRCDIR   := src
BUILDDIR := build

SOURCES  := binding.cpp \
            database.cpp \
            engine.cpp \
            interpreter.cpp \
            lineedit.cpp \
            main.cpp \
            node.cpp \
            parser.cpp \
            query.cpp \
            server.cpp \
            structure.cpp \
            system.cpp \
            timer.cpp

OBJECTS  := $(addprefix $(BUILDDIR)/,$(SOURCES:.cpp=.o))
DEPS     := $(OBJECTS:.o=.d)

.PHONY: all debug test docs pdf clean

all: $(TARGET)

test: $(TARGET)
	@./tests/run_tests.sh

# The only target that needs more than a C++ compiler: node/npx for marked
# and mermaid-cli, and a chrome build for mermaid-cli to render with.  The
# generated html is checked in, so this only has to be run after editing
# docs/c4-model.md.  Mermaid lays out through a headless browser and is not
# byte reproducible, so a rebuild always shows a small cosmetic diff.
docs: docs/c4-model.html

docs/c4-model.html: docs/c4-model.md docs/build-html.py
	@python3 docs/build-html.py

# the pdf is that same page printed by headless chrome, so the page breaks are
# decided by the print stylesheet in build-html.py.  Ghostscript sets the title
# and author afterwards, which chrome does not carry over from the html.
pdf: docs/c4-model.pdf docs/iso-prolog.pdf

docs/c4-model.pdf: docs/c4-model.html docs/build-html.py
	@python3 docs/build-html.py --pdf

# the ISO prolog primer is authored directly as html (fonts embedded, no
# network requests) and printed the same way.  This pdf is checked in - it is
# served by piview as a download - so this only has to be run after editing
# docs/iso-prolog.html.
docs/iso-prolog.pdf: docs/iso-prolog.html docs/build-iso-pdf.sh
	@docs/build-iso-pdf.sh

debug: CXXFLAGS := -std=c++17 $(WARN) -g -O0 -D_DEBUG -pthread
debug: clean $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

-include $(DEPS)
