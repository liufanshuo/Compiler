make clean && make
./parser test.txt > ast.dot
dot -Tpng ast.dot -o ast.png