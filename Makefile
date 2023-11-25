CFLAGS=-isysroot $(SYSROOT) -std=c++20 -Wall -Wextra -Wconversion -Ofast

%.o: %.cc
	clang++ -o $@ $(CFLAGS) -MD -c $<

OBJECTS=bstruct.o bytes.o backend.o

bstruct: $(OBJECTS)
	clang++ -o $@ $(CFLAGS) $^

-include $(OBJECTS:.o=.d)
