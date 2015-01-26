rm ./*.img;
make clean;
make;
make client;
sudo cp ./libmfs.so /lib/;
sudo /sbin/ldconfig;
./server 3004 test2.img
