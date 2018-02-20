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
#define CHOICES_MODE Edit, Delete, Insert
#define CHOICES_RESTRICTION None, Undo, Redundant
#define CHOICES_CONVERSION Normal, Last, Skip
#define CHOICES_EDITOR Editor, MT
#define CHOICES_HEURISTIC
#define CHOICES_FINDER Center_4, Center_Edits_4, Center_Edits_Sparse_4, Center_5, Center_Edits_5, Center_Edits_Sparse_5
#define CHOICES_SELECTOR First, Least, Single, Most
#define CHOICES_LOWER_BOUND No, Basic
#define CHOICES_GRAPH Matrix
#else
#define CHOICES_MODE Edit, Delete, Insert
#define CHOICES_RESTRICTION Redundant
#define CHOICES_CONVERSION Skip
#define CHOICES_EDITOR MT
#define CHOICES_HEURISTIC
#define CHOICES_FINDER Center_4
#define CHOICES_SELECTOR Most
#define CHOICES_LOWER_BOUND No
#define CHOICES_GRAPH Matrix
#endif

#define LIST_CHOICES_EDITOR EDITOR, FINDER, SELECTOR, LOWER_BOUND, GRAPH, MODE, RESTRICTION, CONVERSION
#define LIST_CHOICES_HEURISTIC HEURISTIC, FINDER, SELECTOR, LOWER_BOUND, GRAPH, MODE, RESTRICTION, CONVERSION

#endif
