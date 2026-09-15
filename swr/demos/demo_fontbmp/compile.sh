CC="${CC:-gcc}"
$CC -Wall -Wextra -Wconversion -O3 -march=native demo.c -o demo $(pkg-config --cflags freetype2) -lfreetype -lm
