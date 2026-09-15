#valgrind --leak-check=full --show-leak-kinds=definite,indirect --track-origins=yes --log-file=valgrind-out.txt ./test
valgrind --leak-check=full --show-leak-kinds=definite,indirect --track-origins=yes ./test
