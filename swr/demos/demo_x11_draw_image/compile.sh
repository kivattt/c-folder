CC="${CC:-gcc}"
$CC -g -gdwarf-4 -O3 -march=native demo.c -o demo -lX11 -lXext $(pkg-config --cflags freetype2) -lfreetype -lm $(pkg-config --cflags libpng)
