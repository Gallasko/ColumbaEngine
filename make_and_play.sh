cd release
make -j8 ${1}
cd ..
./release/${1}
