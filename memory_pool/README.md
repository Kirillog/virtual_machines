# memory_pool

## Build
In the root of the project:
```sh
cmake -GNinja -Bbuild
cmake --build build --target test
```

## Results
```sh
./build/memory_pool/test MutexedMemPool

MutexedMemPool:
Time used: 54284273 usec
Memory used: 2563198976 bytes
Mem required: 2560000000 bytes
Overhead:  0.1%

./build/memory_pool/test LockFreeMemPool

LockFreeMemPool:
Time used: 24994964 usec
Memory used: 2563203072 bytes
Mem required: 2560000000 bytes
Overhead:  0.1%

./build/memory_pool/test LocalMemPool

LocalMemPool:
Time used: 10083357 usec
Memory used: 2563232768 bytes
Mem required: 2560000000 bytes
Overhead:  0.1%

./build/memory_pool/test Default

Default:
Time used: 16938203 usec
Memory used: 5123203072 bytes
Mem required: 2560000000 bytes
Overhead: 50.0%
```

