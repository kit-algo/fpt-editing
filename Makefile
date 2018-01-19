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
GENERATED_FILES := build/choices.i

all: $(TARGET)

include $(SOURCE_FILES:src/%.cpp=build/%.d)

debug: $(SOURCE_FILES:src/%.cpp=build/%.o) | build
	$(CPP) $(COMMON) $(DEBUGFLAGS) $^ $(LDFLAGS) -o $(TARGET)

# analyse with gprof [options] ./$(TARGET) gmon.out
profile: $(SOURCE_FILES) | $(SOURCE_FILES:src/%.cpp=build/%.d)
	$(CPP) $(COMMON) $(CPPFLAGS) $(PROFILEFLAGS) $(filter $(SOURCE_FILES), $^) $(LDFLAGS) -o $(TARGET)

release $(TARGET): $(SOURCE_FILES) | $(SOURCE_FILES:src/%.cpp=build/%.d)
	$(CPP) $(COMMON) $(CPPFLAGS) $(RELEASEFLAGS) $(filter $(SOURCE_FILES), $^) $(LDFLAGS) -o $(TARGET)

build:
	find src/ -type d | sed 's/^src/build/' | xargs mkdir

clean:
	rm -rf $(TARGET) build gmon.out *~

build/%.o: src/%.cpp | build/%.d
	$(CPP) $(COMMON) $(CPPFLAGS) $(DEBUGFLAGS) -c $< -o $@

build/%.d: src/%.cpp | build
	$(CPP) $(COMMON) $(CPPFLAGS) -MM -MG -MT $(TARGET) -MT build/$*.o $< | sed 's# ../build/# src/../build/#g' > $@

build/%.i src/../build/%.i: src/main.cpp | build
	for f in $(GENERATED_FILES) ; do [ \! -e $$f ] && touch $$f ; done ; true
	$(CPP) $(COMMON) $(CPPFLAGS) $< -dM -E | grep -E 'CHOICES' | ./choices.py $* > $@
	for f in $(GENERATED_FILES) ; do [ -e $$f -a \! -s $$f ] && rm $$f ; done ; true
