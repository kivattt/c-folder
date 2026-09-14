#gcc -Wall -Wextra -Wconversion -g -O3 -march=native demo.c -o demo -lX11 -lXext -ldl -lrt -pthread $(pkg-config --cflags freetype2) -lfreetype
gcc -Wall -Wextra -Wconversion demo.c -o demo -lX11 -lXext $(pkg-config --cflags freetype2) -lfreetype -lm
