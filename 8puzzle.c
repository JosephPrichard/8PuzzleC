//
// Joseph Prichard 2023
//

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define SIZE 9
#define ROWS 3
#define tile char
#define NEIGHBOR_CNT 4
#define LONGEST_SOL 32

// TYPE AND FUNCTION DEFINITIONS

typedef enum move {
    NONE, UP, DOWN, LEFT, RIGHT
} move;

typedef tile board[SIZE];

typedef struct puzzle {
    struct puzzle* prev;
    board board;
    move move;
    int g;
    int f;
} puzzle;

typedef struct priority_q {
    puzzle** min_heap;
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

puzzle* new_puzzle(const board);

int find_zero(const board);

puzzle* move_puzzle(puzzle* puz, int row_offset, int col_offset);

int heuristic(const board);

void solve(const board, const board);

void print_puzzle(puzzle*);

void reconstruct_path(puzzle*);

// GLOBALS

static const int NEIGHBOR_OFFSETS[NEIGHBOR_CNT][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
static const int NEIGHBOR_MOVES[NEIGHBOR_CNT] = {RIGHT, DOWN, LEFT, UP};
static const char* MOVE_STRINGS[] = {"None", "Up", "Down", "Left", "Right"};

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
        printf("%d ", pq->min_heap[i]->f);
    }
    printf("\n");
}

// HASH TABLE IMPLEMENTATION

hash_table* new_ht(int capacity, float lf_threshold) {
    hash_table* ht = malloc(sizeof(hash_table));
    ht->capacity = next_prime(capacity);
    ht->table = calloc(ht->capacity, sizeof(int));
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
    ht->table = calloc(ht->capacity, sizeof(int));
    // check for allocation errors
    if (ht->table == NULL) {
        printf("hash_table reallocation failed");
        exit(1);
    }
    // add all keys from the old to the new min_heap
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
    priority_q* pq = malloc(sizeof(priority_q));
    pq->capacity = capacity;
    pq->min_heap = malloc(sizeof(puzzle) * pq->capacity);
    pq->size = 0;
    if (pq == NULL || pq->min_heap == NULL) {
        printf("Failed to allocate priority_q");
        exit(1);
    }
    return pq;
}

void ensure_capacity(priority_q* pq) {
    // ensure min_heap's capacity is large enough
    if (pq->size >= pq->capacity) {
        pq->capacity = pq->capacity * 2;
        pq->min_heap = realloc(pq->min_heap, sizeof(puzzle) * pq->capacity);
        // check for allocation errors
        if (pq->min_heap == NULL) {
            printf("priority_q reallocation failed");
            exit(1);
        }
    }
}

void push_pq(priority_q* pq, puzzle* puz) {
    ensure_capacity(pq);
    // add element to end of min_heap
    pq->min_heap[pq->size] = puz;
    // sift the min_heap up
    int pos = pq->size;
    int parent = (pos - 1) / 2;
    // sift up until parent score is larger
    while (parent >= 0) {
        if (pq->min_heap[pos]->f < pq->min_heap[parent]->f) {
            // swap parent with child
            puzzle* temp = pq->min_heap[pos];
            pq->min_heap[pos] = pq->min_heap[parent];
            pq->min_heap[parent] = temp;
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
    // check for empty min_heap
    if (pq->size == 0) {
        printf("Can't pop an empty min_heap.");
        exit(1);
    }
    // extract top element and move bottom to top
    puzzle* top = pq->min_heap[0];
    pq->min_heap[0] = pq->min_heap[pq->size - 1];
    // sift top element down
    int pos = 0;
    for (;;) {
        int left = 2 * pos + 1;
        if (left >= pq->size) {
            break;
        }
        int right = 2 * pos + 2;
        int child = right >= pq->size || pq->min_heap[left]->f < pq->min_heap[right]->f ? left : right;
        if (pq->min_heap[pos]->f > pq->min_heap[child]->f) {
            // swap parent with child
            puzzle* temp = pq->min_heap[pos];
            pq->min_heap[pos] = pq->min_heap[child];
            pq->min_heap[child] = temp;
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
    puzzle* puz = malloc(sizeof(puzzle));
    if (puz == NULL) {
        printf("Failed to allocate puzzle");
        exit(1);
    }
    memcpy(puz->board, brd, sizeof(board));
    puz->move = NONE;
    puz->prev = NULL;
    puz->f = 0;
    puz->g = 0;
    return puz;
}

int find_zero(const board brd) {
    for (int i = 0; i < SIZE; i++) {
        if (brd[i] == 0) {
            return i;
        }
    }
    printf("board doesn't contain 0");
    exit(1);
}

puzzle* move_puzzle(puzzle* puz, int row_offset, int col_offset) {
    // find the location of the zero on the board
    int zero_loc = find_zero(puz->board);
    int zero_row = zero_loc / ROWS;
    int zero_col = zero_loc % ROWS;
    // find the location of the tile to be swapped
    int swap_row = zero_row + row_offset;
    int swap_col = zero_col + col_offset;
    int swap_loc = swap_col + ROWS * swap_row;
    // check if puzzle is out of bounds
    if (swap_row < 0 || swap_row >= ROWS || swap_col < 0 || swap_col >= ROWS) {
        return NULL;
    }
    // initialize the new puzzle state
    puzzle* new_puz = new_puzzle(puz->board);
    new_puz->prev = puz;
    new_puz->g = puz->g + 1;
    new_puz->f = new_puz->g + heuristic(new_puz->board);
    // swap location of 0 with new location
    tile temp = new_puz->board[zero_loc];
    new_puz->board[zero_loc] = new_puz->board[swap_loc];
    new_puz->board[swap_loc] = temp;
    return new_puz;
}
int heuristic(const board brd) {
    int h = 0;
    for (int i = 0; i < SIZE; i++) {
        // manhattan distance
        int row1 = i / ROWS;
        int col1 = i % ROWS;
        int row2 = brd[i] / SIZE;
        int col2 = brd[i] % SIZE;
        h += abs(row2 - row1) + abs(col2 - col1);
    }
    return h;
}

void solve(const board initial_brd, const board goal_brd) {
    int goal_hash = hash(goal_brd);

    puzzle* root = new_puzzle(initial_brd);
    priority_q* open_set = new_pq(1000);
    hash_table* closed_set = new_ht(1000, 0.7f);

    push_pq(open_set, root);

    // iterate until we find a solution
    while(open_set->size >= 0) {
        // pop off the state with the best heuristic
        puzzle* current_puz = pop_pq(open_set);
        int current_hash = hash(current_puz->board);
        ht_insert(closed_set, hash(current_puz->board));

        // check if we've reached the goal state
        if (current_hash == goal_hash) {
            // print out solution
            reconstruct_path(current_puz);
            return;
        }

        // add neighbor states to the priority queue
        for (int i = 0; i < NEIGHBOR_CNT; i++) {
            puzzle* neighbor_puz = move_puzzle(current_puz, NEIGHBOR_OFFSETS[i][0], NEIGHBOR_OFFSETS[i][1]);
            if (neighbor_puz != NULL && !ht_has(closed_set, hash(neighbor_puz->board))) {
                neighbor_puz->move = NEIGHBOR_MOVES[i];
                push_pq(open_set, neighbor_puz);
            }
        }
    }
}

void print_puzzle(puzzle* puz) {
    for (int i = 0; i < SIZE; i++) {
        if (puz->board[i] != 0) {
            printf("%d ", puz->board[i]);
        } else {
            printf("  ");
        }
        if ((i + 1) % 3 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}

void reconstruct_path(puzzle* leaf_puz) {
    int count;
    puzzle* path[LONGEST_SOL];
    for(count = 0; leaf_puz != NULL; count++) {
        path[count] = leaf_puz;
        leaf_puz = leaf_puz->prev;
    }
    printf("Solution: \n\n");
    for (int i = count - 1; i >= 0; i--) {
        printf("%s\n", MOVE_STRINGS[path[i]->move]);
        print_puzzle(path[i]);
    }
}

int main() {
    srand(time(0));

    char initial_brd[SIZE] = {0, 1, 3, 4, 2, 5, 7, 8, 6};
    char goal_brd[SIZE] = {1, 2, 3, 4, 5, 6, 7, 8, 0};

    solve(initial_brd, goal_brd);

    return 0;
}
