#ifndef CONFIG_HPP
#define CONFIG_HPP

#define STATS

using VertexID = uint8_t;
using Packed = uint64_t;
constexpr size_t Packed_Bits = sizeof(Packed) * 8;

#define MINIMAL

#ifndef MINIMAL
#define CHOICES_MODE Edit, Delete, Insert
#define CHOICES_RESTRICTION None, Undo, Redundant
#define CHOICES_CONVERSION Normal, Last, Skip
#define CHOICES_EDITOR Editor
#define CHOICES_HEURISTIC
#define CHOICES_FINDER Center_Matrix_4, Center_Matrix_5
#define CHOICES_SELECTOR First, Least_Unedited, Single
#define CHOICES_LOWER_BOUND No, Basic
#define CHOICES_GRAPH Matrix
#else
#define CHOICES_MODE Edit
#define CHOICES_RESTRICTION Redundant
#define CHOICES_CONVERSION Skip
#define CHOICES_EDITOR Editor
#define CHOICES_HEURISTIC
#define CHOICES_FINDER Center_4, Center_Edits_4, Linear_4
#define CHOICES_SELECTOR Single, Least_Unedited
#define CHOICES_LOWER_BOUND No, Basic
#define CHOICES_GRAPH Matrix
#endif

#define LIST_CHOICES_EDITOR EDITOR, FINDER, SELECTOR, LOWER_BOUND, GRAPH, MODE, RESTRICTION, CONVERSION
#define LIST_CHOICES_HEURISTIC HEURISTIC, FINDER, SELECTOR, LOWER_BOUND, GRAPH, MODE, RESTRICTION, CONVERSION

#endif
