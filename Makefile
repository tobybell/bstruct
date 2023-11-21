bstruct: bstruct.cc
	clang++ -o $@ -std=c++17 -Wall -Wextra -Wconversion -Ofast $<
