#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <stddef.h>
#include <stdint.h>

/* Should statistics be gathered? */
#define STATS

/* Type used for vertex identifiers */
using VertexID = uint8_t;
/* Type used for adjecency matrices */
using Packed = uint64_t;
/* Numer of bits in Packed */
constexpr size_t Packed_Bits = sizeof(Packed) * 8;
/* Function counting trailing zero bits in Packed */
#define PACKED_CTZ __builtin_ctzll
/* Function counting number of set bits in Packed */
#define PACKED_POP __builtin_popcountll

#define MINIMAL
/* Which components are to be compiled?
 * The first set should be a complete list of every component available,
 * the second a reasonable subset aimed at faster compilation.
 * Consumers MUST be listed in every category (…_SELECTOR, …_BOUND, …_RESULT) in which they can be used,
 * Experiments involving a consumer used for a category it is not mentioned in may fail to start, producing an error.
 */
#ifndef MINIMAL
#define CHOICES_MODE Edit, Delete, Insert
#define CHOICES_RESTRICTION None, Undo, Redundant
#define CHOICES_CONVERSION Normal, Last, Skip
#define CHOICES_EDITOR ST, MT
#define CHOICES_HEURISTIC
#define CHOICES_FINDER Center_4, Center_Edits_4, Center_Edits_Sparse_4, Center_5, Center_Edits_5, Center_Edits_Sparse_5
#define CHOICES_CONSUMER_SELECTOR First, Least, /*Most,*/ Single, Single_Edits_Sparse, Single_First, Single_First_Edits_Sparse
#define CHOICES_CONSUMER_BOUND No, Basic, Single, Single_Edits_Sparse, Single_First, Single_First_Edits_Sparse
#define CHOICES_CONSUMER_RESULT
#define CHOICES_GRAPH Matrix
#else
#define CHOICES_MODE Edit
#define CHOICES_RESTRICTION Redundant
#define CHOICES_CONVERSION Skip
#define CHOICES_EDITOR MT
#define CHOICES_HEURISTIC
#define CHOICES_FINDER Center_4, Center_5
#define CHOICES_CONSUMER_SELECTOR First, Least, Single, Single_Edits_Sparse, Single_First, Single_First_Edits_Sparse
#define CHOICES_CONSUMER_BOUND No, Basic, Single, Single_Edits_Sparse, Single_First, Single_First_Edits_Sparse
#define CHOICES_CONSUMER_RESULT
#define CHOICES_GRAPH Matrix
#endif

/* Macros used by choices.py defining which components can be combined and in which order
 * If zero or more choices from a category are allowed, postfix it with ``...''
 * Currently, the code can only handle multiple consumers
 */
#define LIST_CHOICES_EDITOR EDITOR, FINDER, GRAPH, MODE, RESTRICTION, CONVERSION, CONSUMER_SELECTOR, CONSUMER_BOUND, CONSUMER_RESULT...
#define LIST_CHOICES_HEURISTIC HEURISTIC, FINDER, GRAPH, MODE, RESTRICTION, CONVERSION, CONSUMER_SELECTOR, CONSUMER_BOUND, CONSUMER_RESULT...

#endif
