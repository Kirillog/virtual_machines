# mem_cpy

## Build
In the root of the project:
```sh
cmake -GNinja -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build --target memcpy
```

## Results
```sh
./build/mem_cpy/memcpy

Pool size: 0, time: 37273 microseconds
Pool size: 1, time: 26673 microseconds
Pool size: 2, time: 23934 microseconds
Pool size: 3, time: 24440 microseconds
Pool size: 4, time: 24182 microseconds
Pool size: 5, time: 23898 microseconds
Pool size: 6, time: 23683 microseconds
Pool size: 7, time: 23712 microseconds
Pool size: 8, time: 23508 microseconds
```