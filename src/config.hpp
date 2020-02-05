#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <stddef.h>
#include <stdint.h>

/* Should statistics be gathered? */
#define STATS

/* Type used for vertex identifiers */
using VertexID = uint16_t;
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
#define CHOICES_FINDER Center_P3, Center_P4, Center_P5, Center_4, Center_5
#define CHOICES_CONSUMER_SELECTOR First, Most, Most_Pruned, Single_Most
#define CHOICES_CONSUMER_BOUND No, Basic, ARW
#define CHOICES_CONSUMER_RESULT
#define CHOICES_GRAPH Matrix
#else
#define CHOICES_MODE Edit
#define CHOICES_RESTRICTION /*None, Undo,*/ Redundant
#define CHOICES_CONVERSION /*Normal,*/ Skip
#define CHOICES_EDITOR ST, MT
#define CHOICES_HEURISTIC
#define CHOICES_FINDER /*Center_P3, Center_P4, Center_P5,*/ Center_4/*, Center_5, Center_6*/
#define CHOICES_CONSUMER_SELECTOR  First, Most, Most_Pruned/*, Gurobi, Single_Most*/
#define CHOICES_CONSUMER_BOUND /*No,*/ ARW, Basic, Updated, Min_Deg, Gurobi
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
