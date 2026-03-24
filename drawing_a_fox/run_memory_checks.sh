VK_DRIVER_FILES=/usr/share/vulkan/icd.d/nvidia_icd.json valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./main
