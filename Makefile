CPP := g++
DEBUGFLAGS := -ggdb -Og
RELEASEFLAGS := -O3 -DNDEBUG
PROFILEFLAGS := -ggdb $(RELEASEFLAGS)
COMMON := -std=c++14 -pedantic -W -Wall -Wextra -march=native -fopenmp
CPPFLAGS :=
LDFLAGS := -lpthread

TARGET := graphedit

SOURCE_FILES := $(sort $(shell find src/ -name "*.cpp"))
HEADER_FILES := $(sort $(shell find src/ -name "*.hpp"))
TEMPLATED_FILES := $(sort $(shell find build/generated/ -name "*.cpp"))
GENERATED_FILES := build/choices.i

all: release

ifneq ($(MAKECMDGOALS), clean)
include $(SOURCE_FILES:src/%.cpp=build/%.d)
include build/generated/generated.d
endif

debug: TYPEFLAGS := $(DEBUGFLAGS)
debug: $(TARGET)
profile: TYPEFLAGS := $(PROFILEFLAGS)
profile: $(TARGET)
release: TYPEFLAGS := $(RELEASEFLAGS)
release: $(TARGET)

clean:
	rm -rf $(TARGET) build gmon.out *~

$(TARGET): $(SOURCE_FILES:src/%.cpp=build/%.o) $(TEMPLATED_FILES:%.cpp=%.o) | build
	$(CPP) $(COMMON) $(TYPEFLAGS) $^ $(LDFLAGS) -o $(TARGET)

build:
	find src/ -type d | sed 's/^src/build/' | xargs mkdir
	mkdir build/generated

build/generated/generated.d: | $(GENERATED_FILES)
	$(CPP) $(COMMON) $(CPPFLAGS) $(RELEASEFLAGS) -MM -MG -MT build/generated/%.o $(lastword $(shell find build/generated/ -name "*.cpp")) | sed 's#build/generated/.*\.cpp#build/generated/%.cpp#' > $@
build/%.d: src/%.cpp | build
	$(CPP) $(COMMON) $(CPPFLAGS) $(RELEASEFLAGS) -MM -MG -MT build/$*.o $< | sed 's# ../build/# src/../build/#g' > $@

build/generated/%.o: build/generated/%.cpp | build/generated/generated.d
	$(CPP) $(COMMON) $(CPPFLAGS) $(TYPEFLAGS) -c $< -o $@
build/%.o: src/%.cpp | build/%.d $(GENERATED_FILES)
	$(CPP) $(COMMON) $(CPPFLAGS) $(TYPEFLAGS) -c $< -o $@

build/generated/%.cpp build/%.i src/../build/%.i: src/config.hpp | build
	for f in $(GENERATED_FILES) ; do [ \! -e $$f ] && touch $$f ; done ; true
	$(CPP) $(COMMON) $(CPPFLAGS) $(RELEASEFLAGS) $< -dM -E | grep -E 'CHOICES' | ./choices.py $* > $@
	for f in $(GENERATED_FILES) ; do [ -e $$f -a \! -s $$f ] && rm $$f ; done ; true
	touch build/generated/generated.d
