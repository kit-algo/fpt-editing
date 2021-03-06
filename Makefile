DEBUGFLAGS := -ggdb -O0
RELEASEFLAGS := -O3 -DNDEBUG
PROFILEFLAGS := -ggdb $(RELEASEFLAGS)
COMMON := -std=c++17 -pedantic -W -Wall -Wextra -march=native
CXXFLAGS := -I/opt/gurobi/linux64/include/
LDFLAGS := -lpthread -L/opt/gurobi/linux64/lib/ -lgurobi_c++ -lgurobi81

############################################################

TARGET := graphedit

SOURCE_FILES := src/Graph/Graph.cpp src/main.cpp
HEADER_FILES := $(sort $(shell find src/ -name "*.hpp"))

all: release

ifneq ($(MAKECMDGOALS), clean)
-include $(SOURCE_FILES:src/%.cpp=build/%.d)
-include build/generated/generated.d
-include build/generated/list.d
endif

debug: TYPEFLAGS := $(DEBUGFLAGS)
debug: $(TARGET)
profile: TYPEFLAGS := $(PROFILEFLAGS)
profile: $(TARGET)
release: TYPEFLAGS := $(RELEASEFLAGS)
release: $(TARGET)

clean:
	rm -rf $(TARGET) build gmon.out *~

$(TARGET): $(SOURCE_FILES:src/%.cpp=build/%.o) | build build/generated/list.d
	$(CXX) $(COMMON) $(TYPEFLAGS) $^ $(LDFLAGS) -o $(TARGET)

build:
	find src/ -type d | sed 's/^src/build/' | xargs mkdir
	mkdir build/generated

build/generated/%.d: build/generated/%.cpp
	$(CXX) $(COMMON) $(CXXFLAGS) $(RELEASEFLAGS) -MM -MG -MT build/generated/$*.o $< | sed 's#build/generated/\.\./\.\./##g' > $@
build/%.d: src/%.cpp | build
	$(CXX) $(COMMON) $(CXXFLAGS) $(RELEASEFLAGS) -MM -MG -MT build/$*.o $< | sed 's# \.\./build/# src/../build/#g' > $@

build/generated/%.o: build/generated/%.cpp | build/generated/generated.d
	$(CXX) $(COMMON) $(CXXFLAGS) $(TYPEFLAGS) -c $< -o $@
build/%.o: src/%.cpp | build/%.d $(GENERATED_FILES)
	$(CXX) $(COMMON) $(CXXFLAGS) $(TYPEFLAGS) -c $< -o $@

build/generated/%.cpp: build/choices.i

build/choices.i build/generated/list.d build/generated/generated.d: src/config.hpp | build
	touch build/choices.i
	$(CXX) $(COMMON) $(CXXFLAGS) $(RELEASEFLAGS) $< -dM -E | grep -E 'CHOICES' | ./choices.py $* > build/choices.i
