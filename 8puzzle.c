//
// Joseph Prichard 2023
//

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define SIZE 9
#define ROW 3
#define byte char

// TYPE AND FUNCTION DEFINITIONS

typedef enum move {
    NONE, UP, DOWN, LEFT, RIGHT
} move;

typedef byte board[SIZE];

typedef struct puzzle {
    struct puzzle* prev;
    board board;
    move move;
    int g;
    int f;
} puzzle;

typedef struct priority_q {
    puzzle** heap;
    int size;
    int capacity;
} priority_q;

typedef struct hash_table {
    int* table;
    int size;
    int capacity;
    float lf_threshold;
} hash_table;

hash_table* new_ht(int, float);

int is_prime(int);

int next_prime(int);

int hash(const board);

void rehash(hash_table*);

int probe(hash_table*, int, int);

void ht_insert(hash_table* ht, int key);

void ht_probe(hash_table* ht, int key);

int ht_has(hash_table*, int);

priority_q * new_pq(int);

void ensure_capacity(priority_q*);

void push_pq(priority_q*, puzzle*);

puzzle* pop_pq(priority_q*);

// DEBUG TOOLS

void print_ht(hash_table* ht) {
    printf("hash_table\n");
    for (int i = 0; i < ht->capacity; i++) {
        printf("%d ", ht->table[i]);
    }
    printf("\n");
}

void print_pq(priority_q* pq) {
    printf("priority_q\n");
    for (int i = 0; i < pq->size; i++) {
        printf("%d ", pq->heap[i]->f);
    }
    printf("\n");
}

// HASH TABLE IMPLEMENTATION

hash_table* new_ht(int capacity, float lf_threshold) {
    hash_table* ht = (hash_table*) malloc(sizeof(hash_table));
    ht->capacity = next_prime(capacity);
    ht->table = (int*) calloc(ht->capacity, sizeof(int));
    ht->lf_threshold = lf_threshold;
    ht->size = 0;
    if (ht == NULL || ht->table == NULL) {
        printf("Failed to allocate hash_table");
        exit(1);
    }
    return ht;
}

int is_prime(int n) {
    // iterate from 2 to sqrt(n)
    for (int i = 2; i <= sqrt(n); i++) {
        // if n is divisible by any number between 2 and n/2, it is not prime
        if (n % i == 0) {
            return 0;
        }
    }
    if (n <= 1)
        return 0;
    return 1;
}

int next_prime(int n) {
    for (int i = n;; i++) {
        if (is_prime(i)) {
            return i;
        }
    }
}

int hash(const board board) {
    // hash function takes each symbol in the puzzle from start to end as a digit
    int hash = 0;
    for (int i = 0; i < SIZE; i++) {
        hash += board[i] * (int) pow(10, i);
    }
    return hash;
}

int probe(hash_table* ht, int h, int i) {
    return (h + i) % ht->capacity; // linear probe
}

void rehash(hash_table* ht) {
    // keep references to old structures before creating new structures
    int old_capacity = ht->capacity;
    int* old_table = ht->table;
    // allocate a new hash table and rehash all old elements into it
    ht->capacity = next_prime(ht->capacity * 2);
    ht->table = (int*) calloc(ht->capacity, sizeof(int));
    // check for allocation errors
    if (ht->table == NULL) {
        printf("hash_table reallocation failed");
        exit(1);
    }
    // add all keys from the old to the new heap
    for (int i = 0; i < old_capacity; i++) {
        if (old_table[i] != 0) {
            ht_probe(ht, old_table[i]);
        }
    }
    // free the old hash table
    free(old_table);
}

void ht_insert(hash_table* ht, int key) {
    // rehash when load factor exceeds threshold
    if ((float) ht->size / (float) ht->capacity > ht->lf_threshold) {
        rehash(ht);
    }
    ht_probe(ht, key);
    ht->size++;
}

void ht_probe(hash_table* ht, int key) {
    // probe until we find a slot to insert
    for (int i = 0;; i++) {
        int p = probe(ht, key, i);
        if (ht->table[p] == 0) {
            ht->table[p] = key;
            break;
        }
    }
}

int ht_has(hash_table* ht, int key) {
    // probe until we find a match or the first empty slot
    for (int i = 0;; i++) {
        int p = probe(ht, key, i);
        if (ht->table[p] == 0) {
            // probed until empty slot so board isn't in table
            return 0;
        } else if (ht->table[p] == key) {
            // hash values match so board is in table
            return 1;
        }
    }
}

// PQ IMPLEMENTATION

priority_q* new_pq(int capacity) {
    priority_q* pq = (priority_q*) malloc(sizeof(priority_q));
    pq->capacity = capacity;
    pq->heap = (puzzle**) malloc(sizeof(puzzle) * pq->capacity);
    pq->size = 0;
    if (pq == NULL || pq->heap == NULL) {
        printf("Failed to allocate priority_q");
        exit(1);
    }
    return pq;
}

void ensure_capacity(priority_q* pq) {
    // ensure heap's capacity is large enough
    if (pq->size >= pq->capacity) {
        pq->capacity = pq->capacity * 2;
        pq->heap = (puzzle**) realloc(pq->heap, sizeof(puzzle) * pq->capacity);
        // check for allocation errors
        if (pq->heap == NULL) {
            printf("priority_q reallocation failed");
            exit(1);
        }
    }
}

void push_pq(priority_q* pq, puzzle* puz) {
    ensure_capacity(pq);
    // add element to end of heap
    pq->heap[pq->size] = puz;
    // sift the heap up
    int pos = pq->size;
    int parent = (pos - 1) / 2;
    // sift up until parent score is larger
    while (parent >= 0) {
        if (pq->heap[pos]->f > pq->heap[parent]->f) {
            // swap parent with child
            puzzle* temp = pq->heap[pos];
            pq->heap[pos] = pq->heap[parent];
            pq->heap[parent] = temp;
            // climb up the tree
            pos = parent;
            parent = (pos - 1) / 2;
        } else {
            parent = -1;
        }
    }
    pq->size++;
}

puzzle* pop_pq(priority_q* pq) {
    // check for empty heap
    if (pq->size == 0) {
        printf("Can't pop an empty heap.");
        exit(1);
    }
    // extract top element and move bottom to top
    puzzle* top = pq->heap[0];
    pq->heap[0] = pq->heap[pq->size - 1];
    // sift top element down
    int pos = 0;
    for (;;) {
        int left = 2 * pos + 1;
        if (left >= pq->size) {
            break;
        }
        int right = 2 * pos + 2;
        int child = right >= pq->size || pq->heap[left]->f > pq->heap[right]->f ? left : right;
        if (pq->heap[pos]->f < pq->heap[child]->f) {
            // swap parent with child
            puzzle* temp = pq->heap[pos];
            pq->heap[pos] = pq->heap[child];
            pq->heap[child] = temp;
            // climb down tree
            pos = child;
        } else {
            break;
        }
    }
    pq->size--;
    return top;
}

// PUZZLE SOLVER IMPLEMENTATION

puzzle* new_puzzle(const board brd) {
    puzzle* puz = (puzzle*) malloc(sizeof(puzzle));
    if (puz == NULL) {
        printf("Failed to allocate he");
        exit(1);
    }
    memcpy(puz->board, brd, sizeof(board));
    puz->move = NONE;
    puz->prev = NULL;
    puz->f = 0;
    puz->g = 0;
    return puz;
}

void print_puzzle(puzzle* puz) {
    for (int i = 0; i < SIZE; i++) {
        printf("%d, ", puz->board[i]);
        if ((i + 1) % 3 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}

int manhattan_distance(int row1, int row2, int col1, int col2) {
    return abs(row2 - row1) + abs(col2 - col1);
}

int heuristic(const board brd) {
    int h = 0;
    for (int i = 0; i < SIZE; i++) {
        h += manhattan_distance(i % ROW, i / ROW, brd[i] / SIZE, brd[i] % SIZE);
    }
    return h;
}

puzzle* next_puzzle(puzzle* puz) {
    puzzle* next_puz = new_puzzle(puz->board);
    next_puz->prev = puz;
    next_puz->g = puz->g + 1;
    next_puz->f = next_puz->g + heuristic(next_puz->board);
    return next_puz;
}

puzzle* move_up(puzzle* puz) {
    puzzle* next_puz = next_puzzle(puz);
    next_puz->move = UP;
    return next_puz;
}

void solve(const board brd) {
    puzzle* root = new_puzzle(brd);
    priority_q* pq = new_pq(1000);
    hash_table* ht = new_ht(1000, 0.7f);

    push_pq(pq, root);
    print_puzzle(root);

}

int main() {
    srand(time(0));

    char brd[SIZE] = {6, 4, 7, 8, 5, 0, 3, 2, 1};
    solve(brd);

    hash_table* ht = new_ht(10, 0.7f);
    for (int i = 0; i < 20; i++) {
        ht_insert(ht, hash(brd));
    }
    print_ht(ht);
    printf("%d\n", ht_has(ht, hash(brd)));

    return 0;
}
