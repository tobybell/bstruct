CFLAGS=-isysroot $(SYSROOT) -std=c++20 -Wall -Wextra -Wconversion -Og -g

%.o: %.cc
	clang++ -o $@ $(CFLAGS) -MD -c $<

OBJECTS=bstruct.o common.o backend.o prog1.o

.PHONY: run

run: bstruct
	./bstruct

bstruct: $(OBJECTS)
	clang++ -o $@ $(CFLAGS) $^

-include $(OBJECTS:.o=.d)
