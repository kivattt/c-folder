Building freetype2 demos
```
cd freetype/
./configure
make
sudo make install

cd ../ft2demos-2.14.2
make

# Binaries are now in the bin/ folder under ft2demos
```

```
./compile.sh
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.json ./main
```
