CFLAGS=-isysroot $(SYSROOT) -std=c++20 -Wall -Wextra -Wconversion -Og -g -fno-exceptions

MODULES=bstruct print backend prog1 prog2 parse
OBJECTS=$(MODULES:%=build/%.o)

.PHONY: run

run: build/bstruct
	$<

build/bstruct: $(OBJECTS)
	clang++ -o $@ $(CFLAGS) $^

build/%.o: %.cc
	clang++ -o $@ $(CFLAGS) -MD -c $<

clean:
	rm build/*

-include $(OBJECTS:.o=.d)
