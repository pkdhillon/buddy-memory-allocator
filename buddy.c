#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define TOTAL_MEMORY (512 * 1024) // 512KB total memory pool
#define MIN_BLOCK_SIZE (4 * 1024) // 4KB
#define MAX_LEVELS 7 // Since 512K / 2^7 = 4K

typedef struct Node {
    bool is_free;               // True if this block is currently unallocated
    bool is_split;              // True if this block has been split into two smaller blocks
    struct Node* left;          // Pointer to the left child (first half of the split)
    struct Node* right;         // Pointer to the right child (second half of the split)
    struct Node* parent;        // Pointer to the parent node (used for merging)
    size_t size;                // Size of the block in bytes
    size_t mempool_offset;      // Offset in memory_pool representing this block
} Node;


typedef struct {
    Node* root;
    char memory_pool[TOTAL_MEMORY];
} BuddyAllocator;


void print_node_details(Node* node, const char* message) {
    if (node == NULL) {
        printf("%s: Node is NULL\n", message);
        return;
    }

    printf("%s: Node size=%zuK, offset=%zuK, is_split=%d, is_free=%d\n",
           message,
           node->size / 1024,
           node->mempool_offset / 1024,
           node->is_split,
           node->is_free);
}


void print_tree(Node* node, int depth) {
    if (!node) return;

    for (int i = 0; i < depth; i++) {
        printf("  ");
    }

    if (!node->is_split) {
        printf("%s (%zuK)\n",
               node->is_free ? "F" : "A",
               node->size / 1024);
    } else {
        printf("FS (%zuK)\n", node->size / 1024);
    }

    print_tree(node->left, depth + 1);
    print_tree(node->right, depth + 1);
}


Node* create_node(size_t size, size_t mempool_offset, Node* parent) {
    Node* node = malloc(sizeof(Node));

    if (!node) {
        return NULL;
    }

    node->is_free = true;
    node->is_split = false;
    node->left = NULL;
    node->right = NULL;
    node->parent = parent;
    node->size = size;
    node->mempool_offset = mempool_offset;

    return node;
}


BuddyAllocator* create_allocator() {
    BuddyAllocator* allocator = malloc(sizeof(BuddyAllocator));

    if (!allocator) {
        return NULL;
    }

    allocator->root = create_node(TOTAL_MEMORY, 0, NULL);

    if (!allocator->root) {
        free(allocator);
        return NULL;
    }

    return allocator;
}


void split(Node* node) {
    if (!node || node->is_split || node->size <= MIN_BLOCK_SIZE) {
        return;
    }

    size_t half_size = node->size / 2;

    node->left = create_node(
        half_size,
        node->mempool_offset,
        node
    );

    node->right = create_node(
        half_size,
        node->mempool_offset + half_size,
        node
    );

    // If either allocation failed, clean up and leave node unsplit
    if (!node->left || !node->right) {
        free(node->left);
        free(node->right);

        node->left = NULL;
        node->right = NULL;

        return;
    }

    node->is_split = true;
}


// Round a requested allocation size up to the nearest valid buddy block size
size_t round_block_size(size_t size) {
    size_t block_size = MIN_BLOCK_SIZE;

    while (block_size < size && block_size < TOTAL_MEMORY) {
        block_size *= 2;
    }

    return block_size;
}


Node* allocate_recursive(Node* node, size_t size) {
    if (!node) {
        return NULL;
    }

    // If this is an allocated leaf block, it cannot be used
    if (!node->is_split && !node->is_free) {
        return NULL;
    }

    // Exact-sized free leaf block found
    if (node->size == size &&
        !node->is_split &&
        node->is_free) {

        node->is_free = false;
        return node;
    }

    // Split larger blocks until we reach the requested buddy size
    if (node->size > size &&
        node->size > MIN_BLOCK_SIZE) {

        if (!node->is_split) {
            split(node); // Split the node if not split already
        }
    }

    // Search the left subtree first
    Node* allocated_node =
        allocate_recursive(node->left, size);

    // If left side could not satisfy the request, search right
    if (!allocated_node) {
        allocated_node =
            allocate_recursive(node->right, size);
    }

    return allocated_node;
}


void* allocate(BuddyAllocator* allocator, size_t size) {
    if (!allocator || size == 0 || size > TOTAL_MEMORY) {
        return NULL;
    }

    // Any request smaller than 4KB will use at least one 4KB block.
    // Other sizes are rounded up to the nearest power-of-two block size.
    size = round_block_size(size);

    Node* node =
        allocate_recursive(allocator->root, size);

    if (!node) {
        return NULL;
    }

    return allocator->memory_pool + node->mempool_offset;
}


void coalesce(Node* node) {
    if (!node || !node->parent) {
        return;
    }

    Node* parent = node->parent;
    Node* left = parent->left;
    Node* right = parent->right;

    // Merge siblings only when both are unsplit and free
    if (left &&
        right &&
        left->is_free &&
        right->is_free &&
        !left->is_split &&
        !right->is_split) {

        free(left);
        free(right);

        parent->left = NULL;
        parent->right = NULL;
        parent->is_free = true;
        parent->is_split = false;

        // Continue merging upward if possible
        coalesce(parent);
    }
}


void free_recursive(Node* node) {
    if (!node) {
        return;
    }

    node->is_free = true;

    if (!node->is_split) {
        coalesce(node);
    }
}


Node* find_node(Node* node, size_t mempool_offset) {
    if (!node) {
        return NULL;
    }

    if (node->mempool_offset == mempool_offset &&
        !node->is_split) {

        return node;
    }

    if (node->is_split) {
        Node* result =
            find_node(node->left, mempool_offset);

        if (result) {
            return result;
        }

        return find_node(node->right, mempool_offset);
    }

    return NULL;
}


void deallocate(BuddyAllocator* allocator, void* ptr) {
    if (!allocator || !ptr) {
        return;
    }

    if ((char*)ptr < allocator->memory_pool ||
        (char*)ptr >= allocator->memory_pool + TOTAL_MEMORY) {

        return;
    }

    size_t offset =
        (char*)ptr - allocator->memory_pool;

    Node* node =
        find_node(allocator->root, offset);

    // Prevent double-free by only freeing allocated nodes
    if (node && !node->is_free) {
        free_recursive(node);
    }
}


void destroy_tree(Node* node) {
    if (!node) {
        return;
    }

    destroy_tree(node->left);
    destroy_tree(node->right);

    free(node);
}


void destroy_allocator(BuddyAllocator* allocator) {
    if (!allocator) {
        return;
    }

    destroy_tree(allocator->root);
    free(allocator);
}


#ifndef NOMAIN

int main() {
    BuddyAllocator* allocator = create_allocator();

    if (!allocator) {
        printf("Failed to create buddy allocator.\n");
        return 1;
    }

    printf("\nAllocating 16KB\n");
    void* block1 = allocate(allocator, 16 * 1024);
    printf("Block1: %p\n", block1);
    print_tree(allocator->root, 0);


    printf("\nAllocating 8KB\n");
    void* block2 = allocate(allocator, 8 * 1024);
    printf("Block2: %p\n", block2);
    print_tree(allocator->root, 0);


    printf("\nAllocating 12KB (rounds to 16KB)\n");
    void* block3 = allocate(allocator, 12 * 1024);
    printf("Block3: %p\n", block3);
    print_tree(allocator->root, 0);


    printf("\nFreeing 16KB (block1)\n");
    deallocate(allocator, block1);
    print_tree(allocator->root, 0);


    printf("\nFreeing 8KB (block2)\n");
    deallocate(allocator, block2);
    print_tree(allocator->root, 0);


    printf("\nFreeing 12KB (block3, rounds to 16KB)\n");
    deallocate(allocator, block3);
    print_tree(allocator->root, 0);


    printf("\nAllocating 1MB (should fail)\n");
    void* big_block =
        allocate(allocator, 1024 * 1024);

    printf("Big block: %p (expected NULL)\n", big_block);
    print_tree(allocator->root, 0);


    printf("\nDouble freeing 16KB (block1)\n");
    deallocate(allocator, block1);
    print_tree(allocator->root, 0);


    printf("\nAllocating 512KB (full memory)\n");
    void* full_block =
        allocate(allocator, 512 * 1024);

    printf("Full block: %p\n", full_block);
    print_tree(allocator->root, 0);


    // Free the full block so another allocation can be demonstrated
    if (full_block) {
        printf("\nFreeing 512KB full block\n");
        deallocate(allocator, full_block);
        print_tree(allocator->root, 0);
    }


    printf("\nAllocating 1KB (should round to 4KB)\n");
    void* small_block =
        allocate(allocator, 1 * 1024);

    printf("Small block: %p\n", small_block);
    print_tree(allocator->root, 0);


    destroy_allocator(allocator);

    return 0;
}

#endif // NOMAIN