#valgrind --leak-check=full --show-leak-kinds=definite,indirect --track-origins=yes --log-file=valgrind-out.txt ./demo
valgrind --leak-check=full --show-leak-kinds=definite,indirect --track-origins=yes ./demo
