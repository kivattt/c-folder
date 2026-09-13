# Compiling
```
gcc $(pkg-config --cflags freetype2) -lfreetype

# On my Linux Mint computer, that is:
gcc -I/usr/local/include/freetype2 -I/usr/include/libpng16 -I/usr/include/harfbuzz -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
```
