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

Default memcpy time:     24050 microseconds
Pool size: 0, time:      23851 microseconds
Pool size: 1, time:      21612 microseconds
Pool size: 2, time:      22566 microseconds
Pool size: 3, time:      22394 microseconds
Pool size: 4, time:      22185 microseconds
Pool size: 5, time:      22517 microseconds
Pool size: 6, time:      21523 microseconds
Pool size: 7, time:      22219 microseconds
Pool size: 8, time:      22032 microseconds
```