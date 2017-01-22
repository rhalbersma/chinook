/* NOTE -- NEW ACCESS CODE TO ADDRESS SOME ISSUES */


#include <stdio.h>
#ifndef O_BINARY
#define O_BINARY 0
#endif

/* Program looks for files "DB6" and "DB6.idx" in the current directory */
/* If you want to change the file names, look for them in the code.     */

/* Note: this code works for the 6p-databases, but may require some	*/
/* modifications for the 8p-databases.					*/

/* Look at DB_BUFFERS to set the storage requirements you want to use.  */

/* Code has been modified to reduce storage requirements.  With more    */
/* storage, you can rewrite to get more efficient code.                 */

/* Note: code as given only gives the correct result for a position     */
/* where the side to move does not have a capture move, or if the turn  */
/* is switched, then a capture move would be present.                   */
/*                                                                      */
/* Incorrect result returned because white can capture:                 */
/*                                                                      */
/* 8  . - . - . - . -                                                   */
/* 7  - . - . - . - .                                                   */
/* 6  . - . - . - . -                                                   */
/* 5  - . b . b . - .   White to move                                   */
/* 4  . w . - . - . -                                                   */
/* 3  w . - . - . - .                                                   */
/* 2  . - . - . - . -                                                   */
/* 1  - . - . - . - .                                                   */
/*                                                                      */
/* Incorrect result returned because if the turn were switched to       */
/* black, then a capture would be possible.                             */
/* 8  . - . - . - . -                                                   */
/* 7  - . - . - . - .                                                   */
/* 6  . - . b . - . -                                                   */
/* 5  - . b . - . - .   White to move                                   */
/* 4  . w . - . - . -                                                   */
/* 3  - . w . - . - .                                                   */
/* 2  . - . - . - . -                                                   */
/* 1  - . - . - . - .                                                   */

/* Game modes */
#define TESTING         1
#define GAME            2
#define TOURNAMENT      3

#define WHITE   0
#define BLACK   1
#define KINGS   2
#define WKING   2
#define BKING   3

#define PARTIAL 2
#define YES     1
#define NO      0

#define MAX( a, b )     ( (a) > (b) ? (a) : (b) )
#define MIN( a, b )     ( (a) < (b) ? (a) : (b) )
#define ABS( a )        ( (a) >  0  ? (a) : (-a))

#define MAX_DEPTH_SHIFT  6
#define MAX_DEPTH       ( 1 << MAX_DEPTH_SHIFT )

#define GAMEMOVE        0
#define PREDICTION      1
#define OPPNMOVE        2
#define MOVEMADE        3

#define	MAX_PIECES	12

/* Back rank masks */
#define WBACKR  0x01010101
#define BBACKR  0x80808080

/* Column masks */
#define  ODD_COLUMN     0x0f0f0f0f
#define EVEN_COLUMN     0xf0f0f0f0
#define WHITE_HALF      0x33333333
#define BLACK_HALF      0xcccccccc

/* Back 2 ranks */
#define WBACK_2RANKS    (~0x11111111)
#define BBACK_2RANKS    (~0x88888888)

#define MAX_GAME        400
#define MAX_MOVES        50

#define STRING  100

#define ERROR                   ~0
#define ILLEGAL_MOVE            ~1
#define QUIT                    ~2
#define STOP                    ~3
#define TOURN_STOP              2

/* Used to find the leading set bit position */
unsigned char NextBit[ 1<<16 ];
#define NEXT_BIT( POS, BITS )                                   \
        POS = BITS >> 16;                                       \
        if( POS == 0 )                                          \
                POS = NextBit[ BITS ];                          \
        else    POS = NextBit[ POS  ] + 16;

/* Database access */

#define BICOEF          8 /* 0..7 pieces of any time B/W/b/p */

/* whr.h */
typedef unsigned long   WHERE_T;
typedef WHERE_T         * WHERE_VEC_PTR_T;

#define WHERE_T_SZ      sizeof(WHERE_T)

#define BPBOARD         0L
#define WPBOARD         1L
#define BKBOARD         2L
#define WKBOARD         3L

/* definitions for database */

typedef unsigned char           DB_ELEM_T;
typedef DB_ELEM_T       *DB_VEC_PTR_T;

#define DB_ELEM_SZ      sizeof(DB_ELEM_T)

typedef struct sidxstr {
        unsigned sidxbase;
        unsigned sidxindex;
} sidxindex, * sidxindexptr;

typedef struct {
        long            rangeBK;     /* number of Black king configurations */
        long            rangeWK;     /* number of White king configurations */
        long            firstBPIdx;
        long            nextBPIdx;
        long            numPos;
        unsigned      * sidxbase;
	unsigned char * sidxindex;
        unsigned char   nWP;
        unsigned char   nBP;
        unsigned char   nBK;
        unsigned char   nWK;
        unsigned char   rankBP;
        unsigned char   rankWP;
} DB_REC_T;

typedef DB_REC_T        *DB_REC_PTR_T;

#define DB_REC_SZ       sizeof(DB_REC_T)

#define IBLOCK(Index)   ((Index) >> 2)
#define IOFFSET(Index)  ((Index) & 3)

#define GetVal(dbptr, Index)    ((dbptr[IBLOCK(Index)] &                \
                                  RetrieveMask[IOFFSET(Index)]) >>      \
                                  (IOFFSET(Index) << 1))

#define SetVal(dbptr, Index, Val) (dbptr[IBLOCK(Index)] =               \
                                    (dbptr[IBLOCK(Index)] &             \
                                     SetMask[IOFFSET(Index)]) | \
                                    (Val << (IOFFSET(Index)<<1)))

DB_REC_PTR_T    dbCreate();
unsigned long   dbLocbvToSubIdx();
DB_REC_PTR_T    dbValInit();
unsigned long   RotateBoard();

/* Values in the database */
#define TIE     0
#define WIN     1
#define LOSS    2
#define UNKNOWN 3

#define DB_UNKNOWN      -9999

/* Entry for each sub-database.  */
typedef struct db {
        unsigned char defaulttype;      /* Dominant value in the db */
        unsigned char value;            /* Some dbs are all one value - if
                                           so this field is set to that value */
        unsigned short startbyte;       /* Byte where subdatabase starst */
        unsigned long  startaddr;       /* Position number at start byte */
        unsigned long    endaddr;       /* Ending address of db */
        DB_REC_PTR_T db;                /* Pointer to index info */
} DBENTRY, * DBENTRYPTR;

/* bk X wk X bp X wp */
#define DBTABLESIZE     ( 1 << 16 )

/* Given the number w/b kings and checkers (pawns), DBINDEX gives us    */
/* the index into DBTable.  Speeds up finding things.                   */
#define DBINDEX( bk, wk, bp, wp )                               \
        ( (bk << 12) + (wk << 8) + (bp << 4) + wp )

#define Choose( n, k )          ( Bicoef[ n ][ k ] )

/* System dependent parameter */
/* Number of bytes to read in at a time.  This can be increased by mult-*/
/* iples of 2, but the bigger they are, the more expensive it is to de- */
/* compress.  Recommend leaving as is.                                  */
#define DISK_BLOCK              ( 1 << 10 )

/* Amount of storage for the database.  Should be as big as possible.   */
/* Storage is 50 * DISK_BLOCK = 50K - very small.  A few megabytes is   */
/* better.  You can increase this parameter to whatever size you want,  */
/* as long as it does not exceed 32MB total (see next comment).         */
#define DB_BUFFERS              50 /* # of DISK_BLOCK buffers */

/* For each block of 1024 bytes, we have a record. of information */

/* Using short implies that I cannot have more than 2**15 - 1   */
/* buffers.  To have more, change 2 structures below to ints.   */
typedef struct dbbuf {
        int     forward;        /* In a linked list - fwd pointer */
        int     backward;       /* backward pointer */
        long    block;          /* which block of data is this */
        unsigned char data[ DISK_BLOCK ];       /* block of data */
} dbbuffer, * dbbufferptr;


#include <fcntl.h>

/* PC specifdic include files */
/*
#include <alloc.h>
#include <io.h>
#include <sys\stat.h>
*/

long CheckPos();
long DBLookup();
void Setup();
void SkipLine();
void DBInit();
long Nsq();

/* Board vector */
long Turn = BLACK;
unsigned long Locbv[3];

/* Each byte in the database contains either 5 values (3**5=243),       */
/* accounting for values 0..242 in a byte.  The following table tells   */
/* values 243..255.  These values the number of consecutive positions   */
/* whose value is equal to the "dominant" value of the database.  For   */
/* example, Table[13] corresponds to a byte value of 255 (242+13) and   */
/* says that the next 1600 values in the db are all the same as the     */
/* dominant value.                                                      */
int Skip[14] = {
        0, 10, 15, 20, 25, 30, 40, 50, 60, 100, 200, 400, 800, 1600
};

/* Divisors to extract values from a byte.  Byte contains 5 position    */
/* values, each which can be W/L/D.  Div is the number to do a divide/  */
/* mod to extract a value.                                              */
/* The values are stored as ( v1 ) + ( v2 * 3 ) + ( v3 * 3*3 ) +        */
/*              ( v4 * 3*3*3 ) + ( v5 * 3*3*3*3 )                       */
int Div[7] = { 0, 1, 3, 9, 27, 81, 243 };

DBENTRYPTR DBTable[DBTABLESIZE];
int DBFile;

/*
 * Table to map between DB board representation and Chinook's
 * Note: the colours are on opposite sides
 *
 *       Database Board:         Chinook's Board:
 *             WHITE                  BLACK
 *         28  29  30  31          7  15  23  31
 *       24  25  26  27          3  11  19  27
 *         20  21  22  23          6  14  22  30
 *       16  17  18  19          2  10  18  26
 *         12  13  14  15          5  13  21  29
 *        8   9  10  11          1   9  17  25
 *          4   5   6   7          4  12  20  28
 *        0   1   2   3          0   8  16  24
 *            BLACK                    WHITE
 */
/* Note that if your board representation is already the left one, you  */
/* do not need the code to convert it to the right one.                 */

unsigned long RotBoard[256] = {
        0x00000000,     0x10000000,     0x00100000,     0x10100000,
        0x00001000,     0x10001000,     0x00101000,     0x10101000,
        0x00000010,     0x10000010,     0x00100010,     0x10100010,
        0x00001010,     0x10001010,     0x00101010,     0x10101010,
        0x01000000,     0x11000000,     0x01100000,     0x11100000,
        0x01001000,     0x11001000,     0x01101000,     0x11101000,
        0x01000010,     0x11000010,     0x01100010,     0x11100010,
        0x01001010,     0x11001010,     0x01101010,     0x11101010,
        0x00010000,     0x10010000,     0x00110000,     0x10110000,
        0x00011000,     0x10011000,     0x00111000,     0x10111000,
        0x00010010,     0x10010010,     0x00110010,     0x10110010,
        0x00011010,     0x10011010,     0x00111010,     0x10111010,
        0x01010000,     0x11010000,     0x01110000,     0x11110000,
        0x01011000,     0x11011000,     0x01111000,     0x11111000,
        0x01010010,     0x11010010,     0x01110010,     0x11110010,
        0x01011010,     0x11011010,     0x01111010,     0x11111010,
        0x00000100,     0x10000100,     0x00100100,     0x10100100,
        0x00001100,     0x10001100,     0x00101100,     0x10101100,
        0x00000110,     0x10000110,     0x00100110,     0x10100110,
        0x00001110,     0x10001110,     0x00101110,     0x10101110,
        0x01000100,     0x11000100,     0x01100100,     0x11100100,
        0x01001100,     0x11001100,     0x01101100,     0x11101100,
        0x01000110,     0x11000110,     0x01100110,     0x11100110,
        0x01001110,     0x11001110,     0x01101110,     0x11101110,
        0x00010100,     0x10010100,     0x00110100,     0x10110100,
        0x00011100,     0x10011100,     0x00111100,     0x10111100,
        0x00010110,     0x10010110,     0x00110110,     0x10110110,
        0x00011110,     0x10011110,     0x00111110,     0x10111110,
        0x01010100,     0x11010100,     0x01110100,     0x11110100,
        0x01011100,     0x11011100,     0x01111100,     0x11111100,
        0x01010110,     0x11010110,     0x01110110,     0x11110110,
        0x01011110,     0x11011110,     0x01111110,     0x11111110,
        0x00000001,     0x10000001,     0x00100001,     0x10100001,
        0x00001001,     0x10001001,     0x00101001,     0x10101001,
        0x00000011,     0x10000011,     0x00100011,     0x10100011,
        0x00001011,     0x10001011,     0x00101011,     0x10101011,
        0x01000001,     0x11000001,     0x01100001,     0x11100001,
        0x01001001,     0x11001001,     0x01101001,     0x11101001,
        0x01000011,     0x11000011,     0x01100011,     0x11100011,
        0x01001011,     0x11001011,     0x01101011,     0x11101011,
        0x00010001,     0x10010001,     0x00110001,     0x10110001,
        0x00011001,     0x10011001,     0x00111001,     0x10111001,
        0x00010011,     0x10010011,     0x00110011,     0x10110011,
        0x00011011,     0x10011011,     0x00111011,     0x10111011,
        0x01010001,     0x11010001,     0x01110001,     0x11110001,
        0x01011001,     0x11011001,     0x01111001,     0x11111001,
        0x01010011,     0x11010011,     0x01110011,     0x11110011,
        0x01011011,     0x11011011,     0x01111011,     0x11111011,
        0x00000101,     0x10000101,     0x00100101,     0x10100101,
        0x00001101,     0x10001101,     0x00101101,     0x10101101,
        0x00000111,     0x10000111,     0x00100111,     0x10100111,
        0x00001111,     0x10001111,     0x00101111,     0x10101111,
        0x01000101,     0x11000101,     0x01100101,     0x11100101,
        0x01001101,     0x11001101,     0x01101101,     0x11101101,
        0x01000111,     0x11000111,     0x01100111,     0x11100111,
        0x01001111,     0x11001111,     0x01101111,     0x11101111,
        0x00010101,     0x10010101,     0x00110101,     0x10110101,
        0x00011101,     0x10011101,     0x00111101,     0x10111101,
        0x00010111,     0x10010111,     0x00110111,     0x10110111,
        0x00011111,     0x10011111,     0x00111111,     0x10111111,
        0x01010101,     0x11010101,     0x01110101,     0x11110101,
        0x01011101,     0x11011101,     0x01111101,     0x11111101,
        0x01010111,     0x11010111,     0x01110111,     0x11110111,
        0x01011111,     0x11011111,     0x01111111,     0x11111111
};

long Bicoef[33][BICOEF+1];
unsigned long ReverseByte[256];
unsigned long BitPos[32], NotBitPos[32];
unsigned long RetrieveMask[4];

/* This is the number of the biggest database to read.  If you set it   */
/* to 5, for example, the program will only use the 2..5 piece database */
int DBPieces = 8;

/* Used for stoing parts of the database in memory.  DBTop points to the*/
/* head of a linked list of 1024-byte blocks of the database.  The list */
/* is organized so that the oldest parts of the database are the first  */
/* removed to make room for new parts.                                  */
int DBTop;
dbbufferptr DBBuffer;

/* Read in from DB6.idx - index information where in the file to find   */
/* each sub-database, and the starting and ending position numbers for  */
/* each 1024 bytes in the databse.                                      */
long  int * DBIndex;
long  int   MaxBlock;
short int * DBBufPtr;

/*
 * For the complete board we want to use the definitions and indexing
 * procedure below.  But, for now, use a more efficient (though less
 * general) technique.
 */

WHERE_T         BoardPos[4][12];
WHERE_T         *BPK[4];

WHERE_T         BPsave[12];
WHERE_T         *BPKsave;


#define PIECES  nbk, nwk, nbp, nwp, rankbp, rankwp
#define FMTSTR  "About to load %d%d%d%d.%d%d\n"
#define FMTVAR  nbk, nwk, nbp, nwp, rankbp, rankwp


main()
{
        long score;
        char cmd;
	int i;
	unsigned int c;

        printf("Welcome to the Chinook Checkers Program\n");

        printf("\nWait for initialization...\n");
	c = 0;
        NextBit[ 0 ] = 0;
        for( i = 1; i < ( 1<<16 ); i++ )
        {
                if( ( i & ( 1<<c ) ) == 0 )
                        c++;
                NextBit[ i ] = c;
        }

        /* Locbv is our board - 32 bits for white, blank and kings */
        Locbv[WHITE] = Locbv[BLACK] = Locbv[KINGS] = 0;

        /* Read in the database index information */
        DBInit();

        /* Simple interface to illustrate database usage */
        printf( "b\tdisplay board\n" );
        printf( "B\tBlack to move\n" );
        printf( "q\tquit\n" );
        printf( "Q\tQuit\n" );
        printf( "s\tsetup a position (from 1-4, 5-9, etc, using characters \"wWbB. \"\n" );
        printf( "W\tWhite to move\n" );
        printf( "R\tResult of database query\n" );

/* Sample input
*s
8 >B
7 >B
6 >
5 >  w
4 >w
3 >
2 >
1 >

8  . B . - . - . -
7  B . - . - . - .
6  . - . - . - . -
5  - . - . w . - .   Black to move
4  . w . - . - . -
3  - . - . - . - .
2  . - . - . - . -
1  - . - . - . - .
   a b c d e f g h
HEX: 40020 88 88
*R
This position is WIN
**
*/

        for (; ;)
        {
                printf("*");
                scanf("%c", &cmd);

                switch(cmd)
                {
                    case 'b':
                        /* Display the board */
                        Display();
                        break;

                    case 'B':
                        /* Switch turn to BLACK. */
                        Turn = BLACK;
                        break;

                    case 'e':
                        /* Print hexadecimal representation of board */
                        printf("%8x %8x %8x\n", Locbv[WHITE],
                                  Locbv[BLACK], Locbv[KINGS]);
                        break;

                    case 'E':
                        /* Setup board from a hexadecimal representation */
                        scanf("%x %x %x", &Locbv[WHITE],
                                &Locbv[BLACK], &Locbv[KINGS]);
                        Display();
                        break;

                    case 'q':
                    case 'Q':
                        /* Quit */
                        return(0);

                   case 'R':
                        /* Query the database */
                        printf( "The program will not return the correct\n" );
                        printf( "result if the position has a capture move\n" );
                        printf( "pending, or if by switching the turn a\n" );
                        printf( "capture move would now be legal\n" );
                        score = (long) DBLookup();
                        printf("This position is %s\n",
                                (score == WIN ) ? "WIN " :
                                (score == LOSS) ? "LOSS" :  
                                (score == TIE ) ? "DRAW" : "UNKNOWN"  );
                        break;

                    case 's':
                        /* Setup a postion */
                        Setup();
                        break;

                    case 'W':
                        /* Switch turn to WHITE */
                        Turn = WHITE;
                        break;

                    case '\n':
                    case '\t':
                    case ' ':
                        break;

                    default:
                        printf("ERROR: illegal command %c\n", cmd);
                        break;
                }
        }
}

/* DIsplay a board.  Again uses my stupid board representation (see     */
/* above).  Obviously you do not need this routine.                     */
Display()
{
        register int row, column, loc, empty;
        char occupant;

        printf("\n");
        empty = YES;
        for (row = 7; row >= 0; row--)
        {
                printf("%d  ", row + 1);
                if (empty)
                        printf(". ");
                empty = !empty;
                loc = (row >> 1) + ((row & 1) ? 4 : 0);
                for (column = 3; column >= 0; column--)
                {
                        if (Locbv[WHITE] & (1L << loc))
                                occupant = (Locbv[KINGS] & (1L << loc))
                                         ? 'W' : 'w';
                        else if (Locbv[BLACK] & (1L << loc))
                                occupant = (Locbv[KINGS] & (1L << loc))
                                         ? 'B' : 'b';
                        else    occupant = '-';
                        if (column == 0 && empty == NO)
                                printf("%c", occupant);
                        else    printf("%c . ", occupant);
                        loc += 8;
                }
                if (row == 4)
                        printf("  %s to move\n", Turn ? "Black" : "White");
                else    printf("\n");
        }
        printf("   a b c d e f g h\n");
}
/* Allow the user to set up a position.  Again you do not need this routine */
void Setup()
{
        int row, col, addr;
        unsigned long mask;
        char c;

        Locbv[WHITE] = Locbv[BLACK] = Locbv[KINGS] = 0;
        scanf("%c", &c);
        for (row = 7; row >= 0; row--)
        {
                printf("%d >", row + 1);
                addr = (row >> 1);
                if (row & 1)
                        addr += 4;
                for (col = 0; col < 4; col++)
                {
                        mask = (1L << addr);
                        scanf("%c", &c);
                        switch(c)
                        {
                            case ' ':
                            case '.':
                                break;
                            case '\n':
                                col = 4;
                                break;
                            case 'w':
                                Locbv[WHITE] |= mask;
                                break;
                            case 'W':
                                Locbv[WHITE] |= mask;
                                Locbv[KINGS] |= mask;
                                break;
                            case 'b':
                                Locbv[BLACK] |= mask;
                                break;
                            case 'B':
                                Locbv[BLACK] |= mask;
                                Locbv[KINGS] |= mask;
                                break;
                            default:
                                printf("ERROR: illegal setup\n");
                                SkipLine();
                                return;
                        }
                        addr += 8;
                }
                if (c != '\n')
                        scanf("%c", &c);
                if (c != '\n')
                {
                        printf("ERROR: illegal setup\n");
                        SkipLine();
                        return;
                }
        }

        if ((long) CheckPos() == NO)
        {
                printf("ERROR: illegal setup\n");
                return;
        }
        Display();
        printf("HEX: %lx %lx %lx\n",Locbv[0],Locbv[1],Locbv[2]);
}

/* Used by Steup to see if a position is legal - you do not need this code */
long CheckPos()
{
        unsigned long mask;
        long count;

        /* Does this position make sense? */
        /* Count number of men on board - must be <= 12 */
        mask = Locbv[WHITE];
        for (count = 0; mask; count++)
                mask &= (mask -1);
        /* Is there a checker on the last rank? */
        mask = Locbv[WHITE] & ~Locbv[KINGS] & 0x80808080;
        if (count > 12 || mask)
                return(0L);

        /* Count number of men on board - must be <= 12 */
        mask = Locbv[BLACK];
        for (count = 0; mask; count++)
                mask &= (mask -1);
        /* Is there a checker on the last rank? */
        mask = Locbv[BLACK] & ~Locbv[KINGS] & 0x01010101;
        if (count > 12 || mask)
                return(0L);
        
        return(1L);
}

void SkipLine()
{
        char c = ' ';

        while (c != '\n')
                scanf("%c", &c);
}

/* Initialize the database data strucures */

void DBInit()
{
        int i, j, p, w, b, wk, bk, bp, wp, wr, br, stat;
        long dbidx, dbsize;
        long number, index, deftype, addr, byte, min, limit, blocks;
        char dt, dtt, next;
        long * dbindex, buffers;
        int dbffp;
        FILE * dbifp;
        dbbufferptr a;
        DBENTRYPTR ptr;
        short int * dbbufptr;

        /* Initialization */

        /* ReverseByte contains the "reverse" of each byte 0..255. For  */
        /* example, entry 17 (binary 00010001) is a (10001000) - reverse*/
        /* the bits.                                                    */
        for (i = 0; i < 256; i++)
        {
                ReverseByte[i] = 0;
                for (j = 0; j < 8; j++)
                        if (i & (1L << j))
                                ReverseByte[i] |= (1L << (7 - j));
        }

        /* Compute binomial coeficients */
        for (i = 0; i <= 32; i++)
        {
                min = MIN(BICOEF, i);
                for (j = 0; j <= min; j++)
                {
                        if (j == 0 || i == j)
                                Bicoef[i][j] = 1;
                        else    Bicoef[i][j] =
                                Bicoef[i-1][j] + Bicoef[i-1][j-1];
                }
        }
        /* Needed to make sideindex work properly.  Note that if we ever */
        /* do the 7:1 databases, this won't work!                        */
        Bicoef[0][BICOEF] = 0;

        /* Mask for extracting values */
        for (i = 0; i < DB_ELEM_SZ * 4; i++)
                RetrieveMask[i] = 3L << (i << 1);

        /* Mask for bit positons - just trying to make this more efficient*/
        /* by using tables instead of shifts all the time.              */
        for (i = 0; i < 32; i++)
        {
                BitPos[i] = 1L << i;
                NotBitPos[i] = ~BitPos[i];
        }

        /* DBTable is used to speedup looking up an entry.  There is one*/
        /* entry for each possible database.  Note that some of the     */
        /* posible databases do not really exist (e.g. 6 against 0)     */
        for (i = 0; i < DBTABLESIZE; i++)
                DBTable[i] = (DBENTRYPTR) -1;

        printf("Wait for database loading...\n");
        printf("... creating table\n");


        /* Initalize datastructures for each size of database */
        /* For each combination of wk, bk, wp, bp totalling 6 pieces,   */
        /* have an entry in DBTable.  Each entry is a pointer to storage*/
        /* For a pure king endgame, need only 1 entry.  For a db with   */
        /* only 1 side having checkers, need 7 entries (checker can be  */
        /* on one of 7 ranks).  Other dbs need 49 entries - each side   */
        /* has a checker on 1 of 7 ranks.                               */
        for (p = 2; p <= DBPieces; p++)
        {
            for (b = 1; b <= p; b++)
            {
                w = p - b;
                if (w == 0)
                        continue;
                for (bk = b; bk >= 0; bk--)
                {
                    for (wk = w; wk >= 0; wk--)
                    {
                        bp = b - bk;
                        wp = w - wk;
                        if (bp == 0 && wp == 0)
                                number = 1;
                        else if (bp == 0 || wp == 0)
                                number = 7;
                        else    number = 49;
                        index = DBINDEX(bk, wk, bp, wp);

                        /* Allocate storage of 1, 7 or 49 entries */
                        ptr = (DBENTRYPTR) malloc(sizeof(DBENTRY) * number);
                        if (ptr == NULL)
                        {
                                printf("ERROR: malloc failure in DBInit\n");
                                exit(-1);
                        }

                        /* Initialize enach entry */
                        DBTable[index] = ptr;
                        for (i = 0; i < number; i++)
                        {
                                ptr->value       = UNKNOWN;
                                ptr->defaulttype = UNKNOWN;
                                ptr->startaddr   = 0;
                                ptr->startbyte   = 0;
                                ptr->endaddr     = 0;
                                ptr->db          = 0;
                                ptr++;
                        }
                    }
                }
            }
        }

        /* Open database file */
        printf("... using database DB6\n");
        DBFile = open( "DB6", O_BINARY|O_RDONLY);
        if( DBFile == 0 )
        {
                printf("Cannot open DB6\n");
                exit(-1);
        }

        /* Read in index files */
        buffers = 0;
        limit = DBPieces;
        DBPieces = 0;

        /* Index file contains all the sub-database - file is in text   */
        /* each db is in there, and this loop reads in that info.  For  */
        /* example, here is the head of the file:                       */
        /* BASE1100.00 =        DB 1bk, 1wk, 0bp, 0wp - mostly draws    */
        /* S      0       0/0   Start position 0 at block 0, byte 0     */
        /* E    995       0/189 End at position 995, at block 0 byte 189*/
        /*                      By defualt, block size is 1024 bytes    */
        /*                                                              */
        /*                      This says this database of 995 positions*/
        /*                      has been compressed to 189 bytes.       */
        /*                                                              */
        /* BASE1001.00 +        DB 1bk, 0wk, 0bp, 1wp on rank 0. Wins   */
        /* S      0       0/189                                         */
        /* E    125       0/214                                         */
        /*                      125 positions in 214-189=25 bytes       */
        /*                      Most  positions in DB are black wins    */
        /*                                                              */
        /* BASE1001.01 =        DB 1bk, 0wk, 0bp, 1wp on rank 1.  Draws */
        /* S      0       0/214                                         */
        /* etc.                 Interesting.  With the checker on the   */
        /*                      1st rank most pos are draws, on the 0th,*/
        /*                      wins.  Must be because of the move.     */
        /*                                                              */
        /* BASE0011.51 ==       DB 0 kings, 1 bp on 5th, 1 wp on 1st    */
        /*                      == means entire db is drawn.  ++ means  */
        /*                      all db is win; -- all is loss.          */
        /*                                                              */
        /* Here is a bigger db:                                         */
        /* BASE1311.26 -                                                */
        /* S      0    2024/783 Position 0 at block 2024, byte 783      */
        /*                      i.e. starts at addr=2024*1024+783       */
        /* . 327765    2025     Next block (2025), 1st pos is 327,765   */
        /* .1625795    2026     Next block (2026), 1st pos is 1,625,795 */
        /* E1753920    2026/143 Ending addr=2026*1024+143               */
        /*                      Num positions in db = 1,753,920         */
        /*                      Bytes used: endaddr-startaddr=1408      */
        /*                      Nice compression ratio!!                */

                /* Allocate index files */
                /* Need to get size from end of file. */
                dbffp = DBFile;
                dbsize = lseek(dbffp, 0L, 2);
                lseek(dbffp, 0L, 0);
                blocks = (dbsize / DISK_BLOCK);

                /* MaxBlock contains the number of the last block in the */
                /* file.  I check for read errors.  All reads of db get */
                /* DISK_BLOCK bytes - except the last read.             */
                MaxBlock = blocks++;
                printf("... allocating %ld/%ld entry index\n", dbsize, blocks);
                DBIndex = dbindex  = (     long *)
                                malloc(blocks * sizeof(      long));
                DBBufPtr = dbbufptr = (short int *)
                                malloc(blocks * sizeof(short int));
                if (dbindex == NULL || dbbufptr == NULL)
                {
                        printf("ERROR: dbindex malloc\n");
                        exit(-1);
                }

                /* Read index file and build table */
                dbidx = -1;
                dbifp = fopen("DB6.idx", "r");
                if (dbifp == NULL)
                {
                        printf("ERROR: cannot open DB6.idx\n");
                        exit(-1);
                }

                /* Read the entire index file */
                for (i = 0; ;)
                {
                        /* Parse the header line of each sub-database */
                        stat = fscanf(dbifp, "BASE%1d%1d%1d%1d.%1d%1d %c%c",
                                &bk, &wk, &bp, &wp, &br, &wr, &dt, &dtt);
                        if (stat <= 0)
                                break;
                        if (stat < 8)
                        {
                                printf("ERROR: DBInit scanf failure %d\n",
                                                stat);
                                exit(-1);
                        }

                        i = bk + wk + bp + wp;
                        if (i > limit)
                                break;
                        if (i != DBPieces)
                        {
                                printf("... initializing %d piece database\n", i);
                                DBPieces = i;
                        }

                        /* What is the database? */
                        index = DBINDEX(bk, wk, bp, wp);
                        ptr = DBTable[index];
                        if (ptr == NULL)
                        {
                                printf("ERROR: %d %d %d %d %d %d %c dbtab\n",
                                        bk, wk, bp, wp, br, wr, dt);
                                exit(-1);
                        }

                        /* Based on who has checkers, index into the    */
                        /* 1/7/49 entries.                              */
                        if (wp == 0)
                                number = br;
                        else    number = br * 7 + wr;
                        ptr += number;

                        /* If we are not at end of line, we have a db   */
                        /* that is all one value.                       */
                        if (dtt != '\n')
                        {
                                if (dtt == '+')
                                        ptr->value = WIN;
                                else if (dtt == '-')
                                        ptr->value = LOSS;
                                else if (dtt == '=')
                                        ptr->value = TIE;
                                else    printf("ERROR: illegal result %c\n",
                                                dtt);
                                fscanf(dbifp, "\n");
                                continue;
                        }

                        /* Create the index informaiton for the database */
                        ptr->db = dbValInit((long)bk,(long)wk,(long)bp,
                                         (long)wp,(long)br,(long)wr);

                        /* Set the default type */
                        if (dt == '=')
                                deftype = TIE;
                        else if (dt == '+')
                                deftype = WIN;
                        else if (dt == '-')
                                deftype = LOSS;
                        else {
                                printf("ERROR: illegal type %c\n", dt);
                                exit(-1);
                        }
                        ptr->defaulttype = deftype;

                        /* Read in the starting address */
                        next = getc(dbifp);
                        if (next != 'S')
                        {
                                printf("ERROR: illegal start %c\n", next);
                                exit(-1);
                        }
                        stat = fscanf(dbifp, "%ld%ld/%ld\n", &index, &addr,
                                                        &byte);
                        if (stat != 3)
                        {
                                printf("ERROR: illegal start read %d\n",
                                                        stat);
                                exit(-1);
                        }
                        ptr->startaddr = addr;
                        ptr->startbyte = byte;

                        /* If starting address takes us into a new block, */
                        /* update table of indexes.                     */
                        if (addr != dbidx)
                        {
                                dbindex [++dbidx] = index;
                                dbbufptr[  dbidx] = -1;
                        }

                        /* Read each line of the database - block number */
                        /* and position number.                         */
                        for (; ;)
                        {
                                stat = fscanf(dbifp, "%c", &dt);
                                if (stat != 1)
                                {
                                        printf("ERROR: db read1 %d\n", stat);
                                        exit(-1);
                                }
                                if (dt == 'E')
                                        break;
                                stat = fscanf(dbifp, "%ld%ld\n", &index, &addr);
                                if (stat != 2)
                                {
                                        printf("ERROR: db read2 %d\n", stat);
                                        exit(-1);
                                }
                                if (addr != dbidx)
                                {
                                        dbindex [++dbidx] = index;
                                        dbbufptr[  dbidx] = -1;
                                }
                        }
                        stat = fscanf(dbifp, "%ld%ld/%ld\n",
                                                &index, &addr, &byte);
                        if (stat != 3)
                        {
                                printf("ERROR: db read3 %d\n", stat);
                                exit(-1);
                        }
                        ptr->endaddr = addr;
                }
                fclose(dbifp);

        /* Initialize database buffers - this is the storage used to hold */
        /* portions of the database, assuming you do not have enough memory */
        /* to hold the entire db.  If you do, things are *much simpler* */
        buffers += DB_BUFFERS;
        printf("... allocating %d buffers\n", buffers);
        DBBuffer = (dbbufferptr) malloc(buffers * sizeof(dbbuffer));
        if (DBBuffer == NULL)
        {
                printf("ERROR: malloc failure on DBBuffer\n");
                exit(-1);
        }
        printf("... initializing %d buffers\n", DB_BUFFERS);
        for (i = 0; i < DB_BUFFERS; i++)
        {
                a = &DBBuffer[i];
                a->forward  = i + 1;
                a->backward = i - 1;
                a->block    =    -1;
                for (j = DISK_BLOCK -1; j >= 0; j--)
                        a->data[j] = 0;
        }
        DBBuffer[i - 1].forward  = 0;
        DBBuffer[0    ].backward = i - 1;
        DBTop = 0;
}

/* Look up a position in the database */
/* Do not call with one side having 0 pieces */

long DBLookup()
{
        long result, diff, back, fwd, use;
        unsigned long index, cindex, c;
        DBENTRYPTR dbentry;
        long start, end, middle, byte, def;
        unsigned char *buffer;
        long *dbindex;
        short int *dbbufptr;
        int cx, i;

        /* Find out its index */
        index = dbLocbvToSubIdx( &dbentry );

        /* Check if we already know its value because the db is all one value */
        if ((long)index < 0) {
                if (index == -(WIN+1))
                        return((long) WIN);
                if (index == -(LOSS + 1))
                        return((long) LOSS);
                if (index == -(TIE + 1))
                        return((long) TIE);
                if (index == DB_UNKNOWN)
                        return((long) DB_UNKNOWN);
                return(index);
        }

        /* Have to find the value */
        start = dbentry->startaddr;
        end   = dbentry->endaddr;
        byte  = dbentry->startbyte;
        dbindex  = DBIndex;
        dbbufptr = DBBufPtr;

        /* Find the block where the position is located.  Do a binary   */
        /* on the blocks of the databse to find which block the position*/
        /* must be in.                                                  */
        while (start < end) {
                middle = (start + end + 1) / 2;
                if (dbindex[middle] <= index)
                        start = middle;
                else    end = middle - 1;
        }
        if (start != end)
        {
                Display();
                printf("ERROR: cannot agree %d %d \n", start, end);
                exit(-1);
        }

        middle = start;

        /* Is this block in memory?  If not, read it in */
        if (dbbufptr[middle] == -1)
        {
                /* Need a buffer to read into.  Look at the top one (DBTop)*/
                /* If has some data, mark it as invalid */
                if (      DBBuffer[DBTop].block   >= 0)
                    DBBufPtr[DBBuffer[DBTop].block] = -1;
                use = DBTop;

                /* Seek to the block - block# * 1024 */
                if (lseek(DBFile, (long)(middle * DISK_BLOCK), 0) == -1)
                {
                        printf("ERROR: dblookup seek failed\n");
                        return((long) DB_UNKNOWN);
                }

                /* Read in the DISK_BLOCK bytes */
                cx = read(DBFile, &DBBuffer[use].data[0],DISK_BLOCK);
                if (cx != DISK_BLOCK)
                        /* Check for error - should get DISK_BLOCK bytes,*/
                        /* except for the last block */
                        if (middle != MaxBlock)
                        {
                                printf("ERROR: dblookup read failed %d %ld %ld %ld %ld\n",cx,start,end,middle,MaxBlock);
                                return((long) DB_UNKNOWN);
                        }
                /* Mark this buffer as in use */
                dbbufptr[middle] = use;

                /* Fix linked list to point to the next oldest entry */
                DBTop = DBBuffer[DBTop].forward;
                DBBuffer[use].block = middle;
        }
        else {
                /* In memory - no i/o necessary.  Adjust linked list so this */
                /* block is moved to the end of the list, since it is the */
                /* most recently used.                                  */
                use = dbbufptr[middle];
                if (DBTop == use)
                        DBTop = DBBuffer[DBTop].forward;
                else {
                        back = DBBuffer[use].backward;
                        fwd  = DBBuffer[use].forward;
                        DBBuffer[back].forward = fwd; 
                        DBBuffer[fwd].backward = back;

                        back = DBBuffer[DBTop].backward;
                        DBBuffer[back].forward = use;
                        DBBuffer[use].backward = back;
                        DBBuffer[use].forward = DBTop;
                        DBBuffer[DBTop].backward = use;
                }
        }

        /* We have the block - now need to find the position value. */
        if( start == dbentry->startaddr )
        {
                i = byte;
                cindex = 0;
        }
        else {
                i = 0;
                cindex = dbindex[ start ];
        }
        def = dbentry->defaulttype;
        buffer = &DBBuffer[ use ].data[ 0 ];

        /* Scan through block until we find the position.  cindex is    */
        /* current position number; i is the starting byte.             */
        for( ; i < DISK_BLOCK; i++ )
        {
                c = buffer[ i ] & 0xff;
                if( c > 242 )
                {
                        /* Byte is a "skip" byte.  Skip tells us how many*/
                        /* positions in a row have the same value.       */
                        cindex += Skip[ c - 242 ];
                        if( index < cindex )
                        {
                                /* If "skip"ing takes us past the pos'n */
                                /* number, we know the positions value.  */
                                result = def;
                                break;
                        }
                }                     
                else {   
                        /* Byte contains 5 values */
                        if( cindex + 5 <= index )
                                /* Did not find the position - skip 5 pos */
                                cindex += 5;     
                        else {                   
                                /* Found - extract the value */
                                diff = index - cindex;
                                result = c % Div[ 5 - diff + 1 ];
                                result = result / Div[ 5 - diff ];
                                break;                            
                        }                                         
                }                     
        }                

        /* Safety check */
        if (i < 0 || i >= DISK_BLOCK)
        {
                Display();
                printf("ERROR: scanned block and did not find\n");
                printf("INDEX %d CINDEX %d\n", index, cindex);
                printf("Start %d end %d byte %d = middle %d\n",
                        dbentry->startaddr,
                        dbentry->endaddr,
                        dbentry->startbyte, middle);
                exit(-1);
        }

        if (result == WIN)
                return((long) WIN);
        if (result == LOSS)
                return((long) LOSS);
        return((long) TIE);
}


DB_REC_PTR_T dbValInit(PIECES)
        long PIECES;
{
        return(dbCreate(PIECES));
}

/*
 * Allocates memory for a DB_REC_T and initializes its scalars and SIdx.
 */
DB_REC_PTR_T dbCreate(PIECES)
        long PIECES;
{
        DB_REC_PTR_T p;

        p = (DB_REC_PTR_T) malloc(DB_REC_SZ);
        /*
         * Compute the number of black and white king configurations for this
         * database slice
         */
        p->rangeBK = Choose(32 - nbp - nwp, nbk);
        p->rangeWK = Choose(32 - nbp - nwp - nbk, nwk);

        /* Sets p->sidxbase and p->sidxindex */
        sidxCreate(p, nbp, nwp, rankbp, rankwp,
                        &(p->numPos), &(p->firstBPIdx), &(p->nextBPIdx));
        p->numPos *= p->rangeBK * p->rangeWK;
        p->nBP = nbp;
        p->nWP = nwp;
        p->nBK = nbk;
        p->nWK = nwk;
        p->rankBP = rankbp;
        p->rankWP = rankwp;
        return(p);
}

#define LeadWP (7 - (BoardPos[WPBOARD][db->nWP-1] >> 2))

/*
 * CountHits - count the number of pieces occupying squares less than the
 *             current square
 *
 * Parameters:
 *
 *      XXpos   piece array to be counted
 *      nXX     number of pieces in XXpos
 *      YYpos   piece array to be compared against
 *      nYY     number of pieces in YYpos
 *      XXhits  an array containing the number of YYpos pieces which occupy
 *              squares less that those in XXpos.  XXhits[n] has the number
 *              of YYpos pieces in lesser squares than the piece XXpos[n].
 *
 * Example:
 *
 *      Suppose there are black kings on squares 1 and 11, and white kings
 *      on squares 10 and 26.  This routine sets XXhits as follows:
 *
 *      XXpos   26 10           YYpos   11 1            XXhits  2 1
 *      nXX     2               nYY     2
 */
#define CountHits(XXpos, nXX, YYpos, nYY, XXhits) \
{ \
        register long x, y; \
        if (nYY > 0) { \
                for (x = 0; x < nXX; x++) { \
                        if (XXpos[x] < YYpos[nYY-1]) \
                                break; \
                        XXhits[x]++; \
                        for (y = nYY - 2; y >= 0; y--) { \
                                if (XXpos[x] < YYpos[y]) \
                                        break; \
                                XXhits[x]++; \
                        } \
                } \
        } \
}

/*
 * CountRevHits - count the number of pieces occupying squares greater than
 *                the current square
 *
 * Parameters:
 *
 *      XXpos   piece array to be counted
 *      nXX     number of pieces in XXpos
 *      YYpos   piece array to be compared against
 *      nYY     number of pieces in YYpos
 *      XXhits  an array containing the number of YYpos pieces which occupy
 *              squares greater that those in XXpos.  XXhits[n] has the number
 *              of YYpos pieces in greater squares than the piece XXpos[n].
 *
 * This routine is similar to CountHits, except it is used for computing the
 * index of the white checkers (since, unlike the other piece types, they are
 * always placed starting from the top of the board).
 */
#define CountRevHits(XXpos, nXX, YYpos, nYY, XXhits) \
{ \
        register long x, y; \
        if (nYY > 0) { \
                for (x = nXX - 1; x >= 0; x--) { \
                        if (XXpos[x] > YYpos[0]) \
                                break; \
                        XXhits[x]++; \
                        for (y = 1; y < nYY; y++) { \
                                if (XXpos[x] > YYpos[y]) \
                                        break; \
                                XXhits[x]++; \
                        } \
                } \
        } \
}

/*
 * SquishBP - squish the black checker configuration and determine its
 *            reverse index
 *
 * Parameters:
 *
 *      nXX     number of black checkers
 *      XXpos   array containing square numbers (in descending order)
 *
 * Since the black checkers are placed on the board first, there are no
 * conflicts with other pieces and hence no "squishing" is required to
 * determine its reverse index.  Thus, all that is necessary is to compute
 * its reverse index.
 */
#define SquishBP(nXX, XXpos) \
{ \
        if (nXX) { \
                bpidx = DBrevindex(XXpos, nXX); \
                rankbp = XXpos[0] >> 2; \
        } \
        else { \
                bpidx = 0; \
                rankbp = 0; \
        } \
}

/*
 * SquishWP - squish the white checker configuration and determine its
 *            reverse index
 *
 * Parameters:
 *
 *      nXX     number of white checkers
 *      nYY     number of black checkers
 *      XXpos   array containing white checker square numbers
 *      YYpos   array containing black checker square numbers
 *
 * Since the white checkers are placed on the board after the black checkers,
 * there may have been possible conflicts with the black checkers during the
 * enumeration process.  As a result, the computation applied directly to the
 * black checker configuration cannot be applied to the white checker
 * configuration without first "squishing" the white checker configuration.
 * This is done by determining how many of the black checkers interfere with
 * each white checker, and then decrementing this amount from each white
 * checker square.  After this is complete, the reverse index is computed.
 */
#define SquishWP(nXX, nYY, XXpos, YYpos) \
{ \
        register long i; \
        if (nXX) { \
                for (i = nXX - 1; i >= 0; i--) \
                        XXhits[i] = 0; \
                CountRevHits(XXpos, nXX, YYpos, nYY, XXhits); \
                for (i = nXX - 1; i >= 0; i--) \
                        XXhits[i] = 31 - (XXpos[i] + XXhits[i]); \
                wpidx = DBindex(XXhits, nXX); \
                rankwp = (31 - XXpos[nXX - 1]) >> 2; \
        } \
        else { \
                wpidx = 0; \
                rankwp = 0; \
        } \
}

/*
 * SquishBK - squish the black king configuration and determine its
 *            reverse index
 *
 * Parameters:
 *
 *      nXX     number of black kings
 *      nYY     number of white checkers
 *      nZZ     number of black checkers
 *      XXpos   array containing black king square numbers
 *      YYpos   array containing white checker square numbers
 *      ZZpos   array containing black checker square numbers
 *
 * Since the black kings are placed on the board after the white checkers,
 * there may have been possible conflicts with both the white checkers and
 * the black checkers during the enumeration process.  As with the white
 * checkers, the black king configuration must first be "squished" to
 * determine its correct index.  This is done by determining how many of
 * the white and black checkers interfere with each black king, and then
 * decrementing this amount from each black king square.  After this is
 * complete, the reverse index is computed.
 */
#define SquishBK(nXX, nYY, nZZ, XXpos, YYpos, ZZpos) \
{ \
        register long i; \
        if (nXX) { \
                for (i = nXX - 1; i >= 0; i--) \
                        XXhits[i] = 0; \
                CountHits(XXpos, nXX, YYpos, nYY, XXhits); \
                CountHits(XXpos, nXX, ZZpos, nZZ, XXhits); \
                for (i = nXX - 1; i >= 0; i--) \
                        XXhits[i] = XXpos[i] - XXhits[i]; \
                bkidx = DBrevindex(XXhits, nXX); \
        } \
        else bkidx = 0; \
}

/*
 * SquishWK - squish the white king configuration and determine its
 *            reverse index
 *
 * Parameters:
 *
 *      nXX     number of white kings
 *      nYY     number of black kings
 *      nZZ     number of white checkers
 *      nYZ     number of black checkers
 *      XXpos   array containing white king square numbers
 *      YYpos   array containing black king square numbers
 *      ZZpos   array containing white checker square numbers
 *      YZpos   array containing black checker square numbers
 *
 * Since the white kings are placed on the board after the black kings,
 * there may have been possible conflicts with the black kings, white
 * checkers, and the black checkers during the enumeration process.  As
 * with the white checkers and black kings, the white king configuration
 * must first be "squished" to determine its correct index.  This is done
 * by determining how many of the black kings, white checkers, and black
 * checkers interfere with each white king, and then decrementing this
 * amount from each white king square.  After this is complete, the reverse
 * index is computed.
 */
#define SquishWK(nXX, nYY, nZZ, nYZ, XXpos, YYpos, ZZpos, YZpos) \
{ \
        register long i; \
        if (nXX) { \
                for (i = nXX - 1; i >= 0; i--) \
                        XXhits[i] = 0; \
                CountHits(XXpos, nXX, YYpos, nYY, XXhits); \
                CountHits(XXpos, nXX, ZZpos, nZZ, XXhits); \
                CountHits(XXpos, nXX, YZpos, nYZ, XXhits); \
                for (i = nXX - 1; i >= 0; i--) \
                        XXhits[i] = XXpos[i] - XXhits[i]; \
                wkidx = DBrevindex(XXhits, nXX); \
        } \
        else wkidx = 0; \
}

/*
 * EXTRACT_PIECES - extract pieces into an array
 *
 * Parameters:
 *
 *      vec     a 32 bit unsigned integer describing pieces on squares
 *              according to the database format
 *      nXX     number of pieces extracted
 *      XXpos   array containing square numbers (in descending order)
 *
 * Example: Suppose vec = 0x00000003 (which corresponds to pieces on database
 * squares 0 and 1 (1 and 2 in the checkers literature).  EXTRACT_PIECES will
 * set nXX and XXpos as follows:
 *
 *              nXX     2
 *              XXpos   1 0
 */
#define EXTRACT_PIECES(vec, nXX, XXpos)				\
{								\
        nXX = 0;						\
        while (vec) {						\
                NEXT_BIT( pos, vec );				\
		XXpos[nXX] = pos;				\
                vec ^= (unsigned long)(1L << XXpos[(nXX)++]);	\
        }							\
}

/*
-- In: a black-to-move board in Locbv[]
-- Out: Returns the index of the required 2-bit value.
*/
/*
 * dbLocbvToSubIdx - Compute a position index
 */

unsigned long dbLocbvToSubIdx( DBENTRYPTR * dbentry )
{
	int		nbk, nwk, nbp, nwp, rankbp, rankwp;
	int		bppos[MAX_PIECES], wppos[MAX_PIECES];
	int		bkpos[MAX_PIECES], wkpos[MAX_PIECES];
	int		XXhits[MAX_PIECES];
	int		bpidx, wpidx, bkidx, wkidx, firstidx;
	u_int		vec, Blackbv, Whitebv, Kingbv;
	int		Index, pos;
	unsigned      *	baseptr;
	unsigned char *	indexptr;
	DB_REC_PTR_T	db;
	union {
		u_int U;
		u_char C[4];
	} a, b;

	/* If it's white to move, reverse the board & look up black to move. */
	if (Turn == WHITE) {
		a.U = Locbv[WHITE];
		b.C[3] = ReverseByte[a.C[0]];
		b.C[2] = ReverseByte[a.C[1]];
		b.C[1] = ReverseByte[a.C[2]];
		b.C[0] = ReverseByte[a.C[3]];
		Blackbv = b.U;
		a.U = Locbv[BLACK];
		b.C[3] = ReverseByte[a.C[0]];
		b.C[2] = ReverseByte[a.C[1]];
		b.C[1] = ReverseByte[a.C[2]];
		b.C[0] = ReverseByte[a.C[3]];
		Whitebv = b.U;
		a.U = Locbv[KINGS];
		b.C[3] = ReverseByte[a.C[0]];
		b.C[2] = ReverseByte[a.C[1]];
		b.C[1] = ReverseByte[a.C[2]];
		b.C[0] = ReverseByte[a.C[3]];
		Kingbv = b.U;
	}
	else {
		Blackbv = Locbv[BLACK];
		Whitebv = Locbv[WHITE];
		Kingbv = Locbv[KINGS];
	}

	/* Extract and sort the black pawns */
	vec = RotateBoard(Blackbv & ~ Kingbv);
	EXTRACT_PIECES(vec, nbp, bppos);
	SquishBP(nbp, bppos);

	/* Extract and sort the white pawns */
	vec = RotateBoard(Whitebv & ~ Kingbv);
	EXTRACT_PIECES(vec, nwp, wppos);
	SquishWP(nwp, nbp, wppos, bppos);

	/* Extract and sort the black kings */
	vec = RotateBoard(Blackbv & Kingbv);
	EXTRACT_PIECES(vec, nbk, bkpos);
	SquishBK(nbk, nwp, nbp, bkpos, wppos, bppos);

	/* Extract and sort the white kings */
	vec = RotateBoard(Whitebv & Kingbv);
	EXTRACT_PIECES(vec, nwk, wkpos);
	SquishWK(nwk, nbk, nwp, nbp, wkpos, bkpos, wppos, bppos);

	/* Only consider positions where black dominates white */
	*dbentry = DBTable[ DBINDEX( nbk, nwk, nbp, nwp ) ];
	if( *dbentry == 0 )
	{
		printf( "ERROR: cannot happen!\n" );
		exit( -1 );
	}
	if( nwp == 0 )
		*dbentry += rankbp;
	else	*dbentry += ( rankbp * 7 + rankwp );
	if( *dbentry == NULL )
		return( (unsigned) (DB_UNKNOWN - 1) );

	if( (*dbentry)->value != UNKNOWN )
		return( (unsigned) -( (*dbentry)->value + 1 ) );

	/* Determine wpidx, then combine bp & wp indices into bpidx. */
	db    = (*dbentry)->db;
	if( db == NULL )
		return( (unsigned) (DB_UNKNOWN - 1) );

	baseptr  = db->sidxbase;
	indexptr = db->sidxindex;
	bpidx -= db->firstBPIdx;

	/*
	 * Compute the position index by:
	 *
	 * Index = (BASE * rangeBK * rangeWK) +			# BP positions
	 *	   ((wpidx - FIRST) * rangeBK * rangeWK) +	# WP positions
	 *	   (bkidx * rangeWK) +				# BK positions
	 *	    wkidx					# WK positions
	 */
	firstidx = Choose( ( indexptr[bpidx] >> 3 ),
			   ( indexptr[bpidx] & 07 ) );
	bpidx = baseptr[bpidx] + wpidx - firstidx;
	Index = (((bpidx * db->rangeBK) + bkidx) * db->rangeWK) + wkidx;
	return(Index);

} /* dbLocbvToSubIdx() */

/*
 * RotateBoard - convert squares from Chinook's representation to the
 *               database representation
 *
 * Parameter:
 *
 *      vecrot  32 bit value representing piece locations in Chinook's board
 *              representation.
 *
 * See the comment near RotBoard (above) for a mapping of the squares from
 * Chinook's representation to the database representation.
 */
unsigned long RotateBoard(vecrot)
        register unsigned long vecrot;
{
        register unsigned long vec;

        vec  = (RotBoard[(vecrot >> 24) & 0xFF]);
        vec |= (RotBoard[(vecrot >> 16) & 0xFF] << 1);
        vec |= (RotBoard[(vecrot >>  8) & 0xFF] << 2);
        vec |= (RotBoard[vecrot & 0xFF] << 3);
        return(vec);
}

/*
 * A database slice is determined by the leading rank of the black and white
 * checkers.  A position index within the slice is computed by the position
 * of the black checkers, then adding the white checkers, followed by the
 * black kings, and finally the white kings.
 *
 * The following "|" notation refers to "choose";
 *  i.e. | n | = n!/((n-k)!k!).
 *       | k |
 *
 * The number of positions with nbp black checkers with leading rank rbp
 * (where 0 <= rbp <= 7) is MaxBP = | 4 x (rbp+1) | - | 4 x rbp |.
 *                                  |     nbp     |   |  nbp    |
 *
 * Next the white checkers are added.  This is discussed in more detail below.
 *
 * The number of positions created by adding nbk black kings is
 *      MaxBK = | 32 - nbp - nwp |
 *              |      nbk       |
 *
 * and the number of positions created by adding nwk white kings is
 *      MaxWK = | 32 - nbp - nwp - nbk |.
 *              |         nwk          |
 *
 *
 * Computing the number of positions with nbp black checkers having leading
 * rank rbp and nwp white checkers having leading rank rwp is difficult because
 * some of the black checker configurations may have one or more checkers
 * within the first rwp checker squares.  Thus the simple formula applied to
 * computing MaxBP cannot be applied to MaxWP because different black checker
 * configurations may have a different number of white checker configurations.
 * MaxWP is computed by creating a set of subindexes for every black checker
 * configuration.  The number of white checker positions for each black checker
 * configuration is computed and stored, as well as a cumulative total.  The
 * amount of storage required can be quite large for a large number of black
 * checkers.
 *
 * See Appendix B in the Technical Report TR93-13 for more details.
 */

sidxCreate(p, nBP, nWP, rankBPL, rankWPL, numPos, firstBPLIdx, nextBPLIdx)
        DB_REC_PTR_T p;
        long nBP, nWP, rankBPL, rankWPL;
        long *numPos, *firstBPLIdx, *nextBPLIdx; /* VAR parameters */
{
        unsigned long firstWPLIdx, nextWPLIdx, Base;
	unsigned * baseptr;
        unsigned char  * indexptr;
        unsigned long Maximum;
        long nsqr1, nsqr2;
        long fsq, fpc;

        if (nBP == 0) {
                *firstBPLIdx = 0;
                *nextBPLIdx = 1;
        }
        else {
                /*
                 * Determine the number of black positions which have one
                 * or more pawns on rankBPL.  This number is obtained by
                 * (4*(rank+1) choose nBP) - (4*rank choose nBP).
                 */
                *firstBPLIdx = Choose(rankBPL << 2, nBP);
                *nextBPLIdx  = Choose((rankBPL + 1) << 2, nBP);

#ifdef UNUSED
                /*
                 * Don't fill the entire rank with black pawns; leave room
                 * for one white pawn.
                 */
                if (nBP == 4 && (rankBPL + rankWPL) == 7)
                        (*nextBPLIdx)--;
#endif
        }

        /*
         * Allocate the secondary index array.  This array holds the
         * cumulative number of white checker positions for each black
         * checker configuration.
         */
        p->sidxbase = baseptr = (unsigned *) malloc (
                (unsigned) (1 + *nextBPLIdx - *firstBPLIdx) *
                (unsigned) sizeof( unsigned ) );
	p->sidxindex = indexptr = (unsigned char *) malloc(
                (unsigned) (1 + *nextBPLIdx - *firstBPLIdx) *
                (unsigned) sizeof( unsigned char ) );

        /*
         * This array holds the starting index for each white checker
         * configuration.  It is used later in dbLocbvToSubIdx to compute
         * a position index.
         */

        /* initialize the array with FIRST & BASE */
        Base = 0;
        if (nWP == 0) {
                firstWPLIdx = 0;
                fsq = 0;
                fpc = BICOEF - 1;
                nextWPLIdx  = 1;
        }
        if (nBP) {
                SaveIndex(BPBOARD);
                InitIndex(BPBOARD, nBP, MAX(rankBPL << 2, nBP-1));
        }
        /* Do, for each black checker configuation... */
        for (Maximum = *nextBPLIdx - *firstBPLIdx; Maximum; Maximum--) {
                if (nWP) {
                        /* Compute available squares for white checkers */
                        if (nBP) {
                                nsqr1 = Nsq(rankWPL-1, BoardPos[BPBOARD], nBP);
                                nsqr2 = Nsq(rankWPL,   BoardPos[BPBOARD], nBP);
                        }
                        else {
                                nsqr1 = (rankWPL)   << 2;
                                nsqr2 = (rankWPL+1) << 2;
                        }
                        /*
                         * nextWPLIdx - firstWPLidx represents the number of
                         * white checker positions for this black checker
                         * configuration
                         */
                        firstWPLIdx = Choose(nsqr1, nWP);
                        nextWPLIdx  = Choose(nsqr2, nWP);

                        /*
                         * Save the starting index for this white checker
                         * configuration.  This is used later to compute
                         * the index for a position.
                         */
                        fsq = nsqr1;
                        fpc = nWP;
                }

                /* store these values for future reference */
                *baseptr = Base;
                *indexptr = ( fsq << 3 ) | fpc;
                 baseptr++;
	 	 indexptr++;

                /*
                 * Add the number of white checker positions to the cumulative
                 * amount
                 */
                Base += (nextWPLIdx - firstWPLIdx);

                /* Get the next black checker configuration */
                if (nBP)
                        NextIndex(BPBOARD);
        }
        /* Restore the original black checker configuration */
        if (nBP)
                LoadIndex(BPBOARD);
        *numPos = Base;
} /* sidxCreate() */

/*
 * Nsq - return the number of squares available for placement of the white
 * checkers.
 *
 * Paramters:
 *      rank    leading white checker rank
 *      Ppos    an array describing the square number of the black checkers
 *      nP      is the number of black checkers.
 *
 * Although this routine is generalized, it is used only for determining white
 * checker squares.
 */
long Nsq(rank, Ppos, nP)
        register long rank;
        register long nP;
        WHERE_T *Ppos;
{
        register long pawnhit  = ((7 - rank) << 2);
        register long nsquares = ((rank + 1) << 2);

        while (nP--) {
                if (*Ppos++ >= pawnhit)
                        nsquares--;
        }
        return(nsquares);
}

/*
 * The next set of routines initializes and increments the piece counters.
 * BoardPos is a 4x12 array used to store the square number for each of the
 * potential piece types in the following manner:
 *
 *              BoardPos[0] => Black checkers (BPBOARD)
 *              BoardPos[1] => White checkers (WPBOARD)
 *              BoardPos[2] => Black kings (BKBOARD)
 *              BoardPos[3] => White kings (WKBOARD)
 *
 * All square numbers for the Kings and Black checkers are stored in increasing
 * numerical order, from left to right.  The white checker square numbers are
 * stored in decreasing numerical order, left to right.
 *
 * BPK[n] is a pointer to the last piece in BoardPos[n].
 */

/*
 * InitIndex - initialize a piece counter
 *
 * Parameters:
 *      off     piece type (BPBOARD, WPBOARD, BKBOARD, or WKBOARD)
 *      k       number of pieces
 *      n       starting square number of highest ranked piece
 */
InitIndex(off, k, n)
        register long off, k, n;
{
        register WHERE_T *BPP, *BPE;

        /* Initialize all the pieces in the starting position. */
        BPK[off] = &BoardPos[off][k];
        for (BPP = BoardPos[off], BPE = &BPK[off][-1]; BPP < BPE; BPP++)
                *BPP = (long)(BPP - BoardPos[off]);
        *BPE = n;
}

/*
 * NextIndex - get the next position for a subset of pieces
 *
 * Parameters:
 *
 *      off     piece type (BPBOARD, WPBOARD, BKBOARD, or WKBOARD)
 *
 * This routine sets the next position for a subset of pieces.  For example,
 * suppose three black checkers occupy database squares 0,1,2 (which correspond
 * to squares 1,2, and 3 in checkers literature).  Successive calls to this
 * routine sets the following checker configurations:
 * (0,1,2) (0,1,3) (0,2,3) (1,2,3) (0,1,4) etc.
 *
 * The number of times NextIndex is called is known in advance so no overflow
 * checking is necessary.
 */
        
NextIndex(off)
        register long off;
{
        register WHERE_T *BPP, *BPE = &BPK[off][-1];

        for (BPP = BoardPos[off]; BPP < BPE && *BPP == BPP[1] - 1; BPP++)
                *BPP = (long)(BPP - BoardPos[off]);
        *BPP += 1;
}

/*
 * SaveIndex - saves the current position index for future reference
 *
 * Parameters:
 *
 *      off     piece type (BPBOARD, WPBOARD, BKBOARD, or WKBOARD)
 */
SaveIndex(off)
        register long off;
{
        register WHERE_T *BPP = BoardPos[off];
        register WHERE_T *pos = BPsave;
        register long k = 12;

        BPKsave = BPK[off];
        while (k--)
                *pos++ = *BPP++;
}

/*
 * LoadIndex - load the position index obtained from SaveIndex
 *
 * Parameters:
 *
 *      off     piece type (BPBOARD, WPBOARD, BKBOARD, or WKBOARD)
 */
LoadIndex(off)
        register long off;
{
        register WHERE_T *BPP = BoardPos[off];
        register WHERE_T *pos = BPsave;
        register long k = 12;

        BPK[off] = BPKsave;
        while (k--)
                *BPP++ = *pos++;
}

/*
 * DBrevindex - compute a reverse index for a set of pieces
 *
 * Parameters:
 *
 *      pos     array giving position numbers (in descending order)
 *      k       number of pieces
 *
 * Example:
 *
 *      Suppose black checkers are on squares 1, 3 and 4.  The enumeration
 *      order proceeds as follows: (0,1,2) (0,1,3) (0,2,3) (1,2,3) (0,1,4)
 *      (0,2,4) (1,2,4) (0,3,4) (1,3,4).  Thus the reverse index for the black
 *      checker configuration is 8.
 *
 *      DBrevindex computes this reverse index as follows:
 *
 *              pos     4 3 1
 *              k       3
 *
 *              | 4 | = 4   | 3 | = 3   | 1 | = 1    4 + 3 + 1 = 8.
 *              | 3 |       | 2 |       | 1 |
 */
DBrevindex(pos, k)
        register long *pos, k;
{
        register long offset = 0;

        while (k > 0)
                offset += Choose(*pos++, k--);
        return(offset);
}

/*
 * DBindex - compute an index for a set of pieces 
 *
 * Parameters:
 *
 *      pos     array giving position numbers
 *      k       number of pieces
 *
 * This function is similar to DBrevindex except it is used for computing
 * the index of the white checkers.
 */
DBindex(pos, k)
        register long *pos, k;
{
        register long offset = 0;

        pos = &pos[k - 1];
        while (k > 0)
                offset += Choose(*pos--, k--);
        return(offset);
}

