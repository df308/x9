#/bin/bash

echo "- Running examples with GCC with \"-fsanitize=thread,undefined\" enabled.";
gcc -Wextra -Wall -Werror -pedantic -O3 -march=native x9_example_1.c ../x9.c -o X9_TEST_1 -fsanitize=thread,undefined -D X9_DEBUG
gcc -Wextra -Wall -Werror -pedantic -O3 -march=native x9_example_2.c ../x9.c -o X9_TEST_2 -fsanitize=thread,undefined -D X9_DEBUG
gcc -Wextra -Wall -Werror -pedantic -O3 -march=native x9_example_3.c ../x9.c -o X9_TEST_3 -fsanitize=thread,undefined -D X9_DEBUG
gcc -Wextra -Wall -Werror -pedantic -O3 -march=native x9_example_4.c ../x9.c -o X9_TEST_4 -fsanitize=thread,undefined -D X9_DEBUG
gcc -Wextra -Wall -Werror -pedantic -O3 -march=native x9_example_5.c ../x9.c -o X9_TEST_5 -fsanitize=thread,undefined -D X9_DEBUG
gcc -Wextra -Wall -Werror -pedantic -O3 -march=native x9_example_6.c ../x9.c -o X9_TEST_6 -fsanitize=thread,undefined -D X9_DEBUG

./X9_TEST_1; ./X9_TEST_2; ./X9_TEST_3; ./X9_TEST_4; ./X9_TEST_5; ./X9_TEST_6
rm X9_TEST_1 X9_TEST_2 X9_TEST_3 X9_TEST_4 X9_TEST_5 X9_TEST_6

echo ""
echo "- Running examples with clang with \"-fsanitize=address,undefined,leak\" enabled.";

clang -Wextra -Wall -Werror -O3 -march=native x9_example_1.c ../x9.c -o X9_TEST_1 -fsanitize=address,undefined,leak -D X9_DEBUG
clang -Wextra -Wall -Werror -O3 -march=native x9_example_2.c ../x9.c -o X9_TEST_2 -fsanitize=address,undefined,leak -D X9_DEBUG
clang -Wextra -Wall -Werror -O3 -march=native x9_example_3.c ../x9.c -o X9_TEST_3 -fsanitize=address,undefined,leak -D X9_DEBUG
clang -Wextra -Wall -Werror -O3 -march=native x9_example_4.c ../x9.c -o X9_TEST_4 -fsanitize=address,undefined,leak -D X9_DEBUG
clang -Wextra -Wall -Werror -O3 -march=native x9_example_5.c ../x9.c -o X9_TEST_5 -fsanitize=address,undefined,leak -D X9_DEBUG
clang -Wextra -Wall -Werror -O3 -march=native x9_example_6.c ../x9.c -o X9_TEST_6 -fsanitize=address,undefined,leak -D X9_DEBUG

./X9_TEST_1; ./X9_TEST_2; ./X9_TEST_3; ./X9_TEST_4; ./X9_TEST_5; ./X9_TEST_6
rm X9_TEST_1 X9_TEST_2 X9_TEST_3 X9_TEST_4 X9_TEST_5 X9_TEST_6 

