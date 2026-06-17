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
callable and are not indexed by `geth`, `seth`, or `slice`.

`{ ... }` creates an associative map from alternating key/value forms.
`#{ ... }` creates a membership set with duplicate items rejected in literals.

Secondary structures are built explicitly:

```toy
[ 1 2 3 ] >deque
( 1 2 3 ) >deque
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

Words: `len`, `first`, `rest`, `uncons`, `cons`, `append`, `concat`,
`reverse`, `take`, `dropn`, `each`, `map`, `fold`, `filter`, `split`, `merge`,
`splitmid`, `join`.

### Indexed

Indexed values support efficient random access by integer index:

- vector
- string

Words: `geth`, `seth`, `slice`.

Do not treat every sequence as indexed. A linked list, stream, tree traversal,
or generator can be a sequence without cheap random access.

### Associative

Associative values support lookup by key:

- map

Words: `has?`, `get`, `assoc`, `dissoc`, `keys`, `values`, `pairs`.

Do not overload `geth` for maps. Indexed lookup and associative lookup are
separate concepts.

### Membership

Membership values support containment by equality/hash:

- set
- map keys

Words: `has?`, `adjoin`, `remove`, `items`.

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

Indexed update words preserve indexed families:

- vector `seth` returns a vector
- string `seth` returns a string

## Conversion Rules

Conversions that change observable semantics must be explicit. Observable
changes include representation type, order, duplicate policy, lookup behavior,
stack effect, and error behavior.

Examples:

```toy
[ 1 2 2 3 ] >set       \ duplicates intentionally collapse
( 1 2 3 ) >deque       \ choose front/back discipline
m keys                 \ choose key projection
m values               \ choose value projection
m pairs                \ choose pair projection
pairs >map             \ build associative structure
pq pqueue-drain        \ choose priority order output
```

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

## Mutability

Toy data words return updated values rather than exposing in-place mutation as a
language guarantee:

```toy
vector idx value seth
map key value assoc
map key dissoc
set item adjoin
set item remove
deque item push-front
```

The implementation may later optimize by mutating uniquely owned values, using
copy-on-write, or using persistent sharing. User code should not observe aliasing
surprises.

## Maps

Maps are insertion-ordered associative structures.

Rules:

- literal syntax is `{ key value ... }`
- constructor input is a vector or list of two-item vector/list pairs
- keys must be hashable
- duplicate keys are an error
- `get` on a missing key is an error
- `has?` checks expected absence
- `assoc` and `dissoc` return updated maps
- `keys`, `values`, and `pairs` return vectors

Insertion order costs memory but makes examples, printing, tests, and interop
deterministic.

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
- `pop-front` and `pop-back` return `item deque`
- `front` and `back` inspect one end of a non-empty deque
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
- `pqueue-peek` and `pqueue-pop` return values, not raw heap entries
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
