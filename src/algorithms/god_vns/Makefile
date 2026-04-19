CC = g++

main : god.cpp model.cpp vns.cpp init_sol.cpp evaluate.cpp utils.cpp io.cpp params.h
	$(CC) -O3 -march=native -flto -DNDEBUG -o god god.cpp evaluate.cpp model.cpp vns.cpp init_sol.cpp utils.cpp io.cpp
