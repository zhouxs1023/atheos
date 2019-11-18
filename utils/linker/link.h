#include	<altos/coff.h>

#define	dbprintf	printf
typedef struct	LibEntry	LibEntry_s;




/********************** STORAGE CLASSES **********************/

/* This used to be defined as -1, but now n_sclass is unsigned.  */
#define C_EFCN		0xff	/* physical end of function	*/
#define C_NULL		0
#define C_AUTO		1	/* automatic variable		*/
#define C_EXT			2	/* external symbol		*/
#define C_STAT		3	/* static			*/
#define C_REG			4	/* register variable		*/
#define C_EXTDEF	5	/* external definition		*/
#define C_LABEL		6	/* label			*/
#define C_ULABEL	7	/* undefined label		*/
#define C_MOS			8	/* member of structure		*/
#define C_ARG			9	/* function argument		*/
#define C_STRTAG	10	/* structure tag		*/
#define C_MOU			11	/* member of union		*/
#define C_UNTAG		12	/* union tag			*/
#define C_TPDEF		13	/* type definition		*/
#define C_USTATIC	14	/* undefined static		*/
#define C_ENTAG		15	/* enumeration tag		*/
#define C_MOE			16	/* member of enumeration	*/
#define C_REGPARM	17	/* register parameter		*/
#define C_FIELD		18	/* bit field			*/
#define C_AUTOARG	19	/* auto argument		*/
#define C_LASTENT	20	/* dummy entry (end of block)	*/
#define C_BLOCK		100	/* ".bb" or ".eb"		*/
#define C_FCN			101	/* ".bf" or ".ef"		*/
#define C_EOS			102	/* end of structure		*/
#define C_FILE		103	/* file name			*/
#define C_LINE		104	/* line # reformatted as symbol table entry */
#define C_ALIAS	 	105	/* duplicate tag		*/
#define C_HIDDEN	106	/* ext symbol in dmert public lib */




struct	LibEntry
{
	LibEntry_s*	psNext;
	char				zName[256];
	char**			ppzSymbols;
};


typedef	struct
{
	char		zName[128];
	int			nNameOffset;	/* Only used while loading dll symbol tables	*/
	int			nSecNum;
	uint32	nAddress;
	int			nType;
	int			nClass;
	int			nNumAux;
/*	int			nSymTabIndex; */
	int			nIndex;
	int			nFlags;
}	LnkSymbol_s;

/*
typedef struct
{
	char					zName[32];
	int						nSymCount;
	LnkSymbol_s*	pasSymbols;
}	LnkSymTable_s;
*/

#define	SYMF_EXPORTED		0x0001		/* Exported symbol, keep the name */
#define	SYMF_REFERENCED	0x0002		/* Set if a reloc entry refere to this symbol	*/
#define	SYMF_RESOLVED		0x0004		/* Set for external symbols that has been found */

/*	Could be removed, normaly due to ip-relative relocation whitin same section	*/

#define	RELF_DEBUG	0x0001

typedef	struct
{
	uint32				nVarAddr;
	uint32				nValue;
	LnkSymbol_s	*	psSymbol;
	int						nSymIndex;
	int						nType;
	uint32				nFlags;
} LnkReloc_s;

typedef	struct
{
	char				zName[ 64 ];
	uint32			nSize;
	uint32			nVirAddr;						/* Virtual address */
	uint32			nPhysAddr;
	char*				pData;
	uint32			nFlags;
	int					nRelocCount;
	int					nUsedRelocCount;		/* Number of relocs, actualy in use	*/
	LnkReloc_s*	pasRelocs;
	uint32			nFilePos;						/* File position for data (COFF)		*/
	uint32			nRelocFilePos;			/* File position for reloc table (COFF)		*/
	uint32*			panPageRelocOffsets;
	uint16*			panPageRelocCounts;
}	LnkSection_s;


typedef	struct
{
	char			zFileName[128];
	char			zSymName[128];
	uint			nPos;
	uint			nSize;
	char*			pData;
}	LnkRawData_s;




typedef struct
{
	char						zName[ 64 ];
	int							nSectionCount;
	LnkSection_s*		pasSections;

	int							nSymCount;
	int							nLibCount;

	LnkSymbol_s*		pasSymbols;
	off_t						nSymTabFilePos;	/* File position for symbol table (COFF)		*/

	int							nExportCount;
	char**					apzExportTable;
	LnkRawData_s*		apsRawData[128];
	int							nRawDataCount;
	LnkSymbol_s*		psEntrySymbol;		// Symbol for entry point
	LnkSymbol_s*		psInitFncSymbol;	// Symbol for init function
	LnkSymbol_s*		psTermFncSymbol;	// Symbol for terminate function
} LnkImage_s;

extern uint32 g_nVirtualAddress;

LnkImage_s*	LoadCoffImage( const char* pzName, int nDllCount );
LnkImage_s* LoadDllExports( FILE* hFile );
LnkImage_s* load_aelf_lib( FILE* hFile, const char* pzImageName );
