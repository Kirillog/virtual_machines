# string_refcount

## Build
In the root of the project:
```sh
cmake -GNinja -Bbuild
cmake --build build --target string_refcount
```

## Test
```sh
cmake --build build --target string_test
./build/string_refcount/string_test
```