CPP := g++
DEBUGFLAGS := -ggdb -O0
RELEASEFLAGS := -O3 -DNDEBUG
PROFILEFLAGS := -ggdb $(RELEASEFLAGS)
COMMON := -std=c++17 -pedantic -W -Wall -Wextra -march=native
CPPFLAGS :=
LDFLAGS := -lpthread

############################################################

TARGET := graphedit

SOURCE_FILES := src/Graph/Graph.cpp src/main.cpp
TEST_SOURCE_FILES := src/Graph/Graph.cpp src/test_finder.cpp
GUROBI_SOURCE_FILES := src/Graph/Graph.cpp src/main_gurobi.cpp
HEADER_FILES := $(sort $(shell find src/ -name "*.hpp"))

all: release

ifneq ($(MAKECMDGOALS), clean)
-include $(SOURCE_FILES:src/%.cpp=build/%.d)
-include $(TEST_SOURCE_FILES:src/%.cpp=build/%.d)
-include build/generated/generated.d
-include build/generated/list.d
endif

debug: TYPEFLAGS := $(DEBUGFLAGS)
debug: $(TARGET) test_finder json2csv
profile: TYPEFLAGS := $(PROFILEFLAGS)
profile: $(TARGET) test_finder json2csv
release: TYPEFLAGS := $(RELEASEFLAGS)
release: $(TARGET) test_finder json2csv

clean:
	rm -rf $(TARGET) build gmon.out test_finder json2csv *~

test_finder: $(TEST_SOURCE_FILES:src/%.cpp=build/%.o)
	$(CPP) $(COMMON) $(TYPEFLAGS) $^ $(LDFLAGS) -o test_finder

json2csv: json2csv.cpp external/json.hpp
	$(CPP) $(COMMON) $(TYPEFLAGS) json2csv.cpp $(LDFLAGS) -o json2csv

edit_gurobi: $(GUROBI_SOURCE_FILES:src/%.cpp=build/%.o)
	$(CPP) $(COMMON) $(TYPEFLAGS) $^ $(LDFLAGS) -I/opt/gurobi/linux64/include/ -L/opt/gurobi/linux64/lib/ -lgurobi_c++ -lgurobi80 -o edit_gurobi

$(TARGET): $(SOURCE_FILES:src/%.cpp=build/%.o) | build build/generated/list.d
	$(CPP) $(COMMON) $(TYPEFLAGS) $^ $(LDFLAGS) -o $(TARGET)

build:
	find src/ -type d | sed 's/^src/build/' | xargs mkdir
	mkdir build/generated

build/generated/%.d: build/generated/%.cpp
	$(CPP) $(COMMON) $(CPPFLAGS) $(RELEASEFLAGS) -MM -MG -MT build/generated/$*.o $< | sed 's#build/generated/\.\./\.\./##g' > $@
build/%.d: src/%.cpp | build
	$(CPP) $(COMMON) $(CPPFLAGS) $(RELEASEFLAGS) -MM -MG -MT build/$*.o $< | sed 's# \.\./build/# src/../build/#g' > $@

build/generated/%.o: build/generated/%.cpp | build/generated/generated.d
	$(CPP) $(COMMON) $(CPPFLAGS) $(TYPEFLAGS) -c $< -o $@
build/%.o: src/%.cpp | build/%.d $(GENERATED_FILES)
	$(CPP) $(COMMON) $(CPPFLAGS) $(TYPEFLAGS) -c $< -o $@

build/generated/%.cpp: build/choices.i

build/choices.i build/generated/list.d build/generated/generated.d: src/config.hpp | build
	touch build/choices.i
	$(CPP) $(COMMON) $(CPPFLAGS) $(RELEASEFLAGS) $< -dM -E | grep -E 'CHOICES' | ./choices.py $* > build/choices.i
