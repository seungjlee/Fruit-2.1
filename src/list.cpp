
// list.cpp

// includes

#include "board.h"
#include "list.h"
#include "move.h"
#include "util.h"
#include "value.h"

#include <algorithm>
#include <cstring>
#include <limits>

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
  memcpy(list->move_value + pos, list->move_value + pos + 1, sizeof(list->move_value[0]) * size);

  list->size--;
}

void list_copy(list_t * dst, const list_t * src)
{
  ASSERT(dst!=NULL);
  ASSERT(list_is_ok(src));

  dst->size = src->size;
  memcpy(dst->move_value, src->move_value, sizeof(src->move_value[0]) * src->size);
}

void list_sort(list_t * list) {
   ASSERT(list_is_ok(list));

   std::stable_sort(list->move_value, list->move_value + list->size,
                    [](const auto& a, const auto& b) { return std::get<1>(a) > std::get<1>(b); });
}

// list_contain()

bool list_contain(const list_t * list, int move)
{
  ASSERT(list_is_ok(list));
  ASSERT(move_is_ok(move));

  const auto* p = list->move_value;
  const auto* pEnd = p + list->size;
#ifdef USE_MTL
  MTL::X256<int16_t> match((int16_t)move);
  const int32_t* p32 = reinterpret_cast<const int32_t*>(p);
  FOR_STREAM_TYPE(p32, list->size, int32_t) {
    X256<int32_t> values32(p32);
    X256<int16_t> packedValues;
    packedValues.packSaturate(values32,values32);
    X256<int16_t> mask = packedValues == match;
    if (MTL::Array<X256<int64_t>::Increment / 2, int64_t>::Or(reinterpret_cast<const int64_t*>(mask.pData())) != 0)
      return true;
  }
#endif
  for (; p < pEnd; p++) {
    if (std::get<0>(*p) == move)
      return true;
  }

  return false;
}

// list_note()

void list_note(list_t * list) {

   int i, move;

   ASSERT(list_is_ok(list));

   for (i = 0; i < list->size; i++) {
      move = LIST_MOVE(list, i);
      ASSERT(move_is_ok(move));
      LIST_VALUE(list, i) = -move_order(move);
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
         LIST_MOVE(list, pos) = move;
         LIST_VALUE(list, pos) = value;
         pos++;
      }
   }

   ASSERT(pos>=0&&pos<=LIST_SIZE(list));
   list->size = pos;

   // debug

   ASSERT(list_is_ok(list));
}

// end of list.cpp

