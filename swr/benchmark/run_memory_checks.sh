#valgrind --leak-check=full --show-leak-kinds=definite,indirect --track-origins=yes --log-file=valgrind-out.txt ./benchmark
valgrind --leak-check=full --show-leak-kinds=definite,indirect --track-origins=yes ./benchmark
