# This gets run from the toplevel folder, so need to go into the boot folder
cd boot

rm output/* && true
cd bootloader
./build.sh
cd ../x86
./build.sh
cd ../output
cp * ../../sysroot/System
cd ..