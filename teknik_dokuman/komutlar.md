## Hızlı Komutlar

### PC build (Windows)
```
cmake -S PC -B PC/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build PC/build --config Release
```

### Raspi build (Linux/Raspberry Pi)
```
cmake -S raspi -B raspi/build -DCMAKE_BUILD_TYPE=Release
cmake --build raspi/build -j
ctest --test-dir raspi/build
```
