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

Pool size: 0, time: 30201 microseconds
Pool size: 1, time: 26210 microseconds
Pool size: 2, time: 23843 microseconds
Pool size: 3, time: 23636 microseconds
Pool size: 4, time: 24479 microseconds
Pool size: 5, time: 24185 microseconds
Pool size: 6, time: 24376 microseconds
Pool size: 7, time: 24504 microseconds
Pool size: 8, time: 24836 microseconds
```