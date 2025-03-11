# memory_pool

## Build
In the root of the project:
```sh
cmake -GNinja -Bbuild
cmake --build build --target test
```

## Results
```sh
./build/memory_pool/test MemPool

MemPool:
Time used: 375342 usec
Memory used: 162695168 bytes
Mem required: 160000000 bytes
Overhead:  1.7%

./build/memory_pool/test Default

Default:
Time used: 518690 usec
Memory used: 322915328 bytes
Mem required: 160000000 bytes
Overhead: 50.5%
```

