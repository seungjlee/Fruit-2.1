#include <cstring>

#include "board.h"
#include "colour.h"
#include "hash.h"
#include "option.h"
#include "pawn.h"
#include "piece.h"
#include "protocol.h"
#include "square.h"
#include "util.h"
#include "search.h"

static constexpr bool UseTable = true;
static constexpr uint32 TableSize = 16384;

typedef pawn_info_t entry_t;

struct pawn_t {
   entry_t* table;
   uint32 size;
   uint32 mask;
   uint32 used;
   sint64 read_nb;
   sint64 read_hit;
   sint64 write_nb;
   sint64 write_collision;
};

// constants and variables

static /* const */ int PawnStructureWeight = 256; // 100%

static const int DoubledOpening = 10;
static const int DoubledEndgame = 20;

static const int IsolatedOpening = 10;
static const int IsolatedOpeningOpen = 20;
static const int IsolatedEndgame = 20;

static const int BackwardOpening = 8;
static const int BackwardOpeningOpen = 16;
static const int BackwardEndgame = 10;

static const int CandidateOpeningMin = 5;
static const int CandidateOpeningMax = 55;
static const int CandidateEndgameMin = 10;
static const int CandidateEndgameMax = 110;

// this was moved to eval.cpp

/*
static const int PassedOpeningMin = 10;
static const int PassedOpeningMax = 70;
static const int PassedEndgameMin = 20;
static const int PassedEndgameMax = 140;
*/

static /* const */ int Bonus[RankNb];
static /* const */ int FileBonus[FileNb]; // JD
static /* const */ int RankBonus[FileNb];

// variables

int BitEQ[16];
int BitLT[16];
int BitLE[16];
int BitGT[16];
int BitGE[16];

int BitFirst[0x100];
int BitLast[0x100];
int BitCount[0x100];
int BitRev[0x100];

static pawn_t Pawn[MaxThreads][1];

static int BitRank1[RankNb];
static int BitRank2[RankNb];
static int BitRank3[RankNb];

// prototypes

static void pawn_comp_info (pawn_info_t * info, const board_t * board);

// functions

// pawn_init_bit()

void pawn_init_bit() {

   int rank;
   int first, last, count;
   int b, rev;

   // rank-indexed Bit*[]

   for (rank = 0; rank < RankNb; rank++) {

      BitEQ[rank] = 0;
      BitLT[rank] = 0;
      BitLE[rank] = 0;
      BitGT[rank] = 0;
      BitGE[rank] = 0;

      BitRank1[rank] = 0;
      BitRank2[rank] = 0;
      BitRank3[rank] = 0;
   }

   for (rank = Rank1; rank <= Rank8; rank++) {
      BitEQ[rank] = 1 << (rank - Rank1);
      BitLT[rank] = BitEQ[rank] - 1;
      BitLE[rank] = BitLT[rank] | BitEQ[rank];
      BitGT[rank] = BitLE[rank] ^ 0xFF;
      BitGE[rank] = BitGT[rank] | BitEQ[rank];
   }

   for (rank = Rank1; rank <= Rank8; rank++) {
      BitRank1[rank] = BitEQ[rank+1];
      BitRank2[rank] = BitEQ[rank+1] | BitEQ[rank+2];
      BitRank3[rank] = BitEQ[rank+1] | BitEQ[rank+2] | BitEQ[rank+3];
   }

   // bit-indexed Bit*[]

   for (b = 0; b < 0x100; b++) {

      first = Rank8; // HACK for pawn shelter
      last = Rank1; // HACK
      count = 0;
      rev = 0;

      for (rank = Rank1; rank <= Rank8; rank++) {
         if ((b & BitEQ[rank]) != 0) {
            if (rank < first) first = rank;
            if (rank > last) last = rank;
            count++;
            rev |= BitEQ[RANK_OPP(rank)];
         }
      }

      BitFirst[b] = first;
      BitLast[b] = last;
      BitCount[b] = count;
      BitRev[b] = rev;
   }
}

// pawn_parameter()

void pawn_parameter() {

   // UCI options

   PawnStructureWeight = (option_get_int("Pawn Structure") * 256 + 50) / 100;

}

// pawn_init()

void pawn_init() {

   int rank, file, ThreadId;

   // UCI options

   pawn_parameter();

   // bonus

   for (rank = 0; rank < RankNb; rank++) {
   
       Bonus[rank] = 0;
       RankBonus[rank] = 0;
   }
   
   for (file = 0; file < FileNb; file++) FileBonus[file] = 0;

   Bonus[Rank4] = 26;
   Bonus[Rank5] = 77;
   Bonus[Rank6] = 154;
   Bonus[Rank7] = 256;
   
   RankBonus[Rank4] = 2;
   RankBonus[Rank5] = 4;
   RankBonus[Rank6] = 8;
   RankBonus[Rank7] = 20;
   
   //FileBonus[FileA] = 0;
   //FileBonus[FileB] = 0;
   FileBonus[FileC] = 1;
   FileBonus[FileD] = 2;   
   FileBonus[FileE] = 2;
   FileBonus[FileF] = 1;
   //FileBonus[FileG] = 0;
   //FileBonus[FileH] = 0;

   // pawn hash-table

   if (UseTable) {		
		for (int ThreadId = 0; ThreadId < NumberThreads; ThreadId++){
			Pawn[ThreadId]->size = TableSize;
			Pawn[ThreadId]->mask = TableSize - 1;
			Pawn[ThreadId]->table = (entry_t *) my_malloc(Pawn[ThreadId]->size*sizeof(entry_t));

         memset(Pawn[ThreadId]->table,0,Pawn[ThreadId]->size*sizeof(entry_t));
         Pawn[ThreadId]->used = 0;
         Pawn[ThreadId]->read_nb = 0;
         Pawn[ThreadId]->read_hit = 0;
         Pawn[ThreadId]->write_nb = 0;
         Pawn[ThreadId]->write_collision = 0;
		}
   }
}

// pawn_free()

void pawn_free() {
	
	int ThreadId;

   ASSERT(sizeof(entry_t)==16);

   if (UseTable) {
      printf("Thread %d - Pawn table [used,collisions,hits]: %d,%lld,%lld  %f\n",
             ThreadId, Pawn[ThreadId]->used, Pawn[ThreadId]->write_collision, Pawn[ThreadId]->read_hit,
             Pawn[ThreadId]->read_hit/double(Pawn[ThreadId]->used+Pawn[ThreadId]->write_collision+Pawn[ThreadId]->read_hit));
      for (ThreadId = 0; ThreadId < NumberThreads; ThreadId++){
		   my_free(Pawn[ThreadId]->table);
		}
   }
}

// pawn_get_info()

void pawn_get_info(pawn_info_t * info, const board_t * board, int ThreadId) {
   uint64 key;
   entry_t * entry;

   ASSERT(info!=NULL);
   ASSERT(board!=NULL);

   if (UseTable) {
#ifdef RECORD_CACHE_STATS
      Pawn[ThreadId]->read_nb++;
#endif

      key = board->pawn_key;
      entry = &Pawn[ThreadId]->table[KEY_INDEX(key)&Pawn[ThreadId]->mask];

      if (entry->lock == KEY_LOCK(key)) {
#ifdef RECORD_CACHE_STATS
         Pawn[ThreadId]->read_hit++;
#endif

         *info = *entry;
         return;
      }
   }

   // calculation

   pawn_comp_info(info,board);

   // store

   if (UseTable) {
#ifdef RECORD_CACHE_STATS
      Pawn[ThreadId]->write_nb++;

      if (entry->lock == 0) { // HACK: assume free entry
         Pawn[ThreadId]->used++;
      } else {
         Pawn[ThreadId]->write_collision++;
      }
#endif

      *entry = *info;
      entry->lock = KEY_LOCK(key);
   }
}

// pawn_comp_info()

struct PawnData {
   int pawn_support;
   int opening;
   int endgame;
   int flags;
   int file_bits;
   int passed_bits;
   int single_file;
};

static void pawn_comp_info(pawn_info_t * info, const board_t * board) {

   int file, rank;
   int me, opp;
   const sq_t * ptr;
   int sq;
   bool backward, candidate, doubled, isolated, open, passed;
   int t1, t2;
   int n;
   int bits;
   int support;
   PawnData pawn_data[ColourNb];

   ASSERT(info!=NULL);
   ASSERT(board!=NULL);

   // pawn_file[]

#if DEBUG
   for (colour = 0; colour < ColourNb; colour++) {

      int pawn_file[FileNb];

      me = colour;

      for (file = 0; file < FileNb; file++) {
         pawn_file[file] = 0;
      }

      for (ptr = &board->pawn[me][0]; (sq=*ptr) != SquareNone; ptr++) {

         file = SQUARE_FILE(sq);
         rank = PAWN_RANK(sq,me);

         ASSERT(file>=FileA&&file<=FileH);
         ASSERT(rank>=Rank2&&rank<=Rank7);

         pawn_file[file] |= BIT(rank);
      }

      for (file = 0; file < FileNb; file++) {
         if (board->pawn_file[colour][file] != pawn_file[file]) my_fatal("board->pawn_file[][]\n");
      }
   }
#endif

   memset(pawn_data, 0, sizeof(pawn_data));

   // features and scoring
   for (int colour = 0; colour < ColourNb; colour++) {

      me = colour;
      opp = COLOUR_OPP(me);

      for (ptr = &board->pawn[me][0]; (sq=*ptr) != SquareNone; ptr++) {

         // init

         file = SQUARE_FILE(sq);
         rank = PAWN_RANK(sq,me);

         ASSERT(file>=FileA&&file<=FileH);
         ASSERT(rank>=Rank2&&rank<=Rank7);

         // flags

         pawn_data[me].file_bits |= BIT(file);
         if (rank == Rank2) pawn_data[me].flags |= BackRankFlag;

         // features
         
         // pawn support/duos
         
         support = 0;
         
         if (me == White){

             if (board->square[sq+1] == WP || board->square[sq-1] == WP)support += 2;
         
             if (board->square[sq+15] == WP || board->square[sq+17] == WP)support += 1;
             else if (board->square[sq-15] == WP || board->square[sq-17] == WP)support += 1;
         } else {
         	 if (board->square[sq+1] == BP || board->square[sq-1] == BP)support += 2;
         
             if (board->square[sq+15] == BP || board->square[sq+17] == BP)support += 1;
             else if (board->square[sq-15] == BP || board->square[sq-17] == BP)support += 1;
         }
         
         if (support > 0){
         	support += FileBonus[file];
         	support += RankBonus[rank];
         	pawn_data[me].pawn_support += support;
         }

         backward = false;
         candidate = false;
         doubled = false;
         isolated = false;
         open = false;
         passed = false;

         t1 = board->pawn_file[me][file-1] | board->pawn_file[me][file+1];
         t2 = board->pawn_file[me][file] | BitRev[board->pawn_file[opp][file]];

         // doubled

         if ((board->pawn_file[me][file] & BitLT[rank]) != 0) {
            doubled = true;
         }

         // isolated and backward

         if (t1 == 0) {

            isolated = true;

         } else if ((t1 & BitLE[rank]) == 0) {

            backward = true;

            // really backward?

            if ((t1 & BitRank1[rank]) != 0) {

               ASSERT(rank+2<=Rank8);

               if (((t2 & BitRank1[rank])
                  | ((BitRev[board->pawn_file[opp][file-1]] | BitRev[board->pawn_file[opp][file+1]]) & BitRank2[rank])) == 0) {

                  backward = false;
               }

            } else if (rank == Rank2 && ((t1 & BitEQ[rank+2]) != 0)) {

               ASSERT(rank+3<=Rank8);

               if (((t2 & BitRank2[rank])
                  | ((BitRev[board->pawn_file[opp][file-1]] | BitRev[board->pawn_file[opp][file+1]]) & BitRank3[rank])) == 0) {

                  backward = false;
               }
            }
         }

         // open, candidate and passed

         if ((t2 & BitGT[rank]) == 0) {

            open = true;

            if (((BitRev[board->pawn_file[opp][file-1]] | BitRev[board->pawn_file[opp][file+1]]) & BitGT[rank]) == 0) {

               passed = true;
               pawn_data[me].passed_bits |= BIT(file);

            } else {

               // candidate?

               n = 0;

               n += BIT_COUNT(board->pawn_file[me][file-1]&BitLE[rank]);
               n += BIT_COUNT(board->pawn_file[me][file+1]&BitLE[rank]);

               n -= BIT_COUNT(BitRev[board->pawn_file[opp][file-1]]&BitGT[rank]);
               n -= BIT_COUNT(BitRev[board->pawn_file[opp][file+1]]&BitGT[rank]);

               if (n >= 0) {

                  // safe?

                  n = 0;

                  n += BIT_COUNT(board->pawn_file[me][file-1]&BitEQ[rank-1]);
                  n += BIT_COUNT(board->pawn_file[me][file+1]&BitEQ[rank-1]);

                  n -= BIT_COUNT(BitRev[board->pawn_file[opp][file-1]]&BitEQ[rank+1]);
                  n -= BIT_COUNT(BitRev[board->pawn_file[opp][file+1]]&BitEQ[rank+1]);

                  if (n >= 0) candidate = true;
               }
            }
         }

         // score

         if (doubled) {
            pawn_data[me].opening -= DoubledOpening;
            pawn_data[me].endgame -= DoubledEndgame;
         }

         if (isolated) {
            if (open) {
               pawn_data[me].opening -= IsolatedOpeningOpen;
               pawn_data[me].endgame -= IsolatedEndgame;
            } else {
               pawn_data[me].opening -= IsolatedOpening;
               pawn_data[me].endgame -= IsolatedEndgame;
            }
         }

         if (backward) {
            if (open) {
               pawn_data[me].opening -= BackwardOpeningOpen;
               pawn_data[me].endgame -= BackwardEndgame;
            } else {
               pawn_data[me].opening -= BackwardOpening;
               pawn_data[me].endgame -= BackwardEndgame;
            }
         }

         if (candidate) {
            pawn_data[me].opening += quad(CandidateOpeningMin,CandidateOpeningMax,rank);
            pawn_data[me].endgame += quad(CandidateEndgameMin,CandidateEndgameMax,rank);
         }

         // this was moved to the dynamic evaluation

/*
         if (passed) {
            pawn_data[me].opening += quad(PassedOpeningMin,PassedOpeningMax,rank);
            pawn_data[me].endgame += quad(PassedEndgameMin,PassedEndgameMax,rank);
         }
*/
      }   
      
      // pawn duos / support
      
	   pawn_data[me].opening += pawn_data[me].pawn_support >> 1;
      pawn_data[me].endgame += pawn_data[me].pawn_support >> 1;   
   }

   // store info

   info->opening = ((pawn_data[White].opening - pawn_data[Black].opening) * PawnStructureWeight) >> 8;
   info->endgame = ((pawn_data[White].endgame - pawn_data[Black].endgame) * PawnStructureWeight) >> 8;

   for (int colour = 0; colour < ColourNb; colour++) {

      me = colour;
      opp = COLOUR_OPP(me);

      // draw flags

      bits = pawn_data[me].file_bits;

      if (bits != 0 && (bits & (bits-1)) == 0) { // one set bit

         file = BIT_FIRST(bits);
         rank = BIT_FIRST(board->pawn_file[me][file]);
         ASSERT(rank>=Rank2);

         if (((BitRev[board->pawn_file[opp][file-1]] | BitRev[board->pawn_file[opp][file+1]]) & BitGT[rank]) == 0) {
            rank = BIT_LAST(board->pawn_file[me][file]);
            pawn_data[me].single_file = SQUARE_MAKE(file,rank);
         }
      }

      info->flags[colour] = pawn_data[colour].flags;
      info->passed_bits[colour] = pawn_data[colour].passed_bits;
      info->single_file[colour] = pawn_data[colour].single_file;
   }
}

// quad()

int quad(int y_min, int y_max, int x) {

   int y;

   ASSERT(y_min>=0&&y_min<=y_max&&y_max<=+32767);
   ASSERT(x>=Rank2&&x<=Rank7);

   y = y_min + ((y_max - y_min) * Bonus[x] + 128) >> 8;
   ASSERT(y>=y_min&&y<=y_max);

   return y;
}

// end of pawn.cpp

