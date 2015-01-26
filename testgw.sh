gcc -Wall -g -O0 servergw.c -o servergw;
gcc -Wall -g -O0 clientgw.c -o clientgw;
rm ./test2.img;
./servergw 3003 test2.img;
