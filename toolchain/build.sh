
export TARGET=i386-elf
export PREFIX="/Users/alex/Desktop/NOS/toolchain/output"
export PATH="$PREFIX/bin:$PATH"

cd build
rm -rf binutils || true
rm -rf gcc || true  
mkdir binutils || true
mkdir gcc || true 
cd binutils
../../source/binutils-2.41/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror --with-zstd=no
make -j6
make install 
cd ../gcc
../../source/gcc-13.2.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --disable-werror --without-headers --with-gmp="/opt/homebrew/Cellar/gmp/6.3.0" --with-mpfr="/opt/homebrew/Cellar/mpfr/4.2.1" --with-mpc="/opt/homebrew/Cellar/libmpc/1.3.1"
make all-gcc -j6
make all-target-libgcc -j6
make install-gcc
make install-target-libgcc
