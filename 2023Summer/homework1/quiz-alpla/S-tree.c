/*
 * S-Tree: A self-balancing binary search tree.
 *
 * AVL-trees promise a close-to-optimal tree layout for lookup, but they
 * consume a significant amount of memory and require relatively slow
 * balancing operations. Red-black trees offer quicker manipulation with
 * a slightly less optimal tree layout, and the proposed S-Tree offers
 * fast insertion and deletion by balancing trees during lookup.
 *
 * S-trees rely on four fundamental Binary Search Tree (BST) operations:
 * rotate_left, rotate_right, replace_right, and replace_left. The latter
 * two, replace_right and replace_left, are exclusively employed during node
 * removal, following the conventional BST approach. They identify the
 * next/last node in the right/left subtree, respectively, and perform the
 * substitution of the node scheduled for deletion with the identified node.
 *
 * In contrast, rotate_left and rotate_right are integral to a dedicated update
 * phase aimed at rebalancing the tree. This update phase follows both insert
 * and remove phases in the current implementation. Nonetheless, it is
 * theoretically possible to have arbitrary sequences comprising insert,
 * remove, lookup, and update operations. Notably, the frequency of updates
 * directly influences the extent to which the tree layout approaches
 * optimality. However, it is important to consider that each update operation
 * incurs a certain time penalty.
 *
 * The update function exhibits a relatively straightforward process: When a
 * specific node leans to the right or left beyond a defined threshold, a left
 * or right rotation is performed on the node, respectively. Concurrently, the
 * node's hint is consistently updated. Additionally, if the node's hint becomes
 * zero or experiences a change compared to its previous state during the
 * update, modifications are made to the node's parent, as it existed before
 * these update operations.
 */

/* S-Tree uses hints to decide whether to perform a balancing operation or not.
 * Hints are similar to AVL-trees' height property, but they are not
 * required to be absolutely accurate. A hint provides an approximation
 * of the longest chain of nodes under the node to which the hint is attached.
 */
struct st_node {
    short hint;
    struct st_node *parent;
    struct st_node *left, *right;
};

// Is this a kind of aliasing?
struct st_root {
    struct st_node *root;
};

enum st_dir { LEFT, RIGHT };

#define st_root(r) (r->root)
#define st_left(n) (n->left)
#define st_right(n) (n->right)
#define st_rparent(n) (st_right(n)->parent)
#define st_lparent(n) (st_left(n)->parent)
#define st_parent(n) (n->parent)

/* Get the smallest one.
 * Return the left node of left subtree until it has no child, recursively.
 */
struct st_node *st_first(struct st_node *n)
{
    if (!st_left(n))
        return n;

    return st_first(st_left(n));
}

/* Get the larget one.
 * Return the righ node of right subtree until it has no child, recursively.
 */
struct st_node *st_last(struct st_node *n)
{
    if (!st_right(n))
        return n;

    return st_last(st_right(n));
}

static inline void st_rotate_left(struct st_node *n)
{
    struct st_node *l = st_left(n), *p = st_parent(n);

    st_parent(l) = st_parent(n);
    st_left(n) = st_right(l);
    st_parent(n) = l;
    st_right(l) = n;

    if (p && st_left(p) == n)
        st_left(p) = l;
    else if (p)
        st_right(p) = l;

    if (st_left(n))
        st_lparent(n) = n;
}

static inline void st_rotate_right(struct st_node *n)
{
    struct st_node *r = st_right(n), *p = st_parent(n);

    st_parent(r) = st_parent(n);
    st_right(n) = st_left(r);
    st_parent(n) = r;
    st_left(r) = n;

    if (p && st_left(p) == n)
        st_left(p) = r;
    else if (p)
        st_right(p) = r;

    if (st_right(n))
        st_rparent(n) = n;
}

static inline int st_balance(struct st_node *n)
{
    int l = 0, r = 0;

    if (st_left(n))
        l = st_left(n)->hint + 1;

    if (st_right(n))
        r = st_right(n)->hint + 1;

    return l - r;
}

static inline int st_max_hint(struct st_node *n)
{
    int l = 0, r = 0;

    if (st_left(n))
        l = st_left(n)->hint + 1;

    if (st_right(n))
        r = st_right(n)->hint + 1;

    return l > r ? l : r;
}

static inline void st_update(struct st_node **root, struct st_node *n)
{
    if (!n)
        return;

    int b = st_balance(n);
    int prev_hint = n->hint;
    struct st_node *p = st_parent(n);

    if (b < -1) {
        /* leaning to the right */
        if (n == *root)
            *root = st_right(n);
        st_rotate_right(n);
    }

    else if (b > 1) {
        /* leaning to the left */
        if (n == *root)
            *root = st_left(n);
        st_rotate_left(n);
    }

    n->hint = st_max_hint(n);
    if (n->hint == 0 || n->hint != prev_hint)
        st_update(root, p);
}

/* The process of insertion is straightforward and follows the standard approach
 * used in any BST. After inserting a new node into the tree using conventional
 * BST insertion techniques, an update operation is invoked on the newly
 * inserted node.
 */
void st_insert(struct st_node **root,
               struct st_node *p,
               struct st_node *n,
               enum st_dir d)
{
    if (d == LEFT)
        st_left(p) = n;
    else
        st_right(p) = n;

    st_parent(n) = p;
    st_update(root, n);
}

static inline void st_replace_right(struct st_node *n, struct st_node *r)
{
    struct st_node *p = st_parent(n), *rp = st_parent(r);

    if (st_left(rp) == r) {
        st_left(rp) = st_right(r);
        if (st_right(r))
            st_rparent(r) = rp;
    }

    if (st_parent(rp) == n)
        st_parent(rp) = r;

    st_parent(r) = p;
    st_left(r) = st_left(n);

    if (st_right(n) != r) {
        st_right(r) = st_right(n);
        st_rparent(n) = r;
    }

    if (p && st_left(p) == n)
        st_left(p) = r;
    else if (p)
        st_right(p) = r;

    if (st_left(n))
        st_lparent(n) = r;
}

static inline void st_replace_left(struct st_node *n, struct st_node *l)
{
    struct st_node *p = st_parent(n), *lp = st_parent(l);

    if (st_right(lp) == l) {
        st_right(lp) = st_left(l);
        if (st_left(l))
            st_lparent(l) = lp;
    }

    if (st_parent(lp) == n)
        st_parent(lp) = l;

    st_parent(l) = p;
    st_right(l) = st_right(n);

    if (st_left(n) != l) {
        st_left(l) = st_left(n);
        st_lparent(n) = l;
    }

    if (p && st_left(p) == n)
        st_left(p) = l;
    else if (p)
        st_right(p) = l;

    if (st_right(n))
        st_rparent(n) = l;
}

/* The process of deletion in this tree structure is relatively more intricate,
 * although it shares similarities with deletion methods employed in other BST.
 * When removing a node, if the node to be deleted has a right child, the
 * deletion process entails replacing the node to be removed with the first node
 * encountered in the right subtree. Following this replacement, an update
 * operation is invoked on the right child of the newly inserted node.
 *
 * Similarly, if the node to be deleted does not have a right child, the
 * replacement process involves utilizing the first node found in the left
 * subtree. Subsequently, an update operation is called on the left child of th
 * replacement node.
 *
 * In scenarios where the node to be deleted has no children (neither left nor
 * right), it can be directly removed from the tree, and an update operation is
 * invoked on the parent node of the deleted node.
 */
void st_remove(struct st_node **root, struct st_node *del)
{
    if (st_right(del)) {
        struct st_node *least = st_first(st_right(del));
        if (del == *root)
            *root = least;

        st_replace_right(del, st_right(del));  // AAAA
        st_update(root, st_right(del));        // BBBB
        return;
    }

    if (st_left(del)) {
        struct st_node *most = st_last(st_left(del));
        if (del == *root)
            *root = most;

        st_replace_left(del, st_left(del));  // CCCC
        st_update(root, st_left(del));       // DDDD
        return;
    }

    if (del == *root) {
        *root = 0;
        return;
    }

    /* empty node */
    struct st_node *parent = st_parent(del);

    if (st_left(parent) == del)
        st_left(parent) = 0;
    else
        st_right(parent) = 0;

    st_update(root, parent);  // EEEE
}

/* Test program */

#include <assert.h>
#include <stddef.h> /* offsetof */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define container_of(ptr, type, member) \
    ((type *) ((char *) (ptr) - (offsetof(type, member))))

#define treeint_entry(ptr) container_of(ptr, struct treeint, st_n)

struct treeint {
    int value;
    struct st_node st_n;
};

static struct st_root *tree;

int treeint_init()
{
    tree = calloc(sizeof(struct st_root), 1);
    assert(tree);
    return 0;
}

static void __treeint_destroy(struct st_node *n)
{
    if (st_left(n))
        __treeint_destroy(st_left(n));

    if (st_right(n))
        __treeint_destroy(st_right(n));

    struct treeint *i = treeint_entry(n);
    free(i);
}

int treeint_destroy()
{
    assert(tree);
    if (st_root(tree))
        __treeint_destroy(st_root(tree));

    free(tree);
    return 0;
}

struct treeint *treeint_insert(int a)
{
    struct st_node *p = NULL;
    enum st_dir d;
    for (struct st_node *n = st_root(tree); n;) {
        struct treeint *t = container_of(n, struct treeint, st_n);
        if (a == t->value)
            return t;

        p = n;

        if (a < t->value) {
            n = st_left(n);
            d = LEFT;
        } else if (a > t->value) {
            n = st_right(n);
            d = RIGHT;
        }
    }

    struct treeint *i = calloc(sizeof(struct treeint), 1);
    if (st_root(tree))
        st_insert(&st_root(tree), p, &i->st_n, d);
    else
        st_root(tree) = &i->st_n;

    i->value = a;
    return i;
}

struct treeint *treeint_find(int a)
{
    struct st_node *n = st_root(tree);
    while (n) {
        struct treeint *t = treeint_entry(n);
        if (a == t->value)
            return t;

        if (a < t->value)
            n = st_left(n);
        else if (a > t->value)
            n = st_right(n);
    }

    return 0;
}

int treeint_remove(int a)
{
    struct treeint *n = treeint_find(a);
    if (!n)
        return -1;

    st_remove(&st_root(tree), &n->st_n);
    free(n);
    return 0;
}

/* ascending order (What's depth for? Unused.....) */
static void __treeint_dump(struct st_node *n, int depth)
{
    if (!n)
        return;

    __treeint_dump(st_left(n), depth + 1);  // FFFF

    struct treeint *v = treeint_entry(n);
    printf("%d\n", v->value);

    __treeint_dump(st_right(n), depth + 1);  // GGGG
}

void treeint_dump()
{
    __treeint_dump(st_root(tree), 0);
}

int main()
{
    srand(time(0));

    treeint_init();

    for (int i = 0; i < 100; ++i)
        treeint_insert(rand() % 99);

    printf("[ After insertions ]\n");
    treeint_dump();

    printf("Removing...\n");
    for (int i = 0; i < 100; ++i) {
        int v = rand() % 99;
        printf("%2d  ", v);
        if ((i + 1) % 10 == 0)
            printf("\n");
        treeint_remove(v);
    }
    printf("\n");

    printf("[ After removals ]\n");
    treeint_dump();

    treeint_destroy();

    return 0;
}
