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

Default memcpy time:     23758 microseconds
Pool size: 0, time:      28343 microseconds
Pool size: 1, time:      23150 microseconds
Pool size: 2, time:      21848 microseconds
Pool size: 3, time:      22449 microseconds
Pool size: 4, time:      22229 microseconds
Pool size: 5, time:      21889 microseconds
Pool size: 6, time:      21637 microseconds
Pool size: 7, time:      21691 microseconds
Pool size: 8, time:      21917 microseconds
```