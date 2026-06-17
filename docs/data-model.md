# Toy Data Model Plan

Toy's next data-structure work is not just "add a map". The larger goal is to
make the language's data model explicit enough that many structures can share
the same runtime values while exposing clear language-level semantics.

The core observation is that Toy values are boxed `tf_obj` allocations. Current
containers store `tf_obj *` references, so a collection can rearrange references
without copying the full values. This is a powerful model, but it needs precise
rules for ownership, equality, conversion, and word contracts.

## Current Model

Runtime values are represented by `tf_obj`:

- scalars: bool, int, float, string, symbol
- lists: ordered arrays of `tf_obj *`
- capture forms: internal variable binding and fetch objects

Today `TF_OBJ_TYPE_LIST` has several roles:

- an ordered data collection
- an executable quotation
- the parsed program representation
- the default sequence consumed by collection words

That overlap is useful and very Joy-like, but it should be treated as an
explicit language design choice rather than an implementation accident.

Objects are stable heap allocations today. Passing a `tf_obj *` is cheap. The
array that stores a list's element pointers can move during growth or shrink,
but the element objects themselves do not move.

This means code may keep `tf_obj *value` references, but must not keep long-lived
references to list slots such as `&list->elem[i]`.

## Design Layers

Keep these concepts separate:

| Layer | Meaning | Examples |
| --- | --- | --- |
| Representation type | What kind of value this object is | int, string, symbol, vector, map, set |
| Capability | What operations a word may require | callable, sequence, indexed, associative |
| Physical layout | How the runtime stores and searches it | array, hash table, tree, heap, deque |

Public words should generally be documented in terms of capabilities, not
physical layouts. Physical layout is an implementation strategy unless it
changes observable behavior.

## Source Syntax

Toy has dedicated syntax for the core collection types whose semantics are
expected to be common and stable.

The ordered collection and quotation literal is:

```toy
[ 1 2 3 ]
```

This value is both:

- an ordered sequence value
- a quotation when a word consumes a callable

Map and set literals are dedicated semantic forms:

```toy
{ 'name "Alice" 'age 30 }
#{ 1 2 3 }
```

Maps are associative key/value collections. Sets are membership collections
with unique items. Both preserve deterministic insertion-order projections for
printing, tests, and interop, but they are not ordered sequences.

Explicit constructors remain part of the permanent model. They are used for
runtime conversion from compatible data and for secondary structures that do
not need literal syntax:

```toy
[ 1 2 3 2 ] >set
[ [ 'name "Alice" ] [ 'age 30 ] ] >map
[ 5 1 9 2 ] >heap
[ "a" "b" "c" ] >queue
```

Captures use `| name ... |`.

Secondary literal syntax can be added only when the structure has become common
enough to justify permanent surface area. Constructors are not a temporary
fallback; they make conversion, invariants, and policy visible.

```toy
priority[ [ 10 "low" ] [ 1 "urgent" ] ]
```

Do not use one collection literal whose backend changes invisibly by context.
If a value becomes a map, set, queue, or heap, that should be visible either in
the literal syntax or in an explicit constructor/conversion word.

## Default Collection

Do not require every collection literal to specify a structure. It would make
Toy noisy and would weaken quotations.

The default `[ ... ]` value should remain array-backed because it is good for:

- parsing programs
- executing quotations by program counter
- `len`, indexed access, slicing, mapping, folding
- cache locality
- cheap appends in the common case
- serving as interchange input for constructors

Long term, "list" may be too vague. The implementation is closer to a vector or
array-backed quotation. The stable language rule is that `[ ... ]` is the
ordered indexed collection and quotation form. Rename public words only if it
makes the language clearer; avoid churn for its own sake.

## Explicit vs Transparent Conversion

Conversions that change observable semantics must be explicit.

Observable changes include:

- type
- order
- duplicate behavior
- lookup behavior
- stack effect
- error behavior

Examples:

```toy
[ 1 2 2 3 ] >set       \ duplicates intentionally collapse
m keys                 \ choose key projection
m values               \ choose value projection
m pairs                \ choose pair projection
pairs >map             \ build associative structure
heap drain             \ choose priority order output
```

Internal layout optimizations may be transparent when the value's observable
semantics do not change:

- vector capacity growth
- hash table resize
- cached hash computation
- small-vector inline storage
- copy-on-write when a value is uniquely owned
- persistent structural sharing

Rule:

> If the user can observe a different language value, require an explicit word.
> If only performance changes, the runtime may choose automatically.

## Conversion Categories

### Shape-Preserving

Shape-preserving conversions retain order and multiplicity.

Possible examples:

```toy
seq >vec
seq >deque
seq >linked
```

These are mostly physical-layout choices. They should not be public until there
is a clear reason to expose the layout.

### Projection

Projection extracts a specific view from a richer structure:

```toy
m keys
m values
m pairs
s items
```

Maps should not silently become a list, because there are several valid list
views. Require `keys`, `values`, or `pairs`. Sets expose an explicit `items`
projection.

### Invariant-Building

Invariant-building conversions construct a structure with new constraints:

```toy
seq >set       \ uniqueness by equality/hash
pairs >map     \ unique keys
seq >heap      \ priority invariant
seq >queue     \ front/back discipline
```

These conversions may validate input, reorder internal storage, collapse
duplicates, or report errors.

### Lossy or Ambiguous

Lossy conversions need explicit policy words:

```toy
pairs >map       \ duplicate keys are an error
pairs >map-last  \ last duplicate wins
pairs >map-first \ first duplicate wins
```

Start with the strict form. Add policy variants only when scripts need them.

## First Collection Capabilities

Define capabilities before adding many concrete structures.

### Callable

Executable deferred code.

Current values:

- quoted symbols naming words
- quotations/lists

Words:

```toy
exec
i
dip
keep
map
filter
linrec
```

### Sequence

Finite ordered iteration.

Current values:

- list/vector
- string as byte sequence

Possible future values:

- deque
- lazy sequence, only if evaluation rules are settled

Words:

```toy
len
first
rest
uncons
each
map
fold
filter
split
```

### Indexed

Random access by integer index.

Current values:

- list/vector
- string

Possible words:

```toy
geth
seth
slice
take
dropn
```

Do not assume every sequence is indexed. A linked list, stream, tree traversal,
or generator may be a sequence without cheap random access.

### Associative

Lookup by key.

Future values:

- map

Words:

```toy
has?
get
assoc
dissoc
keys
values
pairs
```

Do not overload `geth` for maps. It remains the indexed lookup word for
ordered/indexed values. Associative lookup uses plain `get`.

### Membership

Containment by equality/hash.

Future values:

- set
- map keys

Words:

```toy
has?
adjoin
remove
items
```

Mapping over a set should not silently return a set unless duplicate-collapse is
the intended result. Prefer explicit composition:

```toy
myset items [ square ] map >set
```

### Priority

Access by minimum or maximum priority.

Future values:

- heap
- priority queue

Words:

```toy
push
peek
pop
drain
```

Heap-to-list needs an explicit order word. Raw heap storage order is not a
semantic sequence order.

## Equality and Hashing

Maps and sets force a real value-equality and hashing policy.

Required decisions:

- Integers hash and compare by value.
- Booleans hash and compare by value.
- Strings hash and compare by byte content.
- Symbols hash and compare by name.
- Quotedness should not create a separate runtime identity unless the language
  explicitly wants quoted and unquoted symbols to be different data values.
- Lists can be structurally equal, but using them as hash keys requires stable
  immutability or cached hash invalidation.
- Floats need a policy for `nan`, `-0`, and `0`.
- Maps and sets as keys should wait until structural hashing is mature.

Initial key policy should be conservative:

```text
hashable: bool, int, string, symbol
defer: float, list, map, set
```

Floats can be added after deciding `nan` and signed-zero behavior.

Structural keys can be added after update semantics are settled.

## Mutability Model

Toy should keep update-style data words:

```toy
coll key value seth
map key value assoc
map key dissoc
```

These words return updated values rather than mutating shared objects as a
language guarantee.

The implementation may optimize:

- mutate in place when the object is uniquely owned
- copy on write when shared
- use persistent structural sharing

But user code should not observe aliasing surprises.

This rule makes maps, sets, and future structural hashing much easier to reason
about.

## Ownership Rules

Every collection owns references to its contents.

When storing a `tf_obj *` in a collection:

```c
tf_obj_retain(value);
```

When removing or freeing:

```c
tf_obj_release(value);
```

Constructors that move references from a temporary collection must be explicit
about ownership transfer. Prefer simple retain/release first; optimize transfer
paths only after tests are strong.

## Architecture Direction

The first implementation can use explicit type switches. Do not invent a large
generic object system before there are enough concrete structures to justify it.

Target helper APIs:

```c
bool tf_obj_equal(tf_obj *a, tf_obj *b);
bool tf_obj_hashable(tf_obj *o);
uint64_t tf_obj_hash(tf_obj *o);

bool tf_obj_is_sequence(tf_obj *o);
bool tf_obj_is_indexed(tf_obj *o);
bool tf_obj_is_assoc(tf_obj *o);
```

Once switches start spreading, introduce operation tables or capability helpers:

```c
typedef struct {
    bool (*len)(tf_obj *o, size_t *out);
    bool (*iter_next)(...);
    bool (*get_index)(...);
    bool (*assoc_get)(...);
} tf_collection_ops;
```

Avoid exposing physical layouts too early. Public predicates should start with
semantic representation types and capabilities:

```toy
list?
map?
set?
sequence?
indexed?
assoc?
```

Only add `vector?`, `deque?`, or `hash-map?` if the language exposes layout as
an observable performance choice.

## Word Contract Rules

Existing and future native words should answer:

1. What capability does this word consume?
2. Is the result type obvious?
3. Does the word preserve order?
4. Does the word preserve duplicates?
5. Does the word require a key policy?
6. Is absence an error or checked by a predicate?

Examples:

| Word | Capability | Result Rule |
| --- | --- | --- |
| `len` | finite collection | integer length |
| `map` | ordered sequence + callable | same sequence family when unambiguous |
| `filter` | ordered sequence + predicate | same sequence family when unambiguous |
| `fold` | sequence + callable | accumulator |
| `keys` | associative | ordered list if map has defined iteration order |
| `pairs` | associative | list of two-item lists |
| `has?` | associative or membership | boolean |
| `get` | associative | value or runtime error |
| `items` | membership | list of elements |

Do not overload a word merely because implementation can do it. Overload only
when the language concept is the same.

## Map Semantics, First Pass

Maps are the first serious test of this model.

Initial constructor:

```toy
[ [ 'name "Alice" ] [ 'age 30 ] ] >map
```

Rules:

- input is a sequence of two-item lists
- keys must be hashable under the initial key policy
- duplicate keys are an error
- map iteration order is insertion order unless a stronger reason appears
- `get` on missing key is an error
- `has?` checks expected absence
- `assoc` returns an updated map
- `dissoc` returns an updated map
- `keys`, `values`, and `pairs` return lists

Insertion order costs memory but makes examples, printing, tests, and JSON
interop much clearer. If performance later demands unordered maps, expose that
as a separate decision rather than making ordinary maps nondeterministic.

## Set Semantics, Later

Sets can reuse map hashing and equality.

Initial constructor:

```toy
[ 1 2 2 3 ] >set
```

Rules:

- duplicates collapse intentionally
- `items` returns a deterministic list, probably insertion order
- `has?` checks membership
- mapping a set is explicit through `items`

## Performance Lab Hooks

The data model should make performance experiments meaningful:

- array list vs linked list vs chunked vector
- hash map probing strategies
- insertion-ordered hash map overhead
- cached hash values
- copy-on-write vs persistent structures
- small-vector optimization for short quotations
- allocation pressure and refcount traffic

Record performance work as experiments with benchmarks, not as assumptions.

## Implementation Milestones

1. Write this data-model plan and link it from the roadmap.
2. Extract shared equality into a public internal helper.
3. Add hashability and hashing for conservative scalar keys.
4. Add `TF_OBJ_TYPE_MAP` and `TF_OBJ_TYPE_SET` objects with insertion-ordered
   hash table storage.
5. Add map/set literals and the core words `>map`, `>set`, `map?`, `set?`,
   `has?`, `get`, `assoc`, `dissoc`, `keys`, `values`, `pairs`, `items`,
   `adjoin`, and `remove`.
6. Add focused tests for ownership, missing keys, duplicate keys, source
   printing, equality, and stack effects.
7. Add tooling metadata for the new native words.
8. Revisit sets after map hashing is stable.
9. Revisit syntax only after constructors have enough real usage.

## Open Questions

- Are lists eventually renamed to vectors in public docs?
- Are quotations a distinct public type or still list values with callable
  capability?
- When do floats become hashable?
- Should quotedness be removed from symbol runtime identity?
- Should persistent data structures be a language guarantee or an optimization?
