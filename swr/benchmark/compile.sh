gcc -O3 -march=native benchmark.c -o benchmark $(pkg-config --cflags freetype2) -lfreetype
