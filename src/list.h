
// list.h

#ifndef LIST_H
#define LIST_H

#include <cstdint>
#include <tuple>
#include "board.h"
#include "util.h"

// constants
const int ListSize = 256;

// macros

#define LIST_CLEAR(list)     ((list)->size=0)
#define LIST_ADD(list,mv)    (std::get<0>(list->move_value[list->size++])=mv)

#define LIST_IS_EMPTY(list)  ((list)->size==0)
#define LIST_SIZE(list)      ((list)->size)

#define LIST_MOVE(list,pos)  (std::get<0>(list->move_value[pos]))
#define LIST_VALUE(list,pos) (std::get<1>(list->move_value[pos]))

// types

struct list_t {
   alignas(32) std::tuple<int16_t,int16_t> move_value[ListSize];
   int size;
};

typedef bool (*move_test_t) (int move, board_t * board);

// functions

extern bool list_is_ok    (const list_t * list);

extern void list_remove   (list_t * list, int pos);

extern void list_copy     (list_t * dst, const list_t * src);

extern void list_sort     (list_t * list);

extern bool list_contain  (const list_t * list, int move);
extern void list_note     (list_t * list);

extern void list_filter   (list_t * list, board_t * board, move_test_t test, bool keep);

#endif // !defined LIST_H

// end of list.h

