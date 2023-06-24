PROTOC = protoc

sources := $(wildcard src/*.cc)
objects := $(sources:.cc=.o)

test_sources := $(wildcard test/*.cc)
test_objects := $(test_sources:.cc=.o)
test_binaries := $(basename $(test_sources))

proto_files := $(wildcard proto/*.proto)
proto_sources := $(proto_files:.proto=.pb.cc)
proto_headers := $(proto_files:.proto=.pb.h)
proto_objects := $(proto_files:.proto=.pb.o)

include_dirs := -I .
include_dirs += -I ./include

headers := $(wildcard include/*.h)
headers += $(proto_headers)

libraries := $(shell pkg-config --cflags --libs protobuf)
test_libraries := $(shell pkg-config --cflags --libs gtest gtest_main)

.PHONY: all clean
all: build testall

# link *.o to a build executable
build: $(objects) $(proto_objects)
	$(CXX) -o build $(objects) $(proto_objects) $(libraries) $(LDFLAGS)

# compile *.cc into *.o
$(objects) $(test_objects): %.o : %.cc $(headers)
	$(CXX) $(CXXFLAGS) $(include_dirs) -c $< -o $@

# compile proto files into *.cc, *.h
$(proto_sources) $(proto_headers): $(proto_files)
	$(PROTOC) -I proto --cpp_out=proto $(proto_files)

# build all the test cases
testall: $(test_objects) $(proto_objects) $(objects)
	$(CXX) -o testall $(filter-out src/main.o, $(objects)) $(proto_objects) $(test_objects) $(libraries) $(test_libraries) $(LDFLAGS)

clean:
	-rm -f proto/*.o proto/*.cc proto/*.h proto/*.d
	-rm -f src/*.o
	-rm -f test/*.o
	-rm -f build testall