# Toy Data Model

Toy's collection model is built around explicit representation types plus
capability-oriented words. A word should say what it needs: callable, sequence,
indexed, associative, membership, or priority access.

## Core Values

Runtime values are boxed `tf_obj` allocations. Collections store retained
`tf_obj *` references, so collections can rearrange references without copying
the values themselves.

Current public representation types:

- scalars: bool, int, float, string, symbol
- primary collections: vector, list, map, set
- secondary collections: deque, priority queue
- internal capture forms: varlist and varfetch

## Syntax

The primary collection syntaxes are permanent language surface:

```toy
[ 1 2 3 ]                 \ vector
( 1 2 3 )                 \ list
{ 'name "Ada" 'age 36 }   \ map
#{ 1 2 3 }                \ set
```

`[ ... ]` creates a vector. Vectors are ordered, indexed, array-backed values.
They also serve as compound quotations when a word consumes a callable.

`( ... )` creates a linked list. Lists are ordered sequence data. They are not
callable and are not indexed by `at`, `set-at`, or `slice`.

`{ ... }` creates an associative map from alternating key/value forms.
`#{ ... }` creates a membership set with duplicate items rejected in literals.

Strings are byte sequences rather than Unicode text. A language-level
character is exactly a one-byte string. String literals accept `\n`, `\r`,
`\t`, `\"`, `\\`, and `\xHH`; other backslash escapes are errors. `\xHH`
uses exactly two hexadecimal digits and can construct any byte.

Secondary structures are built explicitly:

```toy
[ 1 2 3 ] >deque
( 1 2 3 ) >deque
[ 1 2 3 ] >list
( 1 2 3 ) >vector
[ [ 10 "low" ] [ 1 "urgent" ] ] >pqueue
[ [ 'name "Ada" ] [ 'age 36 ] ] >map
```

Constructor words are not temporary syntax. They are how Toy names conversions,
validation, and invariants for structures that do not need literal syntax.

## Capabilities

### Callable

Callable values are deferred code:

- quoted symbols naming words
- vector quotations

Lists are intentionally excluded even though they are sequences.

Words: `exec`, `i`, `dip`, `keep`, `bi`, `if`, `while`, `map`, `filter`,
`linrec`, `binrec`, `genrec`.

### Sequence

Sequences are finite ordered iteration:

- vector
- list
- string as a byte sequence

Generic sequence words preserve the input family when that is clear. Mapping a
list returns a list, mapping a vector returns a vector, and mapping a string
returns a string when each result is a one-byte string.

Words: `len`, `first`, `last`, `rest`, `uncons`, `cons`, `push-back`, `concat`,
`reverse`, `take`, `dropn`, `contains?`, `indexof`, `unique`, `sort`, `each`,
`map`, `fold`, `filter`, `split`, `merge`, `splitmid`.

`contains?` and `indexof` use item equality for vectors and lists. For strings,
the second argument is a substring of any length. The empty substring is found
at byte index zero. `indexof` returns `-1` when the item or substring is absent.

`join` is adjacent to sequence words but narrower: it accepts a vector or list
of strings plus a separator and returns a string.

Shared vocabulary means shared semantics, not a lowest-common-denominator
implementation or identical complexity. One-pass words such as `each`, `map`,
`fold`, `filter`, `some`, `all`, and predicate `split` must advance every finite
sequence in O(n), excluding callable work. Intrinsic differences remain visible:
list `uncons` is O(1) because it can share a tail, while vector `uncons` is O(n)
because it returns an independent vector.

### Indexed

Indexed values support efficient random access by integer index:

- vector
- string

Words: `at`, `set-at`, `slice`.

Do not treat every sequence as indexed. A linked list, stream, tree traversal,
or generator can be a sequence without cheap random access.

### Persistent Front

Lists provide constant-time front construction and decomposition with shared
tails.

Words: `cons`, `first`, `rest`, `uncons`.

Repeated `push-back` on a list is quadratic because every call copies the
existing spine. Build in reverse with `cons`, then reverse once:

```toy
( ) [ 1 2 3 4 ] [ swap cons ] fold reverse
\ leaves (1 2 3 4)
```

### Back Stack

Vectors use their last item as the stack top. `push-back` adds one item, `last`
reads the top item, and `pop-back` returns `vector item`. This matches the
constructor input order, so `pop-back push-back` reconstructs the original
vector. Deques share the endpoint update words and use the same `first`/`last`
selectors as other ordered collections.

### Associative

Associative values support lookup by key:

- map

Words: `has?`, `get`, `get-or`, `assoc`, `dissoc`, `keys`, `values`, `pairs`.

Do not overload `at` for maps. Indexed lookup and associative lookup are
separate concepts.

### Membership

Membership values support containment by equality/hash:

- set
- map keys

Words: `has?`, `adjoin`, `remove`, `items`.

This capability is separate from sequence search. Use `contains?` for
vector/list item search or string substring search; use `has?` for map keys and
set items.

Mapping over a set should be explicit through projection:

```toy
myset items [ square ] map >set
```

### Priority

Priority queues expose access by minimum priority.

Words: `pqueue-push`, `pqueue-peek`, `pqueue-pop`, `pqueue-drain`.

The heap layout is not public. `pqueue-drain` projects values in priority order.

## Result Families

Projection words return vectors as the default interchange sequence:

- `range`
- string `split`
- `keys`, `values`, `pairs`
- `items`
- `pqueue-drain`
- `readl`
- `argv`
- `words`

Sequence-transforming words preserve family when the family is part of the
input contract:

| Word           | Input                              | Result                                     |
| -------------- | ---------------------------------- | ------------------------------------------ |
| `map`          | vector/list/string + callable      | same family                                |
| `filter`       | vector/list/string + predicate     | same family                                |
| `split`        | vector/list/string + predicate     | two values of the same family              |
| `merge`        | two sequences of the same family   | same family                                |
| `take`/`dropn` | vector/list/string                 | same family                                |
| `concat`       | two vectors, two lists, two strings | same family                                |
| `reverse`      | vector/list/string                 | same family                                |
| `unique`       | vector/list/string                 | same family                                |
| `sort`         | vector/list/string                 | same family                                |

Indexed update words preserve indexed families:

- vector `set-at` returns a vector
- string `set-at` returns a string

## Conversion Rules

Conversions that change observable semantics must be explicit. Observable
changes include representation type, order, duplicate policy, lookup behavior,
stack effect, and error behavior.

Examples:

```toy
[ 1 2 3 ] >list       \ choose linked-list sequence behavior
( 1 2 3 ) >vector     \ choose indexed vector behavior
[ "a" "b" "c" ] >string \ validate and combine characters
[ 1 2 2 3 ] >set       \ duplicates intentionally collapse
( 1 2 3 ) >deque       \ choose front/back discipline
m keys                 \ choose key projection
m values               \ choose value projection
m pairs                \ choose pair projection
pairs >map             \ build associative structure
pq pqueue-drain        \ choose priority order output
63 >char               \ construct the one-byte string "?"
"?" char-code          \ inspect its unsigned code
```

`>char` accepts integer codes from 0 through 255. `char-code` is its inverse,
and `key` returns the same one-byte string representation. `repr` returns a
source-style string and uses canonical escapes for control and non-ASCII bytes.

Internal layout optimizations may be transparent when observable semantics do
not change: vector capacity growth, hash-table resize, cached hashes,
small-vector storage, copy-on-write, and persistent sharing.

## Equality and Hashing

Structural equality exists for vectors, lists, maps, sets, deques, and priority
queues where the representation semantics define equality. Vector and list are
different representation types, so `[ 1 2 ]` and `( 1 2 )` are not equal even
though they contain the same items.

Initial hash-key policy is conservative:

```text
hashable: bool, int, string, symbol
defer: float, vector, list, map, set, deque, pqueue
```

Floats need a policy for `nan`, `-0`, and `0`. Structural keys can be added
after update semantics and cached hash invalidation are settled.

## Lists

Lists are immutable singly linked values. The list wrapper caches its length,
and independently refcounted nodes allow multiple lists to share a suffix.

| Operation | Complexity | Storage behavior |
| --- | ---: | --- |
| `len`, `empty?`, `first` | O(1) | no node copy |
| `cons` | O(1) | one new node; shares the old list |
| `rest`, `uncons` | O(1) | new wrapper; shares the suffix |
| `last` | O(n) | traverses the spine |
| `take`, `reverse`, `push-back` | O(n) | copies the required nodes |
| `dropn` | O(n) | traverses n nodes, then shares the suffix |
| `concat` | O(length(left)) | copies the left spine and shares the right |
| `splitmid` | O(n) | copies the left half and shares the right half |
| sequence traversal words | O(n) | advance directly by node |
| `sort` | O(n log n) | stable pointer sort, then rebuilds the list |
| `unique` | expected O(n) for hashable scalars | O(n²) equality fallback for unhashable values |

Converting an existing list with `>list`, or an existing vector with `>vector`,
is an identity operation.

## Sorting and Uniqueness

Natural `sort` is stable. Numeric vectors/lists may mix integers and floats;
string elements compare lexicographically. NaN is rejected because it does not
participate in a total numeric order. The current implementation uses stable
insertion sort for runs of at most 16 elements and bottom-up stable merge sort
above that, giving O(n log n) worst-case comparisons and O(n) auxiliary pointer
storage. Strings use insertion sort through 64 bytes and byte-counting sort for
larger inputs.

`unique` preserves the first occurrence. Strings use a 256-entry byte table.
Vectors and lists scan directly while small, then use a temporary set after 16
distinct hashable values. Bool, int, string, and symbol collections therefore
have expected O(n) behavior. Floats and structural values retain the general
O(n²) equality fallback until their hashing policy is defined. These cutoffs are
implementation details selected by [`sequence-algorithms.toy`](../benchmarks/sequence-algorithms.toy).

## Mutability

Toy data words return updated values rather than exposing in-place mutation as a
language guarantee:

```toy
vector idx value set-at
vector item push-back
map key value assoc
map key dissoc
set item adjoin
set item remove
deque item push-front
```

When an endpoint removal returns both state and an item, it returns them in the
constructor's input order. `vector pop-back` leaves `vector item`, while deque
`pop-front` leaves `deque item`. This makes the corresponding pop/push pair an
identity. `uncons` already follows the same rule because `cons` accepts an item
followed by a sequence.

The implementation may optimize by mutating uniquely owned values, using
copy-on-write, or using persistent sharing. User code must not observe aliasing
surprises. Vectors currently keep two items inline and use copy-on-write for
`set-at`, `push-back`, `pop-back`, and `concat`. A vector `concat` reuses the
left vector only when it is uniquely owned; otherwise it allocates a result
after reserving for the exact output length, so an alias never observes the
update. Exact-size vector producers reserve their final element count before
filling, while selective or delimiter-driven producers keep geometric growth
rather than retaining a loose upper bound. Indexed reads are O(1); endpoint
updates are O(1) or amortized O(1) on a unique update chain and O(n) when a
shared vector must be copied. Removing the back item does not shrink vector
capacity.

Strings are immutable flat byte sequences. Short strings and symbols keep their
bytes inside `tf_obj` (up to 22 bytes on 64-bit targets and 10 bytes on 32-bit
targets) without increasing the object size; longer values use one separate
allocation. Exact-size producers fill the final string storage directly, while
dynamic byte buffers transfer their allocation into the result. Indexed byte
reads remain O(1), but they return a newly allocated one-byte string. Heap
strings reuse their otherwise-idle inline bytes to track capacity. `set-at`,
`push-back`, and `concat` may update a uniquely owned string; geometric reserve
makes repeated `push-back` and fixed-chunk `concat` amortized linear in the total
output length. Shared strings are copied before updates, preserving value
semantics. Slices always copy their output, and repeated front `cons` remains
O(n²) because existing bytes must shift even when capacity is available.

## Maps

Maps are insertion-ordered associative structures.

Rules:

- literal syntax is `{ key value ... }`
- constructor input is a vector or list of two-item vector/list pairs
- keys must be hashable
- duplicate keys are an error
- `get` on a missing key is an error
- `has?` checks expected absence
- `get-or` returns a caller-supplied default when a key is absent
- `assoc` and `dissoc` return updated maps
- `keys`, `values`, and `pairs` return vectors

Insertion order costs memory but makes examples, printing, tests, and interop
deterministic.

Maps keep insertion-ordered entries in a dense array and store entry indexes in
an open-addressed hash table. Empty maps allocate no backing storage; the first
insertion allocates four entry slots and eight buckets, and known-size producers
reserve before filling. Lookup and unique-owner `assoc` are expected O(1), with
geometric growth making insertion amortized O(1). Updating a shared map copies
its entries and buckets first, preserving value semantics at O(n). Ordered
`dissoc` is O(n) when a key is present because later entries shift and bucket
indexes must be rebuilt; an absent key is an expected-O(1) no-op. Projection
through `keys`, `values`, or `pairs` is O(n).

## Sets

Sets reuse the same hashing and equality policy as maps.

Rules:

- literal syntax is `#{ item ... }`
- constructor input is a vector, list, or string
- duplicates collapse intentionally in `>set`
- `items` returns a deterministic vector in insertion order
- mapping a set is explicit through `items`

## Deques

Deques are secondary ordered collections for efficient operations at both ends.
They have no literal syntax.

Rules:

- `>deque` accepts vectors, lists, and strings
- conversion preserves front-to-back order
- `push-front` and `push-back` return updated deques
- `pop-front` and `pop-back` return `deque item`, matching the corresponding
  constructor input order
- `first` and `last` consume a non-empty deque and return one end; use
  `dup first` or `dup last` when the deque should be preserved
- `items` projects a deque to a vector
- `len` and `empty?` treat deques as finite collections

## Priority Queues

Priority queues are backed by a heap internally. The heap layout is not public.

Rules:

- `>pqueue` accepts a vector or list of two-item vector/list pairs
- pair shape is `[ priority value ]` or `( priority value )`
- priorities must be finite numbers
- lower priorities are returned first
- equal priorities are returned in insertion order
- `pqueue-push` returns an updated priority queue
- `pqueue-peek` consumes a priority queue and returns the next value; use
  `dup pqueue-peek` when the queue should be preserved
- `pqueue-pop` returns `pqueue value`, leaving the removed value on top; it is
  not an inverse of `pqueue-push` because the stable insertion order is hidden
- `pqueue-drain` projects values to a vector in priority order
- `len` and `empty?` treat priority queues as finite collections

## Implementation Notes

The current implementation uses explicit type switches. That is acceptable
while the set of structures is still small. Introduce shared capability helpers
or operation tables only when repeated switches start obscuring the word
contracts.

Useful future experiments:

- small-vector optimization for short quotations
- chunked or unrolled linked lists for better cache locality
- persistent vector/list implementations
- cached structural hashes
- copy-on-write for uniquely owned values
- map probing and insertion-order storage strategies
