# High-Performance Thread Pool with Work Stealing

A C++17 thread pool implementation featuring work-stealing scheduling for optimal load balancing and high throughput task execution.

## Performance

- **~5x faster** than `std::async` for task submission
- **500k+ tasks/sec** sustained throughput

## Quick Start

### Build
```bash
make
```

This compiles:
- `examples` - Usage demonstrations and benchmarks
- `test` - Comprehensive test suite

### Run Examples & Benchmarks
```bash
make run
# or
./examples
```

### Run Tests
```bash
make check
# or
./test
```

### Clean Build
```bash
make clean
```

## Requirements

- C++17 or later
- pthread support
- GCC/Clang with `-O3` optimization recommended

## Example benchmark:
<img width="363" height="258" alt="image" src="https://github.com/user-attachments/assets/ca27875d-0d1d-480f-841e-46b0ea96f4e3" />
