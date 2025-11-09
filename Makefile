CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -pthread
LDFLAGS = -pthread

# Targets
#DEMO = demo
#DEMO_OBJS = demo.o
# demo has been deprecated
EXAMPLES = examples
TEST = test
OBJS = thread_pool.o
EXAMPLE_OBJS = examples.o
TEST_OBJS = test.o

all: $(EXAMPLES) $(TEST) $(DEMO)

$(EXAMPLES): $(OBJS) $(EXAMPLE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(TEST): $(OBJS) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

#$(DEMO): $(OBJS) $(DEMO_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

thread_pool.o: thread_pool.cpp thread_pool.h
	$(CXX) $(CXXFLAGS) -c thread_pool.cpp

examples.o: examples.cpp thread_pool.h
	$(CXX) $(CXXFLAGS) -c examples.cpp

test.o: test.cpp thread_pool.h
	$(CXX) $(CXXFLAGS) -c test.cpp

#demo.o: demo.cpp thread_pool.h
#	$(CXX) $(CXXFLAGS) -c demo.cpp

clean:
	rm -f $(OBJS) $(EXAMPLE_OBJS) $(TEST_OBJS) $(DEMO_OBJS) $(EXAMPLES) $(TEST) $(DEMO)

run: $(EXAMPLES)
	./$(EXAMPLES)

check: $(TEST)
	./$(TEST)

#demo: $(DEMO)
#	./$(DEMO)

.PHONY: all clean run check demo
