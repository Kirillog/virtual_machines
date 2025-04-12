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

Pool size: 0, time: 30931 microseconds
Pool size: 1, time: 26719 microseconds
Pool size: 2, time: 26579 microseconds
Pool size: 3, time: 24693 microseconds
Pool size: 4, time: 25453 microseconds
Pool size: 5, time: 24480 microseconds
Pool size: 6, time: 24526 microseconds
Pool size: 7, time: 24161 microseconds
Pool size: 8, time: 24341 microseconds
```