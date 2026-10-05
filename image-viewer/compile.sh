CC="${CC:-gcc}"
#$CC -g -gdwarf-4 -O3 -march=native main.c -o img -lX11 -lXext $(pkg-config --cflags freetype2) -lfreetype -lm $(pkg-config --cflags libpng)
$CC -g -Og -march=native main.c -o img -lX11 -lXext $(pkg-config --cflags freetype2) -lfreetype -lm $(pkg-config --cflags libpng)
