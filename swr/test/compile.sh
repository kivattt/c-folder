gcc -Wall -Wextra -Wconversion -O3 -march=native test.c -o test $(pkg-config --cflags freetype2) -lfreetype -lm
