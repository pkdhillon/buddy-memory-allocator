# Buddy Memory Allocator

A buddy-system memory allocator implemented in C that simulates dynamic memory allocation using recursive block splitting and coalescing.

The allocator manages a fixed 512 KB memory pool and divides memory into power-of-two blocks based on allocation requests.

## Features

- Implements the buddy memory allocation algorithm
- Manages a simulated 512 KB memory pool
- Supports allocations with a minimum block size of 4 KB
- Rounds allocation requests to valid power-of-two block sizes
- Recursively splits larger blocks to satisfy allocation requests
- Tracks blocks using a binary tree structure
- Supports memory deallocation
- Recursively coalesces adjacent free buddy blocks
- Prevents invalid and duplicate deallocation from corrupting the allocator
- Displays the current memory allocation tree for testing and visualization

## How It Works

The allocator begins with a single 512 KB free memory block.

When memory is requested, the allocator recursively divides larger blocks into two equal-sized buddy blocks until an appropriately sized block is available.

For example:

```text
512 KB
├── 256 KB
│   ├── 128 KB
│   │   ├── 64 KB
│   │   │   ├── 32 KB
│   │   │   │   ├── 16 KB
│   │   │   │   └── 16 KB
```

If a requested size does not correspond to a valid buddy block size, it is rounded up to the next available power-of-two block.

Examples:

```text
1 KB  → 4 KB
8 KB  → 8 KB
12 KB → 16 KB
20 KB → 32 KB
```

When memory is deallocated, the allocator checks whether the corresponding buddy block is also free. If both buddies are free, they are merged into their parent block. This process continues recursively when possible.

## Block Representation

Each memory block is represented by a node containing information about its current state:

```c
typedef struct Node {
    bool is_free;
    bool is_split;
    struct Node* left;
    struct Node* right;
    struct Node* parent;
    size_t size;
    size_t mempool_offset;
} Node;
```

The binary tree tracks how the original memory pool has been divided.

## Memory Pool

The allocator uses a simulated memory pool:

```c
#define TOTAL_MEMORY (512 * 1024)
#define MIN_BLOCK_SIZE (4 * 1024)
```

Pointers returned by the allocator correspond to offsets within this memory pool.

## Testing

The included test program demonstrates:

- 16 KB allocation
- 8 KB allocation
- 12 KB request rounded to 16 KB
- Deallocation and buddy coalescing
- Allocation requests larger than the available memory
- Duplicate deallocation handling
- Full 512 KB allocation
- 1 KB request rounded to the minimum 4 KB block

The allocation tree is printed after operations so changes to the allocator can be observed directly.

Example:

```text
Allocating 16KB

FS (512K)
  FS (256K)
    FS (128K)
      FS (64K)
        FS (32K)
          A (16K)
          F (16K)
        F (32K)
      F (64K)
    F (128K)
  F (256K)
```

Where:

- `F` = Free block
- `A` = Allocated block
- `FS` = Split block

## Build and Run

Compile using GCC:

```bash
gcc -Wall -Wextra -std=c11 buddy.c -o buddy
```

Run:

```bash
./buddy
```

## Concepts Demonstrated

- C programming
- Dynamic memory management
- Pointers and pointer arithmetic
- Binary trees
- Recursive algorithms
- Memory allocation strategies
- Block splitting and coalescing
- Operating systems memory-management concepts
