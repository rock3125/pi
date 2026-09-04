EMXX ?= em++

ROOT := ..
OUT := pi.js
SOURCES := \
	$(ROOT)/src/binding.cpp \
	$(ROOT)/src/database.cpp \
	$(ROOT)/src/engine.cpp \
	$(ROOT)/src/interpreter.cpp \
	$(ROOT)/src/node.cpp \
	$(ROOT)/src/parser.cpp \
	$(ROOT)/src/query.cpp \
	$(ROOT)/src/structure.cpp \
	$(ROOT)/src/system.cpp \
	$(ROOT)/src/timer.cpp \
	pi_wasm.cpp

CXXFLAGS := -std=c++17 -O3 -I$(ROOT)/src -Wall -Wno-sign-compare -Wno-switch
EMFLAGS := --bind \
	-s WASM=1 \
	-s MODULARIZE=1 \
	-s EXPORT_NAME=createPiModule \
	-s ENVIRONMENT=web \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s ASSERTIONS=0

.PHONY: all clean serve debug

all: $(OUT)

$(OUT): $(SOURCES)
	$(EMXX) $(CXXFLAGS) $(SOURCES) $(EMFLAGS) -o $@

debug: CXXFLAGS := -std=c++17 -O0 -gsource-map -I$(ROOT)/src -Wall -Wno-sign-compare -Wno-switch
debug: EMFLAGS += -s ASSERTIONS=2
debug: clean all

serve: all
	python3 -m http.server 8080

clean:
	rm -f pi.js pi.wasm pi.js.map pi.wasm.map
