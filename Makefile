CFLAGS=-isysroot $(SYSROOT) -std=c++20 -Wall -Wextra -Wconversion -Og -g -fno-exceptions

%.o: %.cc
	clang++ -o $@ $(CFLAGS) -MD -c $<

OBJECTS=bstruct.o common.o backend.o prog1.o prog2.o

.PHONY: run

run: bstruct
	./bstruct

bstruct: $(OBJECTS)
	clang++ -o $@ $(CFLAGS) $^

clean:
	rm *.o *.d bstruct

-include $(OBJECTS:.o=.d)
