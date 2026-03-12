`cvulkan.h` and `cvulkan.c` written by [botondmester](https://github.com/botondmester)

## Building
```
sudo apt install libvulkan1 vulkan-validationlayers
./compile.sh
./a.out
```

## Running on Wayland (swaywm)
```
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.json ./a.out
```

## Known issues
- vkQueuePresentKHR spins the CPU to 100% usage while waiting for the next VSync'd frame. Need manual sleep
- Resizing can randomly freeze the application on Cinnamon DE, X11
- Doesn't run on my swaywm (wayland) setup without the `VK_DRIVER_FILES=...` thing
