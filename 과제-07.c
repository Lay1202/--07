#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define V          100
#define SPARSE_E   100
#define DENSE_E    4000
#define OP_COUNT   10
#define DEFAULT_SEED 2024

typedef struct {
    int u, v;
} Edge;

typedef struct {
    Edge ins[OP_COUNT];
    Edge del[OP_COUNT];
    Edge conn;
    int  adj_v;
} TestData;

typedef struct {
    size_t memory;
    long   ins_cmp;
    long   del_cmp;
    long   conn_cmp;
    int    conn_found;
    long   adj_cmp;
    int    adj_deg;
    int    adj_list[V];
    double avg_conn;
    double avg_adj;
} Result;

typedef struct {
    int n;
    int edge_count;
    int matrix[V][V];
} AdjMatrix;

static AdjMatrix* matrix_create(void)
{
    AdjMatrix* g = (AdjMatrix*)calloc(1, sizeof(AdjMatrix));
    if (!g) { perror("calloc"); exit(1); }
    g->n = V;
    return g;
}

static void matrix_build_edge(AdjMatrix* g, int u, int v)
{
    g->matrix[u][v] = 1;
    g->matrix[v][u] = 1;
    g->edge_count++;
}

static int matrix_has_edge(const AdjMatrix* g, int u, int v, long* cmp)
{
    (*cmp)++;
    return g->matrix[u][v] == 1;
}

static int matrix_insert(AdjMatrix* g, int u, int v, long* cmp)
{
    if (matrix_has_edge(g, u, v, cmp)) return 0;
    g->matrix[u][v] = 1;
    g->matrix[v][u] = 1;
    g->edge_count++;
    return 1;
}

static int matrix_delete(AdjMatrix* g, int u, int v, long* cmp)
{
    if (!matrix_has_edge(g, u, v, cmp)) return 0;
    g->matrix[u][v] = 0;
    g->matrix[v][u] = 0;
    g->edge_count--;
    return 1;
}

static long matrix_neighbors(const AdjMatrix* g, int u, int* out, int* cnt)
{
    long cmp = 0;
    *cnt = 0;
    for (int j = 0; j < V; j++) {
        cmp++;
        if (g->matrix[u][j] == 1) out[(*cnt)++] = j;
    }
    return cmp;
}

static size_t matrix_memory(const AdjMatrix* g)
{
    (void)g;
    return sizeof(AdjMatrix);
}

typedef struct Node {
    int vertex;
    struct Node* next;
} Node;

typedef struct {
    int n;
    int edge_count;
    size_t node_count;
    Node* head[V];
} AdjList;

static AdjList* list_create(void)
{
    AdjList* g = (AdjList*)calloc(1, sizeof(AdjList));
    if (!g) { perror("calloc"); exit(1); }
    g->n = V;
    return g;
}

static void list_push_front(AdjList* g, int u, int v)
{
    Node* p = (Node*)malloc(sizeof(Node));
    if (!p) { perror("malloc"); exit(1); }
    p->vertex = v;
    p->next = g->head[u];
    g->head[u] = p;
    g->node_count++;
}

static void list_build_edge(AdjList* g, int u, int v)
{
    list_push_front(g, u, v);
    list_push_front(g, v, u);
    g->edge_count++;
}

static int list_has_edge(const AdjList* g, int u, int v, long* cmp)
{
    for (const Node* p = g->head[u]; p; p = p->next) {
        (*cmp)++;
        if (p->vertex == v) return 1;
    }
    return 0;
}

static int list_insert(AdjList* g, int u, int v, long* cmp)
{
    if (list_has_edge(g, u, v, cmp)) return 0;
    list_push_front(g, u, v);
    list_push_front(g, v, u);
    g->edge_count++;
    return 1;
}

static int list_remove_one(AdjList* g, int from, int target, long* cmp)
{
    Node* prev = NULL;
    for (Node* p = g->head[from]; p; prev = p, p = p->next) {
        (*cmp)++;
        if (p->vertex == target) {
            if (prev) prev->next = p->next;
            else      g->head[from] = p->next;
            free(p);
            g->node_count--;
            return 1;
        }
    }
    return 0;
}

static int list_delete(AdjList* g, int u, int v, long* cmp)
{
    if (!list_remove_one(g, u, v, cmp)) return 0;
    list_remove_one(g, v, u, cmp);
    g->edge_count--;
    return 1;
}

static long list_neighbors(const AdjList* g, int u, int* out, int* cnt)
{
    long cmp = 0;
    *cnt = 0;
    for (const Node* p = g->head[u]; p; p = p->next) {
        cmp++;
        out[(*cnt)++] = p->vertex;
    }
    return cmp;
}

static size_t list_memory(const AdjList* g)
{
    return sizeof(AdjList) + g->node_count * sizeof(Node);
}

static void list_free(AdjList* g)
{
    for (int i = 0; i < V; i++) {
        Node* p = g->head[i];
        while (p) {
            Node* next = p->next;
            free(p);
            p = next;
        }
    }
    free(g);
}

static void generate_edges(Edge* edges, int m)
{
    static char used[V][V];
    memset(used, 0, sizeof(used));

    int cnt = 0;
    while (cnt < m) {
        int u = rand() % V;
        int v = rand() % V;
        if (u == v) continue;
        if (used[u][v]) continue;
        used[u][v] = used[v][u] = 1;
        edges[cnt].u = u;
        edges[cnt].v = v;
        cnt++;
    }
}

static void make_test_data(TestData* t, const Edge* edges, int m)
{
    static char exists[V][V];
    memset(exists, 0, sizeof(exists));
    for (int i = 0; i < m; i++) {
        exists[edges[i].u][edges[i].v] = 1;
        exists[edges[i].v][edges[i].u] = 1;
    }

    for (int k = 0; k < OP_COUNT; ) {
        int u = rand() % V;
        int v = rand() % V;
        if (u == v || exists[u][v]) continue;
        exists[u][v] = exists[v][u] = 1;
        t->ins[k].u = u;
        t->ins[k].v = v;
        k++;
    }

    int* idx = (int*)malloc(sizeof(int) * m);
    if (!idx) { perror("malloc"); exit(1); }
    for (int i = 0; i < m; i++) idx[i] = i;
    for (int k = 0; k < OP_COUNT; k++) {
        int r = k + rand() % (m - k);
        int tmp = idx[k]; idx[k] = idx[r]; idx[r] = tmp;
        t->del[k] = edges[idx[k]];
    }
    free(idx);

    do {
        t->conn.u = rand() % V;
        t->conn.v = rand() % V;
    } while (t->conn.u == t->conn.v);
    t->adj_v = rand() % V;
}

static Result run_matrix(const Edge* edges, int m, const TestData* t, AdjMatrix** out_g)
{
    Result r;
    memset(&r, 0, sizeof(r));
    AdjMatrix* g = matrix_create();
    for (int i = 0; i < m; i++) matrix_build_edge(g, edges[i].u, edges[i].v);

    r.memory = matrix_memory(g);

    r.conn_found = matrix_has_edge(g, t->conn.u, t->conn.v, &r.conn_cmp);

    r.adj_cmp = matrix_neighbors(g, t->adj_v, r.adj_list, &r.adj_deg);

    long total = 0, pairs = 0;
    for (int u = 0; u < V; u++)
        for (int v = u + 1; v < V; v++) { matrix_has_edge(g, u, v, &total); pairs++; }
    r.avg_conn = (double)total / pairs;
    total = 0;
    for (int u = 0; u < V; u++) {
        int tmp[V], c;
        total += matrix_neighbors(g, u, tmp, &c);
    }
    r.avg_adj = (double)total / V;

    for (int k = 0; k < OP_COUNT; k++)
        if (!matrix_insert(g, t->ins[k].u, t->ins[k].v, &r.ins_cmp)) {
            fprintf(stderr, "internal error: matrix insert failed\n"); exit(1);
        }
    for (int k = 0; k < OP_COUNT; k++)
        if (!matrix_delete(g, t->del[k].u, t->del[k].v, &r.del_cmp)) {
            fprintf(stderr, "internal error: matrix delete failed\n"); exit(1);
        }

    *out_g = g;
    return r;
}

static Result run_list(const Edge* edges, int m, const TestData* t, AdjList** out_g)
{
    Result r;
    memset(&r, 0, sizeof(r));
    AdjList* g = list_create();
    for (int i = 0; i < m; i++) list_build_edge(g, edges[i].u, edges[i].v);

    r.memory = list_memory(g);

    r.conn_found = list_has_edge(g, t->conn.u, t->conn.v, &r.conn_cmp);

    r.adj_cmp = list_neighbors(g, t->adj_v, r.adj_list, &r.adj_deg);

    long total = 0, pairs = 0;
    for (int u = 0; u < V; u++)
        for (int v = u + 1; v < V; v++) { list_has_edge(g, u, v, &total); pairs++; }
    r.avg_conn = (double)total / pairs;
    total = 0;
    for (int u = 0; u < V; u++) {
        int tmp[V], c;
        total += list_neighbors(g, u, tmp, &c);
    }
    r.avg_adj = (double)total / V;

    for (int k = 0; k < OP_COUNT; k++)
        if (!list_insert(g, t->ins[k].u, t->ins[k].v, &r.ins_cmp)) {
            fprintf(stderr, "internal error: list insert failed\n"); exit(1);
        }
    for (int k = 0; k < OP_COUNT; k++)
        if (!list_delete(g, t->del[k].u, t->del[k].v, &r.del_cmp)) {
            fprintf(stderr, "internal error: list delete failed\n"); exit(1);
        }

    *out_g = g;
    return r;
}

static int same_graph(const AdjMatrix* a, const AdjList* b)
{
    long dummy = 0;
    if (a->edge_count != b->edge_count) return 0;
    if ((size_t)(2 * a->edge_count) != b->node_count) return 0;
    for (int u = 0; u < V; u++)
        for (int v = 0; v < V; v++) {
            int x = a->matrix[u][v] == 1;
            int y = list_has_edge(b, u, v, &dummy);
            if (x != y) return 0;
        }
    return 1;
}

static void print_case(int no, const char* graph_name, const char* repr,
    const Result* r, const TestData* t)
{
    printf("Case %d : %s Graph - %s\n", no, graph_name, repr);
    printf("%-30s : %zu Bytes\n", "Memory", r->memory);
    printf("%-30s : %ld\n", "Edge insertion comparisons", r->ins_cmp);
    printf("%-30s : %ld\n", "Edge deletion comparisons", r->del_cmp);
    printf("%-30s : %ld\n", "Connectivity check comparisons", r->conn_cmp);
    printf("%-30s : %ld\n", "Adjacent vertices comparisons", r->adj_cmp);

    printf("  - Connectivity (%d, %d) : %s\n", t->conn.u, t->conn.v,
        r->conn_found ? "connected" : "not connected");
    printf("  - Adjacent vertices of %d (%d): ", t->adj_v, r->adj_deg);

    int sorted[V];
    memcpy(sorted, r->adj_list, sizeof(int) * r->adj_deg);
    for (int i = 1; i < r->adj_deg; i++) {
        int key = sorted[i], j = i - 1;
        while (j >= 0 && sorted[j] > key) { sorted[j + 1] = sorted[j]; j--; }
        sorted[j + 1] = key;
    }
    for (int i = 0; i < r->adj_deg; i++) printf("%d ", sorted[i]);
    printf("\n\n");
}

static void print_supplement(const char* graph_name, const Result* m, const Result* l)
{
    printf("  [%s] 참고 평균값\n", graph_name);
    printf("    %-34s Matrix %8.2f | List %8.2f\n", "삽입 1회 평균 비교 횟수",
        (double)m->ins_cmp / OP_COUNT, (double)l->ins_cmp / OP_COUNT);
    printf("    %-34s Matrix %8.2f | List %8.2f\n", "삭제 1회 평균 비교 횟수",
        (double)m->del_cmp / OP_COUNT, (double)l->del_cmp / OP_COUNT);
    printf("    %-34s Matrix %8.2f | List %8.2f\n", "연결 확인 평균 (모든 정점 쌍)",
        m->avg_conn, l->avg_conn);
    printf("    %-34s Matrix %8.2f | List %8.2f\n", "인접 정점 탐색 평균 (모든 정점)",
        m->avg_adj, l->avg_adj);
}

int main(int argc, char* argv[])
{
    unsigned seed = DEFAULT_SEED;
    if (argc > 1) seed = (unsigned)strtoul(argv[1], NULL, 10);
    srand(seed);

    static Edge sparse_edges[SPARSE_E];
    static Edge dense_edges[DENSE_E];
    TestData sparse_t, dense_t;

    generate_edges(sparse_edges, SPARSE_E);
    generate_edges(dense_edges, DENSE_E);
    make_test_data(&sparse_t, sparse_edges, SPARSE_E);
    make_test_data(&dense_t, dense_edges, DENSE_E);

    AdjMatrix* sm, * dm;
    AdjList* sl, * dl;
    Result r_sm = run_matrix(sparse_edges, SPARSE_E, &sparse_t, &sm);
    Result r_sl = run_list(sparse_edges, SPARSE_E, &sparse_t, &sl);
    Result r_dm = run_matrix(dense_edges, DENSE_E, &dense_t, &dm);
    Result r_dl = run_list(dense_edges, DENSE_E, &dense_t, &dl);

    printf("=== Graph: %d vertices, undirected simple graph (seed = %u) ===\n", V, seed);
    printf("=== Edge insertion/deletion: %d edges each (comparisons are summed) ===\n\n",
        OP_COUNT);

    print_case(1, "Sparse", "Adjacency Matrix", &r_sm, &sparse_t);
    print_case(2, "Sparse", "Adjacency List", &r_sl, &sparse_t);
    print_case(3, "Dense", "Adjacency Matrix", &r_dm, &dense_t);
    print_case(4, "Dense", "Adjacency List", &r_dl, &dense_t);

    printf("--- Verification ---\n");
    printf("Sparse graph: matrix and list are %s\n",
        same_graph(sm, sl) ? "IDENTICAL" : "DIFFERENT (error)");
    printf("Dense  graph: matrix and list are %s\n\n",
        same_graph(dm, dl) ? "IDENTICAL" : "DIFFERENT (error)");

    printf("--- Supplementary averages ---\n");
    print_supplement("Sparse", &r_sm, &r_sl);
    print_supplement("Dense", &r_dm, &r_dl);

    free(sm);
    free(dm);
    list_free(sl);
    list_free(dl);
    return 0;
}
