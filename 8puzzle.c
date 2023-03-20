//
// Joseph Prichard 2023
//

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <stdint.h>

#define SIZE 9
#define ROWS 3
#define tile char
#define NEIGHBOR_CNT 4
#define LONGEST_SOL 32
#define CHILD_CNT 4
#define LF_THRESHOLD 0.7f

// TYPE AND FUNCTION DEFINITIONS

typedef enum move {
    NONE, UP, DOWN, LEFT, RIGHT
} move;

typedef tile board[SIZE];

typedef struct puzzle {
    struct puzzle* parent;
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
} hash_table;

typedef struct list {
    puzzle** arr;
    int size;
    int capacity;
} list;

list* new_list();

void push_list(list* ls, puzzle*);

hash_table* new_ht();

int hash_board(const board);

void rehash(hash_table*);

int probe(hash_table*, int, int);

void insert_into_ht(hash_table* ht, int key);

void probe_ht(hash_table* ht, int key);

int ht_has_key(hash_table* ht, int key);

int is_prime(int);

int next_prime(int);

priority_q* new_pq();

void ensure_capacity(priority_q*);

void push_pq(priority_q*, puzzle*);

puzzle* pop_pq(priority_q*);

puzzle* new_puzzle(const board);

int find_zero(const board);

int move_board(board brd_in, board brd_out, int row_offset, int col_offset);

int heuristic(const board);

void solve(const board, const board);

void print_board(const board);

void reconstruct_path(puzzle*);

void parse_board(board brd, FILE* input_file);

// GLOBALS

static const int NEIGHBOR_OFFSETS[NEIGHBOR_CNT][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
static const int NEIGHBOR_MOVES[NEIGHBOR_CNT] = {RIGHT, DOWN, LEFT, UP};
static const char* MOVE_STRINGS[] = {"Start", "Up", "Down", "Left", "Right"};

// LIST IMPLEMENTATION

list* new_list() {
    list* ls = malloc(sizeof(list));
    ls->size = 0;
    ls->capacity = 10;
    ls->arr = malloc(sizeof(puzzle*) * ls->capacity);
    if (ls == NULL && ls->arr == NULL) {
        printf("Failed to allocate list");
        exit(1);
    }
    return ls;
}

void push_list(list* ls, puzzle* puz) {
    // add to block of all puzzles
    if (ls->size >= ls->capacity) {
        ls->capacity = ls->capacity * 2;
        ls->arr = realloc(ls->arr, sizeof(puzzle*) * ls->capacity);
        if (ls->arr == NULL) {
            printf("Failed to reallocate list");
            exit(1);
        }
    }
    ls->arr[ls->size] = puz;
    ls->size++;
}

// HASH TABLE IMPLEMENTATION

hash_table* new_ht() {
    hash_table* ht = malloc(sizeof(hash_table));
    ht->capacity = next_prime(10);
    ht->table = calloc(ht->capacity, sizeof(int));
    ht->size = 0;
    if (ht == NULL || ht->table == NULL) {
        printf("Failed to allocate hash_table");
        exit(1);
    }
    return ht;
}

int hash_board(const board board) {
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
            probe_ht(ht, old_table[i]);
        }
    }
    // free the old hash table
    free(old_table);
}

void insert_into_ht(hash_table* ht, int key) {
    // rehash when load factor exceeds threshold
    if ((float) ht->size / (float) ht->capacity > LF_THRESHOLD) {
        rehash(ht);
    }
    probe_ht(ht, key);
    ht->size++;
}

void probe_ht(hash_table* ht, int key) {
    // probe until we find a slot to insert
    for (int i = 0;; i++) {
        int p = probe(ht, key, i);
        if (ht->table[p] == 0) {
            ht->table[p] = key;
            break;
        }
    }
}

int ht_has_key(hash_table* ht, int key) {
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

// PQ IMPLEMENTATION

priority_q* new_pq() {
    priority_q* pq = malloc(sizeof(priority_q));
    pq->capacity = 10;
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
    int parent = (pos - 1) / CHILD_CNT;
    // sift up until parent score is larger
    while (parent >= 0) {
        if (pq->min_heap[pos]->f < pq->min_heap[parent]->f) {
            // swap parent with child
            puzzle* temp = pq->min_heap[pos];
            pq->min_heap[pos] = pq->min_heap[parent];
            pq->min_heap[parent] = temp;
            // climb up the tree
            pos = parent;
            parent = (pos - 1) / CHILD_CNT;
        } else {
            parent = -1;
        }
    }
    pq->size++;
}

puzzle* pop_pq(priority_q* pq) {
    // check for empty min_heap
    if (pq->size == 0) {
        printf("Can't pop an empty min_heap");
        exit(1);
    }
    // extract top element and move bottom to top
    puzzle* top = pq->min_heap[0];
    pq->min_heap[0] = pq->min_heap[pq->size - 1];
    // sift top element down
    int pos = 0;
    for (;;) {
        // get the smallest child
        int first_child = CHILD_CNT * pos + 1;
        int child = first_child;
        if (child >= pq->size) {
            break;
        }
        // iterate from leftmost to rightmost child to find the smallest at level
        for (int i = 1; i < CHILD_CNT; i++) {
            int new_child = first_child + i;
            if (new_child >= pq->size) {
                break;
            }
            if(pq->min_heap[new_child]->f < pq->min_heap[child]->f) {
                child = new_child;
            }
        }
        // swap child with parent if child is smaller
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
    puz->parent = NULL;
    puz->f = 0;
    puz->g = 0;
    return puz;
}

int find_zero(const board brd) {
    for (int i = 0; i < SIZE; i++)
        if (brd[i] == 0)
            return i;
    printf("board doesn't contain 0");
    exit(1);
}

int move_board(board brd_in, board brd_out, int row_offset, int col_offset) {
    // copy input to output (overrides output board)
    memcpy(brd_out, brd_in, sizeof(board));
    // find the location of the zero on the board
    int zero_loc = find_zero(brd_in);
    int zero_row = zero_loc / ROWS;
    int zero_col = zero_loc % ROWS;
    // find the location of the tile to be swapped
    int swap_row = zero_row + row_offset;
    int swap_col = zero_col + col_offset;
    int swap_loc = swap_col + ROWS * swap_row;
    // check if puzzle is out of bounds
    if (swap_row < 0 || swap_row >= ROWS || swap_col < 0 || swap_col >= ROWS) {
        return 1;
    }
    // swap location of 0 with new location
    tile temp = brd_out[zero_loc];
    brd_out[zero_loc] = brd_out[swap_loc];
    brd_out[swap_loc] = temp;
    return 0;
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
    int goal_hash = hash_board(goal_brd);

    list* puzzles = new_list();
    puzzle* root = new_puzzle(initial_brd);
    priority_q* open_set = new_pq();
    hash_table* closed_set = new_ht();

    push_list(puzzles, root);
    push_pq(open_set, root);

    // iterate until we find a solution
    while(open_set->size >= 0) {
        // pop off the state with the best heuristic
        puzzle* current_puz = pop_pq(open_set);
        int current_hash = hash_board(current_puz->board);
        insert_into_ht(closed_set, current_hash);

        // check if we've reached the goal state
        if (current_hash == goal_hash) {
            // print out solution
            reconstruct_path(current_puz);
            break;
        }

        // add neighbor states to the priority queue
        for (int i = 0; i < NEIGHBOR_CNT; i++) {
            board neighbor_board = {0};
            int row_offset = NEIGHBOR_OFFSETS[i][0];
            int col_offset = NEIGHBOR_OFFSETS[i][1];
            // write a moved board state into the neighbor board, then check for error states and if neighbor bord is closed
            if (move_board(current_puz->board, neighbor_board, row_offset, col_offset) == 0
                && !ht_has_key(closed_set, hash_board(neighbor_board))) {

                // create a new neighbor with the new board and calculated states
                puzzle* neighbor_puz = new_puzzle(neighbor_board);
                neighbor_puz->parent = current_puz;
                neighbor_puz->g = current_puz->g + 1;
                neighbor_puz->f = neighbor_puz->g + heuristic(neighbor_board);
                neighbor_puz->move = NEIGHBOR_MOVES[i];

                // add to list of all puzzles
                push_list(puzzles, neighbor_puz);

                // add neighbor board to pq
                push_pq(open_set, neighbor_puz);
            }
        }
    }

    // free all puzzles stored in list
    for (int i = 0; i < puzzles->size; i++) {
        free(puzzles->arr[i]);
    }

    free(puzzles->arr);
    free(puzzles);
    free(closed_set->table);
    free(closed_set);
    free(open_set->min_heap);
    free(open_set);
}

void print_board(const board brd) {
    for (int i = 0; i < SIZE; i++) {
        if (brd[i] != 0) {
            printf("%d ", brd[i]);
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
        leaf_puz = leaf_puz->parent;
    }
    for (int i = count - 1; i >= 0; i--) {
        printf("%s\n", MOVE_STRINGS[path[i]->move]);
        print_board(path[i]->board);
    }
    printf("Solved in %d steps\n", count - 1);
}

void parse_board(board brd, FILE* input_file) {
    int count = 0;
    for (;;) {
        char c = (char) fgetc(input_file);
        if (c == EOF)
            break;
        if(isdigit(c)) {
            int symbol = c - '0';
            brd[count] = (char) symbol;
            count++;
        }
    }
    if (count < 9) {
        printf("An input board's size must be 9.");
        exit(1);
    }
}

int main(int argc, char** argv) {
    if (argc <= 1) {
        printf("First argument must be input file.");
        return 1;
    }

    char* file_path = argv[1];
    FILE* input_file = fopen(file_path, "r");

    board initial_brd = {0};
    parse_board(initial_brd, input_file);

    board goal_brd = {1, 2, 3, 4, 5, 6, 7, 8, 0};

    printf("Starting...\n\n");

    clock_t tic = clock();

    solve(initial_brd, goal_brd);

    clock_t toc = clock() - tic;
    printf("Total execution time: %d ms", (int) toc);

    return 0;
}
