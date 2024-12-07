
// list.cpp

// includes

#include "board.h"
#include "list.h"
#include "move.h"
#include "util.h"
#include "value.h"

#include <cstdint>
#include <cstring>

#ifdef USE_MTL
#include <MTL/Array.h>
#include <MTL/Stream/AVX.h>
using namespace MTL;
#endif

// constants

static const bool UseStrict = true;

// functions

// list_is_ok()

bool list_is_ok(const list_t * list) {

   if (list == NULL) return false;

   if (list->size < 0 || list->size >= ListSize) return false;

   return true;
}

// list_remove()

void list_remove(list_t* list, int pos)
{
  ASSERT(list_is_ok(list));
  ASSERT(pos>=0&&pos<list->size);

  size_t size = list->size - pos - 1;
  memcpy(list->move  + pos, list->move  + pos + 1, sizeof(list->move[0] ) * size);
  memcpy(list->value + pos, list->value + pos + 1, sizeof(list->value[0]) * size);

  list->size--;
}

void list_copy(list_t * dst, const list_t * src)
{
  ASSERT(dst!=NULL);
  ASSERT(list_is_ok(src));

  dst->size = src->size;
  memcpy(dst->move,  src->move,  sizeof(src->move[0] ) * src->size);
  memcpy(dst->value, src->value, sizeof(src->value[0]) * src->size);
}

void list_sort(list_t * list) {

   int size;
   int i, j;
   int move, value;

   ASSERT(list_is_ok(list));

   // init

   size = list->size;
   list->value[size] = -32768; // HACK: sentinel

   // insert sort (stable)

   for (i = size-2; i >= 0; i--) {

      move = list->move[i];
      value = list->value[i];

      for (j = i; value < list->value[j+1]; j++) {
         list->move[j] = list->move[j+1];
         list->value[j] = list->value[j+1];
      }

      ASSERT(j<size);

      list->move[j] = move;
      list->value[j] = value;
   }
}

// list_contain()

bool list_contain(const list_t * list, int move)
{
  ASSERT(list_is_ok(list));
  ASSERT(move_is_ok(move));

  const uint16_t* p = list->move;
  const uint16_t* pEnd = p + list->size;
#ifdef USE_MTL
  X256<uint16_t> match((uint16_t)move);
  FOR_STREAM_TYPE(p, list->size, uint16_t) {
    X256<uint16_t> mask = X256<uint16_t>(p) == match;
    if (Sum<X256<int64_t>::Increment>(reinterpret_cast<const int64_t*>(mask.pData())) != 0)
      return true;
  }
#endif
  for (; p < pEnd; p++) {
    if (*p == move)
      return true;
  }

  return false;
}

// list_note()

void list_note(list_t * list) {

   int i, move;

   ASSERT(list_is_ok(list));

   for (i = 0; i < list->size; i++) {
      move = list->move[i];
      ASSERT(move_is_ok(move));
      list->value[i] = -move_order(move);
   }
}

// list_filter()

void list_filter(list_t * list, board_t * board, move_test_t test, bool keep) {

   int pos;
   int i, move, value;

   ASSERT(list!=NULL);
   ASSERT(board!=NULL);
   ASSERT(test!=NULL);
   ASSERT(keep==true||keep==false);

   pos = 0;

   for (i = 0; i < LIST_SIZE(list); i++) {

      ASSERT(pos>=0&&pos<=i);

      move = LIST_MOVE(list,i);
      value = LIST_VALUE(list,i);

      if ((*test)(move,board) == keep) {
         list->move[pos] = move;
         list->value[pos] = value;
         pos++;
      }
   }

   ASSERT(pos>=0&&pos<=LIST_SIZE(list));
   list->size = pos;

   // debug

   ASSERT(list_is_ok(list));
}

// end of list.cpp

