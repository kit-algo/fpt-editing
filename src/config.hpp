#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <stddef.h>
#include <stdint.h>

/* Should statistics be gathered */
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

#ifndef MINIMAL
#define CHOICES_MODE Edit /*, Delete, Insert*/
#define CHOICES_RESTRICTION None, Undo, Redundant
#define CHOICES_CONVERSION Normal, Last, Skip
#define CHOICES_EDITOR /*ST,*/ MT
#define CHOICES_HEURISTIC
#define CHOICES_FINDER Center_4, Center_Edits_4, Center_Edits_Sparse_4 //, Center_5, Center_Edits_5, Center_Edits_Sparse_5
#define CHOICES_CONSUMER_SELECTOR First, Least, /*Most,*/ Single
#define CHOICES_CONSUMER_BOUND No, Basic, Single
#define CHOICES_CONSUMER_RESULT
#define CHOICES_GRAPH Matrix
#else
#define CHOICES_MODE Edit
#define CHOICES_RESTRICTION Redundant
#define CHOICES_CONVERSION Skip
#define CHOICES_EDITOR MT
#define CHOICES_HEURISTIC
#define CHOICES_FINDER Center_4
#define CHOICES_CONSUMER_SELECTOR Least, Single
#define CHOICES_CONSUMER_BOUND No, Single
#define CHOICES_CONSUMER_RESULT
#define CHOICES_GRAPH Matrix
#endif

#define LIST_CHOICES_EDITOR EDITOR, FINDER, GRAPH, MODE, RESTRICTION, CONVERSION, CONSUMER_SELECTOR, CONSUMER_BOUND, CONSUMER_RESULT...
#define LIST_CHOICES_HEURISTIC HEURISTIC, FINDER, GRAPH, MODE, RESTRICTION, CONVERSION, CONSUMER_SELECTOR, CONSUMER_BOUND, CONSUMER_RESULT...

#endif
