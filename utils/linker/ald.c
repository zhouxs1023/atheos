#include	<stdio.h>
#include	<ctype.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<string.h>
#include	<malloc.h>
#include	<sys/errno.h>

#include "../include/atheos/aelf.h"
#include "../include/exec/types.h"

#include	"../include/macros.h"

/*#include	"../include/dos/altexe.h" */

#include	"../include/atheos/altexec.h"
#include	"link.h"

#define	PAGE_SIZE 	4096
#define	PAGE_SHIFT	12

char*	g_pzOutName = "a.out";
char*	g_pzDefName = NULL;
char*	g_pzObjName = NULL;
char*	g_apzDllNames[256];
int		g_nDllCount 	= 0;

BOOL		g_bExportAll	=	FALSE;
BOOL		g_bPrintInfo	=	FALSE;
int			g_nExeVersion	=	5;
uint32	g_nVirtualAddress = 1024 * 1024;

LnkImage_s* g_apsLibs[256];


/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/

int	sys_filelength( const char* pzPath )
{
	struct	stat	sStat;
	if ( stat( pzPath, &sStat ) != -1 ) {
		return( sStat.st_size );
	} else {
		return( -1 );
	}
}

/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/

int LocateFile( char* found, const char *file, const char* p, int nBufSize )
{
  memset( found, 0, nBufSize );

  if (strpbrk (file, "/") != 0)
  {
    strcpy(found, file);
    return( 0 );
  }
  else
  {
    const char *test_dir = p;

    do {
      const char *dp;

      dp = strchr( test_dir, ':' );

      if (dp == (char *)0) {
				dp = test_dir + strlen(test_dir);
			}


      if (dp == test_dir)
			{
				strcpy( found, file );
			}
      else
      {
				strncpy( found, test_dir, dp - test_dir );

				if ( '/' != found[dp - test_dir] ) {
					found[dp - test_dir] = '/';
					strcpy( found + (dp - test_dir) + 1, file );
				} else {
					strcpy( found + (dp - test_dir), file );
				}
      }

      if ( __file_exists( found ) ) {
				return( 0 );
			}

      if ( *dp == 0 ) {
				break;
			}

      test_dir = dp + 1;

    } while (*test_dir != 0);
  }
  return( -1 );
}

/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/

int	ApplyExpDefs( LnkImage_s	*psImage )
{
	int							lError = 0;
	int							i, j;
	BOOL						bStartFound = FALSE;
	BOOL						bInitFound = FALSE;
	BOOL						bFiniFound = FALSE;

	for ( j = 0 ; j < psImage->nSymCount ; ++j )
	{
		LnkSymbol_s* psSymbol = &psImage->pasSymbols[j];

		if ( psSymbol->nSecNum >= 0 && strcmp( psSymbol->zName, "___init" ) == 0 )
		{
			if ( bStartFound ) {
				printf( "WARNING : multiple definitions of __init\n" );
				psSymbol->nFlags |= SYMF_EXPORTED;
			} else {
				psSymbol->nFlags |= SYMF_EXPORTED;
				bStartFound = TRUE;
				psImage->psEntrySymbol = psSymbol;
			}
		}

		if ( psSymbol->nSecNum >= 0 && strcmp( psSymbol->zName, "__dll_init" ) == 0 )
		{
			if ( bInitFound ) {
				printf( "WARNING : multiple definitions of __dll_init\n" );
			} else {
				bInitFound = TRUE;
				psImage->psInitFncSymbol = psSymbol;
			}
		}
		
		if ( psSymbol->nSecNum >= 0 && strcmp( psSymbol->zName, "__dll_fini" ) == 0 )
		{
			if ( bFiniFound ) {
				printf( "WARNING : multiple definitions of __dll_fini\n" );
			} else {
				bFiniFound = TRUE;
				psImage->psTermFncSymbol = psSymbol;
			}
		}
		
	}

	if ( strstr( g_pzOutName, ".so" ) == NULL && strstr( g_pzOutName, ".dll" ) &&
			 FALSE == bStartFound )
	{
		printf( "WARNING : could not find symbol _init!\n" );
	}

	if ( g_bPrintInfo ) {
		printf( "%d symbols exported\n", psImage->nExportCount );
	}

	if ( FALSE == g_bExportAll )
	{
		for ( i = 0 ; i < psImage->nExportCount ; ++i )
		{
			for ( j = 0 ; j < psImage->nSymCount ; ++j )
			{
				LnkSymbol_s* psSymbol = &psImage->pasSymbols[j];

				if ( strcmp( psSymbol->zName, psImage->apzExportTable[i] ) == 0 )
				{
					psSymbol->nFlags |= SYMF_EXPORTED;
					break;
				}
			}
			if ( j == psImage->nSymCount )
			{
				printf( "ERROR : %s defined in '%s', but not found in '%s'\n",
								psImage->apzExportTable[i], g_pzDefName, g_pzObjName );
				lError = -1;
			}
		}
	}
	else	/* Export all symbols */
	{
		int	j;
		int	nNumAux = 0;

 		for ( j = 0 ; j < psImage->nSymCount ; ++j )
 		{
 			LnkSymbol_s	*psSymbol = &psImage->pasSymbols[j];

			if ( nNumAux == 0 )
			{
				nNumAux = psSymbol->nNumAux;

				if ( psSymbol->nSecNum >= 0 ) {
	 				psSymbol->nFlags |= SYMF_EXPORTED;
				}
			}
			else
			{
				nNumAux--;
			}
		}
	}
	return( lError );
}

/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/

int	FindLibSymbol( LnkImage_s	*psImage, const char* pzName, LnkReloc_s* psReloc )
{
	int	i,j;

	if ( strcmp( pzName, "___init" ) == 0 || strcmp( pzName, "__init" ) == 0 || strcmp( pzName, "_init" ) == 0 ) {
		return( -1 );
	}

	for ( j = 0 ; j < psImage->nLibCount ; ++j )
	{
		for ( i = 0 ; i < g_apsLibs[ j ]->nSymCount ; ++i )
		{
			LnkSymbol_s* psSymbol = &g_apsLibs[ j ]->pasSymbols[i];

			if ( psSymbol->nSecNum >= 0 && strcmp( psSymbol->zName, pzName ) == 0 )
			{
				return( j );
			}
		}
	}
	return( -1 );
}

/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/

int	ResolveExternals( LnkImage_s* psImage )
{
	int							nUndefinedSymbols = 0;
	int							i,j;

	for ( i = 0 ; i < psImage->nSectionCount ; i++ )
	{
		LnkSection_s*	psSection	=	&psImage->pasSections[i];

		if ( g_bPrintInfo ) {
			printf( "Section %d has %d reloc entries\n", i, psSection->nRelocCount );
		}

		for ( j = 0 ; j < psSection->nRelocCount ; j++ )
		{
			LnkReloc_s*		psReloc		=	&psSection->pasRelocs[ j ];
			LnkSymbol_s*	psSymbol	=	&psImage->pasSymbols[ psReloc->nSymIndex ];

			psReloc->psSymbol = psSymbol;

			if ( psSymbol->nClass == C_NULL ) {
				printf( "WARNING : Reference to class 'NULL' symbol\n" );
			}

			if ( psSymbol->nSecNum < 0 && (psSymbol->nFlags & SYMF_RESOLVED) == 0 )
			{
				psSymbol->nSecNum = FindLibSymbol( psImage, psSymbol->zName, psReloc );

				if ( psSymbol->nSecNum >= 0 )
				{
/*					printf( "Found symbol <%s> in lib <%s>\n", psSymbol->zName, g_apsLibs[ psSymbol->nSecNum ]->zName ); */
					psSymbol->nSecNum = (-psSymbol->nSecNum) - 1;
					psSymbol->nFlags |= SYMF_RESOLVED;
				}
				else
				{
					printf( "Error : Undefined symbol <%s>\n", psSymbol->zName );
					psSymbol	=	NULL;

					nUndefinedSymbols++;
				}
			}
		}
	}
	if ( nUndefinedSymbols == 0 ) {
		return( 0 );
	} else {
		printf( "ERROR : %ld undefined symbols\n", nUndefinedSymbols );
		return( -1 );
	}
}

/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/

int	RelocImage( LnkImage_s	*psImage )
{
	int							i,j;

	if ( g_bPrintInfo ) {
		printf( "Make pointers relative to symbols\n" );
	}

	for ( i = 0 ; i < psImage->nSectionCount ; i++ )
	{
		LnkSection_s*	psSection	=	&psImage->pasSections[i];

		for ( j = 0 ; j < psSection->nRelocCount ; j++ )
		{
			uint32*				Target;
			LnkReloc_s*		psReloc			=	&psSection->pasRelocs[ j ];
			LnkSymbol_s*	psSymbol		=	&psImage->pasSymbols[ psReloc->nSymIndex ];

			if ( psReloc->nSymIndex < 0 || psReloc->nSymIndex >= psImage->nSymCount ) {
				printf( "Error : Reloc entry %d reference non-existing symbol %d\n", j, psReloc->nSymIndex );
				return( -1 );
			}

			if ( psSymbol->nClass == C_NULL ) {
				printf( "WARNING : Reference to class 'NULL' symbol\n" );
			}

			psReloc->psSymbol	= psSymbol;

			if ( psReloc->nVarAddr < 0 || psReloc->nVarAddr > psSection->nSize - 4 )
			{
				printf( "Error : Reloc #%d reference address %d in a %d size section\n",
								j, psReloc->nVarAddr, psSection->nSize );
				return( -1 );
			}

			Target = (uint32*) &psSection->pData[ psReloc->nVarAddr ];

				/*** s_wSecNum < 0 means external, s_wSecNum > nSectionCount means raw data symbol	***/

			if ( psSymbol->nSecNum >= 0 && psSymbol->nSecNum < psImage->nSectionCount )
			{
				LnkSection_s*	psSymSection	=	&psImage->pasSections[ psSymbol->nSecNum ];

				*Target -= psSymSection->nVirAddr;

				switch( psReloc->nType )
				{
					case R_PCRLONG:
						Target[0] += psReloc->nVarAddr - psSymbol->nAddress + psSection->nVirAddr;
						psSymbol->nFlags |= SYMF_REFERENCED;

						if ( -4 != Target[0] ) {
							printf( "Warning : Value of IP-relative pointer = %d\n", Target[0] );
						}
						break;
					case R_DIR32:
						psSymbol->nFlags |= SYMF_REFERENCED;
						Target[0] -= psSymbol->nAddress;
						break;
					default:
						printf( "ERROR : UNKNOWN RELOCATION TYPE (%ld)\n", psReloc->nType );
						printf( "Symbol <%s> from section %ld\n", psSymbol->zName, psSymbol->nSecNum );
						printf( "Reloc #%ld at address %lx in section %ld\n", j, psReloc->nVarAddr, i );
						printf( "Target = %lx\n", Target[0] );
						break;
				}
			}
			else
			{
				switch( psReloc->nType )
				{
					case R_PCRLONG:
						Target[0] += psReloc->nVarAddr + psSection->nVirAddr;

						if ( -4 != Target[0] ) {
							printf( "Warning : Value of ext IP-relative pointer = %d\n", Target[0] );
						}

						psSymbol->nFlags |= SYMF_REFERENCED;
						break;						
					case R_DIR32:
						psSymbol->nFlags |= SYMF_REFERENCED;
/*
	 					if ( 0 != Target[0] ) {
							printf( "Warning : Value of ext absolute pointer = %d\n", Target[0] );
						}
*/						
						break;
					default:
						printf( "ERROR : UNKNOWN RELOCATION TYPE (%ld)\n", psReloc->nType );
						printf( "Symbol <%s> from section %ld\n", psSymbol->zName, psSymbol->nSecNum );
						printf( "Reloc #%ld at address %lx in section %ld\n", j, psReloc->nVarAddr, i );
						printf( "Target = %lx\n", Target[0] );
						break;
				}
			}
			psReloc->nValue = Target[0];
		}
	}
	return( 0 );
}







int	new_reloc_image( LnkImage_s	*psImage )
{
	int							i,j;

	if ( g_bPrintInfo ) {
		printf( "Make pointers relative to symbols\n" );
	}

	for ( i = 0 ; i < psImage->nSectionCount ; i++ )
	{
		LnkSection_s*	psSection	=	&psImage->pasSections[i];

		for ( j = 0 ; j < psSection->nRelocCount ; j++ )
		{
			uint32*				Target;
			LnkReloc_s*		psReloc			=	&psSection->pasRelocs[ j ];
			LnkSymbol_s*	psSymbol		=	&psImage->pasSymbols[ psReloc->nSymIndex ];

			if ( psReloc->nSymIndex < 0 || psReloc->nSymIndex >= psImage->nSymCount ) {
				printf( "Error : Reloc entry %d reference non-existing symbol %d\n", j, psReloc->nSymIndex );
				return( -1 );
			}

			if ( psSymbol->nClass == C_NULL ) {
				printf( "WARNING : Reference to class 'NULL' symbol\n" );
			}

			psReloc->psSymbol	= psSymbol;

			if ( psReloc->nVarAddr < 0 || psReloc->nVarAddr > psSection->nSize - 4 )
			{
				printf( "Error : Reloc #%d reference address %d in a %d size section\n",
								j, psReloc->nVarAddr, psSection->nSize );
				return( -1 );
			}

			Target = (uint32*) &psSection->pData[ psReloc->nVarAddr ];

				/*** s_wSecNum < 0 means external, s_wSecNum > nSectionCount means raw data symbol	***/

			if ( psSymbol->nSecNum >= 0 && psSymbol->nSecNum < psImage->nSectionCount )
			{
				LnkSection_s*	psSymSection	=	&psImage->pasSections[ psSymbol->nSecNum ];


				switch( psReloc->nType )
				{
					case R_PCRLONG:
						Target[0] -= psSection->nPhysAddr - psSection->nVirAddr;
						Target[0] += psSymSection->nPhysAddr - psSymSection->nVirAddr;
						
						
						psSymbol->nFlags |= SYMF_REFERENCED;
						break;
					case R_DIR32:
						Target[0] -= psSymSection->nVirAddr;
						Target[0] += psSymSection->nPhysAddr;
						
						psSymbol->nFlags |= SYMF_REFERENCED;
						break;
					default:
						printf( "ERROR : UNKNOWN RELOCATION TYPE (%ld)\n", psReloc->nType );
						printf( "Symbol <%s> from section %ld\n", psSymbol->zName, psSymbol->nSecNum );
						printf( "Reloc #%ld at address %lx in section %ld\n", j, psReloc->nVarAddr, i );
						printf( "Target = %lx\n", Target[0] );
						break;
				}
			}
			else
			{
				switch( psReloc->nType )
				{
					case R_PCRLONG:
						Target[0] += psReloc->nVarAddr + psSection->nVirAddr;

						if ( -4 != Target[0] ) {
							printf( "Warning : Value of ext IP-relative pointer = %d\n", Target[0] );
						}

						psSymbol->nFlags |= SYMF_REFERENCED;
						break;						
					case R_DIR32:
						psSymbol->nFlags |= SYMF_REFERENCED;
/*
	 					if ( 0 != Target[0] ) {
							printf( "Warning : Value of ext absolute pointer = %d\n", Target[0] );
						}
*/						
						break;
					default:
						printf( "ERROR : UNKNOWN RELOCATION TYPE (%ld)\n", psReloc->nType );
						printf( "Symbol <%s> from section %ld\n", psSymbol->zName, psSymbol->nSecNum );
						printf( "Reloc #%ld at address %lx in section %ld\n", j, psReloc->nVarAddr, i );
						printf( "Target = %lx\n", Target[0] );
						break;
				}
			}
			psReloc->nValue = Target[0];
		}
	}
	return( 0 );
}











/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/

static BOOL IsRelocInPage( uint32 nPageAddr, uint32 nPtrOffset )
{
	return( nPtrOffset + 4UL > nPageAddr && nPtrOffset < nPageAddr + PAGE_SIZE );
}

/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/

static int BuildSectionRelocMaps( LnkSection_s*	psSection )
{
	uint32			nPrevAddr = 0;
	int					nError = -ENOMEM;


	if ( psSection->nRelocCount == 0 ) {
		return( 0 );
	}

	{
		int32	i;
		int		j;

		int		nAbsRelocs = 0;
		int		nRelRelocs = 0;
		int		nPageCount = (psSection->nSize + PAGE_SIZE - 1) >> PAGE_SHIFT;

		psSection->panPageRelocOffsets = malloc( nPageCount * sizeof(int32) );

		if ( NULL != psSection->panPageRelocOffsets )
		{
			psSection->panPageRelocCounts = malloc( nPageCount * sizeof(int16) );

			if ( NULL != psSection->panPageRelocCounts )
			{
				int	nPrevReloc = 0;

				memset( psSection->panPageRelocOffsets, -1, nPageCount * sizeof(int32) );
				memset( psSection->panPageRelocCounts, 0, nPageCount * sizeof(int16) );

				for ( i = 0 ; i < nPageCount ; ++i )
				{
					for ( j = nPrevReloc ; j < psSection->nRelocCount ; ++j )
					{
						if ( IsRelocInPage( i * PAGE_SIZE, psSection->pasRelocs[j].nVarAddr ) )
						{
							psSection->panPageRelocCounts[ i ]++;
							if ( -1 == psSection->panPageRelocOffsets[ i ] ) {
								psSection->panPageRelocOffsets[ i ] = j;
							}
						}
						else
						{
							if ( -1 != psSection->panPageRelocOffsets[ i ] ) {
								break;
							}
						}
					}

					if ( -1 != psSection->panPageRelocOffsets[ i ] ) {
						nPrevReloc = max( j - 2, 0 );
					}
				}
				return( 0 );
			}
			else
			{
				nError = -ENOMEM;
			}
		}
		else
		{
			nError = -ENOMEM;
		}
	}
	return( nError );
}

/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/

int	FixupImage( LnkImage_s* psImage )
{
	if ( g_bPrintInfo ) {
		printf( "Resolve externals\n" );
	}
	if ( ResolveExternals( psImage ) == 0 )
	{
		if ( g_bPrintInfo ) {
			printf( "Apply export definitions\n" );
		}
		if ( ApplyExpDefs( psImage ) == 0 )
		{
			int	nError;
			
			if ( g_bPrintInfo ) {
				printf( "Reloc image\n" );
			}

			if ( 4 == g_nExeVersion ) {
				nError = RelocImage( psImage );
			} else {
				nError = new_reloc_image( psImage );
			}
			
			if ( nError == 0 ) {
				return( 0 );
			} else {
				printf( "Error : Failed to reloc image\n" 	);
			}
		}
	}
	return( -1 );
}

/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/

int	CmpRelocs( const void* p1, const void* p2 )
{
	return( ((ExeReloc_s*)p1)->re_nVarAddr - ((ExeReloc_s*)p2)->re_nVarAddr );
}

/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/

void BeginChunk( int hFile, int32 lChnkID, int32 lSize )
{
	write( hFile, &lChnkID, 4 );
	write( hFile, &lSize, 4 );
}

void	write_aelf( LnkImage_s* psImage )
{
	FILE*										hFile;
	AElf32_ElfHdr_s					sElfHdr;
	AElf32_ProgramHeader_s	sProgHdr;
	int											nSecCount;
	int											nFirstImageSec;
	AElf32_SectionHeader_s*	pasSections;
	int											i;
	int											nStrTabSize 	= 1;
	int											nSymbolCount	= 0;
	int											nFileOffset;
	
	int											nImageNameOffset;
	int											nStrTabName;
	int											nSymTabName;
	int											nLibTabName;
	int											nRelocTabName;

	char*	pzStrTabSecName			=	".dynstr";
	char*	pzSymTabSecName 		=	".dynsym";
	char*	pzLibTabSecName 		=	".libtab";
	char*	pzRelMapSectionName	=	".relocmap";
	
	unlink( g_pzOutName );

	hFile = fopen( g_pzOutName, "w" );

	if ( NULL == hFile ) {
		printf( "error : unable to create output file '%s' : %s\n", g_pzOutName, strerror(errno) );
	}
	
	nSecCount = 3 + psImage->nSectionCount * 3;	// string_table + sym_table + lib_table + relocs + reloc_maps + segments

	nFirstImageSec = 3;
	
	pasSections = malloc( sizeof( AElf32_SectionHeader_s ) * nSecCount );

	for ( i = 0 ; i < psImage->nSymCount ; ++i )
	{
		LnkSymbol_s*	psSymbol = &psImage->pasSymbols[i];
		
		if ( psSymbol->nFlags & (SYMF_EXPORTED | SYMF_REFERENCED | SYMF_RESOLVED) )	{
			psSymbol->nIndex = nSymbolCount;
			nSymbolCount++;
		}
	}

	for ( i = 0 ; i < psImage->nSectionCount ; ++i )
	{
		LnkSection_s*		psSection	=	&psImage->pasSections[i];
		int j;
	
		qsort( psSection->pasRelocs, psSection->nRelocCount, sizeof(LnkReloc_s), CmpRelocs );
		for ( j = 0, psSection->nUsedRelocCount = 0 ; j < psSection->nRelocCount ; ++j )
		{
			LnkReloc_s	*psReloc =	&psSection->pasRelocs[j];

			if ( (psReloc->nFlags & RELF_DEBUG) == 0 ) {
				psSection->nUsedRelocCount++;
			}
		}
		BuildSectionRelocMaps( psSection );
	}
	
		// String table
	
	pasSections[ 0 ].sh_nName 			= 0;
	pasSections[ 0 ].sh_nType 			= SHT_STRTAB;
	pasSections[ 0 ].sh_nFlags	 		= SHF_ALLOC;
	pasSections[ 0 ].sh_nAddress 		= 0;
	pasSections[ 0 ].sh_nOffset 		= 0;
	pasSections[ 0 ].sh_nSize 			= 0;	// initialized later
	pasSections[ 0 ].sh_nLink 			= 0;
	pasSections[ 0 ].sh_nInfo 			= 0;
	pasSections[ 0 ].sh_nAddrAlign	= 4;
	pasSections[ 0 ].sh_nEntrySize	= 0;

		// Symbol table
	
	pasSections[ 1 ].sh_nName 			= 0;
	pasSections[ 1 ].sh_nType 			= SHT_DYNSYM;
	pasSections[ 1 ].sh_nFlags	 		= SHF_ALLOC;
	pasSections[ 1 ].sh_nAddress 		= 0;
	pasSections[ 1 ].sh_nOffset 		= 0;
	pasSections[ 1 ].sh_nSize 			= nSymbolCount * sizeof( AElf32_Symbol_s );
	pasSections[ 1 ].sh_nLink 			= 1;	// String table
	pasSections[ 1 ].sh_nInfo 			= 0;	// Should be one greater than the index of the last local symbol's name
	pasSections[ 1 ].sh_nAddrAlign	= 4;
	pasSections[ 1 ].sh_nEntrySize	= sizeof( AElf32_Symbol_s );

		// library table
	
	pasSections[ 2 ].sh_nName 			= 0;
	pasSections[ 2 ].sh_nType 			= SHT_LIBTAB;
	pasSections[ 2 ].sh_nFlags	 		= SHF_ALLOC;
	pasSections[ 2 ].sh_nAddress 		= 0;
	pasSections[ 2 ].sh_nOffset 		= 0;
	pasSections[ 2 ].sh_nSize 			= psImage->nLibCount * AE_LIBNAME_LENGTH;
	pasSections[ 2 ].sh_nLink 			= 1;	// String table
	pasSections[ 2 ].sh_nInfo 			= 0;
	pasSections[ 2 ].sh_nAddrAlign	= 4;
	pasSections[ 2 ].sh_nEntrySize	= AE_LIBNAME_LENGTH;

		// reloc tables
	for ( i = 0 ; i < psImage->nSectionCount ; ++i )
	{
		LnkSection_s*		psSection	=	&psImage->pasSections[i];
		
		pasSections[ nFirstImageSec + i ].sh_nName 			= 0;
		pasSections[ nFirstImageSec + i ].sh_nType 			= SHT_RELA;
		pasSections[ nFirstImageSec + i ].sh_nFlags	 		= SHF_ALLOC;
		pasSections[ nFirstImageSec + i ].sh_nAddress 	= 0;
		pasSections[ nFirstImageSec + i ].sh_nOffset 		= 0;
		pasSections[ nFirstImageSec + i ].sh_nSize 			= psSection->nUsedRelocCount * sizeof( AElf32_Reloc_s );
		pasSections[ nFirstImageSec + i ].sh_nLink 			= 1;	// String table
		pasSections[ nFirstImageSec + i ].sh_nInfo 			= psImage->nSectionCount * 2 + nFirstImageSec + i + 1;
		pasSections[ nFirstImageSec + i ].sh_nAddrAlign = 4;
		pasSections[ nFirstImageSec + i ].sh_nEntrySize = sizeof( AElf32_Reloc_s );
	}
	
		// reloc table maps
	for ( i = 0 ; i < psImage->nSectionCount ; ++i )
	{
		int 						nOffset = psImage->nSectionCount + nFirstImageSec;
		LnkSection_s*		psSection	=	&psImage->pasSections[i];
		int							nPageCount = (psSection->nSize + PAGE_SIZE - 1) >> PAGE_SHIFT;
		
		pasSections[ nOffset + i ].sh_nName 			= 0;
		pasSections[ nOffset + i ].sh_nType 			= SHT_RELOC_MAP;
		pasSections[ nOffset + i ].sh_nFlags	 		= SHF_ALLOC;
		pasSections[ nOffset + i ].sh_nAddress 		= 0;
		pasSections[ nOffset + i ].sh_nOffset 		= 0;
		pasSections[ nOffset + i ].sh_nSize 			= (psSection->nRelocCount > 0 ) ? nPageCount * 6 : 0;
		pasSections[ nOffset + i ].sh_nLink 			= psImage->nSectionCount + nFirstImageSec + i + 1;
		pasSections[ nOffset + i ].sh_nInfo 			= 0;
		pasSections[ nOffset + i ].sh_nAddrAlign 	= 4;
		pasSections[ nOffset + i ].sh_nEntrySize 	= 6;
	}
	
		// segment data
	for ( i = 0 ; i < psImage->nSectionCount ; ++i )
	{
		int nOffset = psImage->nSectionCount * 2 + nFirstImageSec;
		LnkSection_s*		psSection	=	&psImage->pasSections[i];
		
		pasSections[ nOffset + i ].sh_nName 			= 0;
		pasSections[ nOffset + i ].sh_nType 			= (psSection->nFlags & STYP_BSS) ? SHT_NOBITS : SHT_PROGBITS;
		pasSections[ nOffset + i ].sh_nFlags	 		= SHF_ALLOC | ((psSection->nFlags & STYP_TEXT) ? SHF_EXECINST : SHF_WRITE);
		pasSections[ nOffset + i ].sh_nAddress 		= psSection->nPhysAddr;
		pasSections[ nOffset + i ].sh_nOffset 		= 0;
		pasSections[ nOffset + i ].sh_nSize 			= psSection->nSize;
		pasSections[ nOffset + i ].sh_nLink 			= SHN_UNDEF;
		pasSections[ nOffset + i ].sh_nInfo 			= 0;
		pasSections[ nOffset + i ].sh_nAddrAlign 	= PAGE_SIZE;
		pasSections[ nOffset + i ].sh_nEntrySize 	= 0;
	}


	pasSections[ 0 ].sh_nName	= nStrTabSize;
	nStrTabSize += strlen( pzStrTabSecName ) + 1;
	
	pasSections[ 1 ].sh_nName	= nStrTabSize;
	nStrTabSize += strlen( pzSymTabSecName ) + 1;
	
	pasSections[ 2 ].sh_nName	= nStrTabSize;
	nStrTabSize += strlen( pzLibTabSecName ) + 1;

		// program segment, and reloc table section names
	for ( i = 0 ; i < psImage->nSectionCount ; ++i )
	{
		LnkSection_s*		psSection	=	&psImage->pasSections[i];

		pasSections[ nFirstImageSec + i ].sh_nName = nStrTabSize;	// reloc table
		nStrTabSize += 4; // ".rel"
		pasSections[ nFirstImageSec + psImage->nSectionCount * 2 + i ].sh_nName = nStrTabSize;	// program segment
		nStrTabSize += strlen( "psSection->zName" ) + 1; // ".rel" + "SectionName"
	}

		// reloc map section names
	for ( i = 0 ; i < psImage->nSectionCount ; ++i )
	{
		pasSections[ nFirstImageSec + psImage->nSectionCount + i ].sh_nName = nStrTabSize;	// reloc maps
	}
	nStrTabSize += strlen( pzRelMapSectionName ) + 1;


		// Symbol names
	for ( i = 0 ; i < psImage->nSymCount ; ++i )
	{
		LnkSymbol_s*	psSymbol = &psImage->pasSymbols[i];
		
		if ( (psSymbol->nFlags & SYMF_EXPORTED) || (psSymbol->nFlags & SYMF_RESOLVED) )
		{
			psSymbol->nNameOffset = nStrTabSize;
			nStrTabSize += strlen( psSymbol->zName ) + 1;
		}
		else
		{
			psSymbol->nNameOffset = 0;
		}
	}
	pasSections[ 0 ].sh_nSize	= nStrTabSize;
	
		// Calculate file offsets
	
	nFileOffset = sizeof(AElf32_ElfHdr_s) + sizeof(AElf32_ProgramHeader_s) + nSecCount * sizeof(AElf32_SectionHeader_s);
	
	for ( i = 0 ; i < nSecCount ; ++i )
	{
		pasSections[i].sh_nOffset = nFileOffset;

		if ( SHT_NOBITS != pasSections[i].sh_nType ) {
			nFileOffset += pasSections[i].sh_nSize;
		}
	}
	
	memset( sElfHdr.e_anIdent, 0, AEI_NIDENT );

	sElfHdr.e_anIdent[ AEI_MAG0 ] 	= AELFMAG0;
	sElfHdr.e_anIdent[ AEI_MAG1 ] 	= AELFMAG1;
	sElfHdr.e_anIdent[ AEI_MAG2 ] 	= AELFMAG2;
	sElfHdr.e_anIdent[ AEI_MAG3 ] 	= AELFMAG3;
	sElfHdr.e_anIdent[ AEI_CLASS ] 	= AELFCLASS32;
	sElfHdr.e_anIdent[ AEI_DATA ]		= AELFDATALSB;
	
	sElfHdr.e_nType								= ET_EXEC;	// Propably wrong, since elf executables is not relocatable
	sElfHdr.e_nMachine						= EM_386;
	sElfHdr.e_nVersion						=	AEV_CURRENT;

	if ( NULL != psImage->psEntrySymbol ) {
		sElfHdr.e_nEntry = psImage->pasSections[ psImage->psEntrySymbol->nSecNum ].nPhysAddr + psImage->psEntrySymbol->nAddress;
	} else {
		sElfHdr.e_nEntry	= 0;
	}
	sElfHdr.e_nProgHdrOff					=	sizeof(AElf32_ElfHdr_s);
	sElfHdr.e_nSecHdrOff					= sizeof(AElf32_ElfHdr_s) + sizeof(AElf32_ProgramHeader_s);	// Offset to first section header
	sElfHdr.e_nFlags							= 0;
	sElfHdr.e_nElfHdrSize					= sizeof( AElf32_ElfHdr_s );
	sElfHdr.e_nProgHdrEntrySize		=	sizeof(AElf32_ProgramHeader_s);
	sElfHdr.e_nProgHdrCount				=	1;
	sElfHdr.e_nSecHdrSize					= sizeof( AElf32_SectionHeader_s );
	sElfHdr.e_nSecHdrCount				= nSecCount;
	sElfHdr.e_nSecNameStrTabIndex	= 1;

	sProgHdr.ph_nType = PHT_PROGRAM;
	
	if ( NULL != psImage->psInitFncSymbol ) {
		sProgHdr.ph_nInitFunc = psImage->pasSections[ psImage->psInitFncSymbol->nSecNum ].nPhysAddr;
		sProgHdr.ph_nInitFunc += psImage->psInitFncSymbol->nAddress;
	} else {
		sProgHdr.ph_nInitFunc	= 0;
	}
	
	if ( NULL != psImage->psTermFncSymbol ) {
		sProgHdr.ph_nTermFunc = psImage->pasSections[ psImage->psTermFncSymbol->nSecNum ].nPhysAddr;
		sProgHdr.ph_nTermFunc += psImage->psTermFncSymbol->nAddress;
	} else {
		sProgHdr.ph_nTermFunc	= 0;
	}
	
	fwrite( &sElfHdr, sizeof( sElfHdr ), 1, hFile );
	fwrite( &sProgHdr, sizeof( sProgHdr ), 1, hFile );

	for ( i = 0 ; i < nSecCount ; ++i )
	{
		fwrite( &pasSections[i], sizeof( AElf32_SectionHeader_s ), 1, hFile );
	}
		// Write string table

	fwrite( "\0", 1, 1, hFile ); // Write the NULL string

	fwrite( pzStrTabSecName, strlen( pzStrTabSecName ) + 1, 1, hFile );
	fwrite( pzSymTabSecName, strlen( pzSymTabSecName ) + 1, 1, hFile );
	fwrite( pzLibTabSecName, strlen( pzLibTabSecName ) + 1, 1, hFile );

		// program segment, and reloc table section names
	for ( i = 0 ; i < psImage->nSectionCount ; ++i )
	{
		LnkSection_s*		psSection	=	&psImage->pasSections[i];
		
		fwrite( ".rel", 4, 1, hFile );
		fwrite( "psSection->zName", strlen( "psSection->zName" ) + 1, 1, hFile );
	}
	
	fwrite( pzRelMapSectionName, strlen( pzRelMapSectionName ) + 1, 1, hFile );
	
	for ( i = 0 ; i < psImage->nSymCount ; ++i )
	{
		LnkSymbol_s		*psSymbol = &psImage->pasSymbols[i];

/* 			if ( (psSymbol->nFlags & SYMF_EXPORTED) || (psSymbol->nFlags & SYMF_RESOLVED) ) */
	
		if ( 0 != psSymbol->nNameOffset ) {
			fwrite( psSymbol->zName, strlen( psSymbol->zName ) + 1, 1, hFile );
		}
	}

		// Write the symbol table
	
	for ( i = 0 ; i < psImage->nSymCount ; ++i )
	{
		LnkSymbol_s*	psSymbol = &psImage->pasSymbols[i];

		if ( psSymbol->nFlags & (SYMF_EXPORTED | SYMF_REFERENCED) )
		{
			AElf32_Symbol_s	sSymbol;
			int							nBinding;

			if ( psSymbol->nFlags & SYMF_EXPORTED ) {
				nBinding = STB_EXPORTED;
			} else {
				nBinding = STB_LOCAL;
			}
			sSymbol.st_nName			=	psSymbol->nNameOffset;
			sSymbol.st_nValue			= psSymbol->nAddress;
			sSymbol.st_nInfo			= AELF32_ST_INFO( nBinding, STT_NOTYPE );
			sSymbol.st_nOther			=	0;
			sSymbol.st_nSecIndex	= psSymbol->nSecNum;
			
			fwrite( &sSymbol, sizeof( AElf32_Symbol_s ), 1, hFile );
		}
	}
		// write library table
	for ( i = 0 ; i < psImage->nLibCount ; ++i )
	{
		char	zLibName[ AE_LIBNAME_LENGTH ];

		memset( zLibName, 0, AE_LIBNAME_LENGTH );
		strncpy( zLibName, g_apsLibs[ i ]->zName, AE_LIBNAME_LENGTH );
		zLibName[ AE_LIBNAME_LENGTH - 1 ] = '\0';

		fwrite( zLibName, AE_LIBNAME_LENGTH, 1, hFile );
	}
	
		// Write relocation tables
	
	for ( i = 0 ; i < psImage->nSectionCount ; ++i )
	{
		LnkSection_s*		psSection	=	&psImage->pasSections[i];

//		int	nSize = sizeof( ExeReloc_s ) * psSection->nUsedRelocCount;

		if ( psSection->nUsedRelocCount > 0 )
		{
			int	j;

			for ( j = 0 ; j < psSection->nRelocCount ; ++j )
			{
				LnkReloc_s	*psReloc	=	&psSection->pasRelocs[j];

				if ( (psReloc->nFlags & RELF_DEBUG) == 0 )
				{
					AElf32_Reloc_s	sReloc;
					int							nType;

					switch( psReloc->nType )
					{
						case R_PCRLONG:
							nType = R_386_PC32;
							break;
						case R_DIR32:
							nType	=	R_386_32;
							break;
						default:
							printf( "error : write_aelf() found reloc whith unknown type %d\n", psReloc->nType );
							break;
					}

					sReloc.r_nOffset	= psReloc->nVarAddr;
					sReloc.r_nAddend	= psReloc->nValue;
					sReloc.r_nInfo		= AELF32_R_INFO( psReloc->psSymbol->nIndex, nType );

					fwrite( &sReloc, sizeof( AElf32_Reloc_s ), 1, hFile );
				}
			}
		}
	}

		// Write reloc maps
	for ( i = 0 ; i < psImage->nSectionCount ; ++i )
	{
		LnkSection_s*		psSection	=	&psImage->pasSections[i];
		int							nPageCount = (psSection->nSize + PAGE_SIZE - 1) >> PAGE_SHIFT;

		if ( 0 == nPageCount ) {
			printf( "Warning : Section %d is 0 pages large, and has %d relocs!\n", i, psSection->nRelocCount );
		}

		if ( 0 != psSection->nRelocCount )
		{
			fwrite( psSection->panPageRelocOffsets, nPageCount * 4, 1, hFile );
			fwrite( psSection->panPageRelocCounts, nPageCount * 2, 1, hFile );
		}
	}

		// Write image segment data
	for ( i = 0 ; i < psImage->nSectionCount ; ++i )
	{
		LnkSection_s*		psSection	=	&psImage->pasSections[i];
		
		if ( (psSection->nFlags & STYP_BSS ) == 0 )
		{
			fwrite( psSection->pData, psSection->nSize, 1, hFile );
		}
	}
	
	fclose( hFile );
}

/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/

void WriteImage( LnkImage_s *psImage )
{
	int	hFile;

	int32	lChunkID;
	int32	lChunkSize;

	int32	lHdrSize;
	int	i;

	lHdrSize = sizeof( ChunkHdr_s ) + sizeof( ExeImage_s );

	unlink( g_pzOutName );
	if ( (hFile = open( g_pzOutName, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, S_IRWXU )) != -1 )
	{
			/******	Write exec header	***************************************************************/

		ExeImage_s	sExeHdr;

		sExeHdr.im_nCodeSize = 0;
		sExeHdr.im_nDataSize = 0;

		for ( i = 0 ; i < psImage->nSectionCount ; ++i )
		{
			if ( psImage->pasSections[i].nFlags & STYP_TEXT )	{
				sExeHdr.im_nCodeSize	+= psImage->pasSections[i].nSize;
			} else {
				sExeHdr.im_nDataSize	+= psImage->pasSections[i].nSize;
			}
		}

		sExeHdr.im_nMagic				=	ALTEXE_MAGIC;
		sExeHdr.im_nVersion			=	g_nExeVersion;
		sExeHdr.im_nSecCount		=	psImage->nSectionCount; /* + psImage->im_nRawDataCount; */
		sExeHdr.im_nLibCount		= psImage->nLibCount;

		sExeHdr.im_nSymCount = 0;

		for ( i = 0 ; i < psImage->nSymCount ; ++i )
		{
			LnkSymbol_s*	psSymbol = &psImage->pasSymbols[i];

			if ( psSymbol->nFlags & (SYMF_EXPORTED | SYMF_REFERENCED) )	{
				sExeHdr.im_nSymCount++;
			}
		}


		for ( i = strlen( g_pzOutName ) - 1 ; i >= 0 ; --i )
		{
			if ( g_pzOutName[i] == '/' || g_pzOutName[i] == '\\' ) {
				i++;
				break;
			}
		}

		strcpy( sExeHdr.im_zName, &g_pzOutName[ ( i < 0 ) ? 0 : i ] );


		BeginChunk( hFile, CHUNK_EXEHEADER, lHdrSize );
		write( hFile, &sExeHdr, sizeof( ExeImage_s ) );

			/******	Write sections	***************************************************************/

		for ( i = 0 ; i < psImage->nSectionCount ; ++i )
		{
			ExeSection_s		sExeSec;
			LnkSection_s*		psSection	=	&psImage->pasSections[i];
			int							lSize = (psSection->nFlags & STYP_BSS ) ? 0 : psSection->nSize;
			int							j;

			lSize +=	sizeof( ExeSection_s );

				/***	Count the number of reloc entries that is actualy needed	***/

			for ( j = 0, psSection->nUsedRelocCount = 0 ; j < psSection->nRelocCount ; ++j )
			{
				LnkReloc_s	*psReloc =	&psSection->pasRelocs[j];

				if ( (psReloc->nFlags & RELF_DEBUG) == 0 ) {
					psSection->nUsedRelocCount++;
				}
			}

			if ( g_bPrintInfo ) {
				printf( "Section #%ld contain %ld referenced of total %ld relocs\n",
								i, psSection->nUsedRelocCount, psSection->nRelocCount );
			}

			sExeSec.se_nRelocCount	=	psSection->nUsedRelocCount;
			sExeSec.se_nSize				=	psSection->nSize;
			sExeSec.se_nFlags				=	psSection->nFlags;

			BeginChunk( hFile, CHUNK_SECTION, lSize + 8 );
			write( hFile, &sExeSec, sizeof( ExeSection_s ) );

			if ( g_bPrintInfo ) {
				printf( "Write section %ld, %ld\n",	sizeof( ExeSection_s ), psSection->nSize );
			}

			if ( (psSection->nFlags & STYP_BSS ) == 0 )
			{
				write( hFile, psSection->pData, psSection->nSize );
			}
		}

			/******	Write reloc tables	**************************************************************/

		{
			int							nIndex = 0;


				/*** Count referenced symbols	***/

			for ( i = 0 ; i < psImage->nSymCount ; ++i )
			{
				LnkSymbol_s*	psSymbol = &psImage->pasSymbols[i];

				if ( psSymbol->nFlags & (SYMF_EXPORTED | SYMF_REFERENCED | SYMF_RESOLVED) )	{
					psSymbol->nIndex = nIndex++;
				}
			}
		}

		for ( i = 0 ; i < psImage->nSectionCount ; ++i )
		{
			LnkSection_s*		psSection	=	&psImage->pasSections[i];

			int	lSize = sizeof( ExeReloc_s ) * psSection->nUsedRelocCount;

			BeginChunk( hFile, CHUNK_RELOCS, lSize + 8 );

			if ( lSize )
			{
				int	j;

				qsort( psSection->pasRelocs, psSection->nRelocCount, sizeof(LnkReloc_s), CmpRelocs );

				BuildSectionRelocMaps( psSection );

				for ( j = 0 ; j < psSection->nRelocCount ; ++j )
				{
					LnkReloc_s	*psReloc	=	&psSection->pasRelocs[j];

					if ( (psReloc->nFlags & RELF_DEBUG) == 0 )
					{
						ExeReloc_s	sReloc;

						sReloc.re_nVarAddr			= psReloc->nVarAddr;
						sReloc.re_nValue				= psReloc->nValue;
						sReloc.re_nSymIndex			=	psReloc->psSymbol->nIndex;
						sReloc.re_nType					=	psReloc->nType;

						write( hFile, &sReloc, sizeof( ExeReloc_s ) );
					}
				}
			}
		}

			/******	Write symbol table	*********************************************************************/

		{
			int							lNameOffset;
			int							lSize;
			int							lUsedSyms;
			int							j;

				/*** Count referenced symbols	***/

			for ( j = 0, lUsedSyms = 0 ; j < psImage->nSymCount ; ++j )
			{
				LnkSymbol_s*	psSymbol = &psImage->pasSymbols[j];

				if ( psSymbol->nFlags & (SYMF_EXPORTED | SYMF_REFERENCED) )	{
					lUsedSyms++;
				}
			}

			if ( g_bPrintInfo ) {
				printf( "Image contain %ld referenced of total %ld symbols\n",
								lUsedSyms, psImage->nSymCount );
			}

				/*** Find the name index for all inported, and exported symbols ***/

			for ( j = 0, lNameOffset = 0 ; j < psImage->nSymCount ; ++j )
			{
				LnkSymbol_s*	psSymbol = &psImage->pasSymbols[j];

				if ( (psSymbol->nFlags & SYMF_EXPORTED) || (psSymbol->nFlags & SYMF_RESOLVED) )
				{
					psSymbol->nNameOffset = lNameOffset;
					lNameOffset += strlen( psSymbol->zName ) + 1;
				}
				else
				{
					psSymbol->nNameOffset = -1;
				}
			}

			lSize = lUsedSyms * sizeof(ExeSymbol_s) + lNameOffset;

			BeginChunk( hFile, CHUNK_SYMBOLS, lSize + 8 );

				/*** Write the actual symbol table ***/

			for ( j = 0 ; j < psImage->nSymCount ; ++j )
			{
				LnkSymbol_s*	psSymbol = &psImage->pasSymbols[j];

				if ( psSymbol->nFlags & (SYMF_EXPORTED | SYMF_REFERENCED) )
				{
					ExeSymbol_s	sSymbol;

					sSymbol.s_nNameOffset	=	psSymbol->nNameOffset;
					sSymbol.s_nSecNum			= psSymbol->nSecNum;
					sSymbol.s_nType				= psSymbol->nType;
					sSymbol.s_nClass			= psSymbol->nClass;
					sSymbol.s_nNumAux			= psSymbol->nNumAux;
					sSymbol.s_nAddress		= psSymbol->nAddress;

					write( hFile, &sSymbol, sizeof( ExeSymbol_s ) );
				}
			}

				/*** Save names of exported symbols ***/

			for ( j = 0 ; j < psImage->nSymCount ; ++j )
			{
				LnkSymbol_s		*psSymbol = &psImage->pasSymbols[j];

/* 			if ( (psSymbol->nFlags & SYMF_EXPORTED) || (psSymbol->nFlags & SYMF_RESOLVED) ) */

				if ( -1 != psSymbol->nNameOffset ) {
					write( hFile, psSymbol->zName, strlen( psSymbol->zName ) + 1 );
				}
			}
		}

		BeginChunk( hFile, CHUNK_LIB_TABLE, psImage->nLibCount * AE_LIBNAME_LENGTH + 8 );

		for ( i = 0 ; i < psImage->nLibCount ; ++i )
		{
			char	zLibName[ AE_LIBNAME_LENGTH ];

			memset( zLibName, 0, AE_LIBNAME_LENGTH );
			strncpy( zLibName, g_apsLibs[ i ]->zName, AE_LIBNAME_LENGTH );
			zLibName[ AE_LIBNAME_LENGTH - 1 ] = '\0';

			write( hFile, zLibName, AE_LIBNAME_LENGTH );
		}

		for ( i = 0 ; i < psImage->nSectionCount ; ++i )
		{
			LnkSection_s*		psSection	=	&psImage->pasSections[i];
 			int							nPageCount = (psSection->nSize + PAGE_SIZE - 1) >> PAGE_SHIFT;

			if ( 0 == nPageCount ) {
				printf( "Warning : Section %d is 0 pages large, and has %d relocs!\n", i, psSection->nRelocCount );
			}

			if ( 0 != psSection->nRelocCount )
			{
				BeginChunk( hFile, CHUNK_RELOC_MAP, nPageCount * 6 + 8 );

				write( hFile, psSection->panPageRelocOffsets, nPageCount * 4 );
				write( hFile, psSection->panPageRelocCounts, nPageCount * 2 );
			}
			else
			{
				BeginChunk( hFile, CHUNK_RELOC_MAP, 8 );
			}
		}

		close( hFile );
	}
}

/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/

int	LoadLibs()
{
	int		i;
	char*	pzDllPath = getenv( "DLL_PATH" );



	if ( NULL == pzDllPath ) {
		pzDllPath = getenv( "LIBRARY_PATH" );
	}

	for ( i = 0 ; i < g_nDllCount ; ++i )
	{
		char*	zName = g_apzDllNames[i];
		FILE*	hFile;

/*		printf( "Load dll '%s'\n", zName ); */

		hFile = fopen( zName, "rb" );

		if ( NULL == hFile ) {
			char	zFullPath[512];
			if ( LocateFile( zFullPath, zName, pzDllPath, 512 ) == 0 ) {
				hFile = fopen( zFullPath, "rb" );
			}
		}

		if ( NULL != hFile )
		{
			g_apsLibs[i] = load_aelf_lib( hFile, g_apzDllNames[i] );

			if ( NULL == g_apsLibs[i] ) {
				printf( "load old dll\n" );
				fseek( hFile, 0, SEEK_SET );
				g_apsLibs[i] = LoadDllExports( hFile );
			}
			fclose( hFile );
		}
		else
		{
			printf( "ERROR : Failed to open dll '%s'\n", zName );

			printf( "DLL_PATH='%s'\n", (pzDllPath) ? pzDllPath : "not_defined" );
			return( -1 );
		}
		if ( g_apsLibs[i] == NULL )
		{
			printf( "ERROR : Invalid dll '%s'\n", zName );
			return( -1 );
		}
	}
	return( 0 );
}

/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/

int	LoadDef( LnkImage_s	*psImage )
{
	FILE	*hFile;

	psImage->nExportCount = 0;

	if ( g_pzDefName != NULL )
	{
		if ( hFile = fopen( g_pzDefName, "r" ) )
		{
			int	lExpSize	=	1024;

			if ( psImage->apzExportTable = malloc( lExpSize * sizeof( char* ) ) )
			{
				UBYTE		zLine[256];

				while ( fgets( zLine, 256, hFile ) != NULL )
				{
					int	i;
					int	lStart;
					int	lSize;

					/***	Ignore leading white spaces ***/

				 	for ( lStart = 0 ; isspace( zLine[ lStart ] ) && lStart < 256 ; ++lStart )
						/*** EMPTY ***/;

					if ( lStart == 256 )
					{
						printf( "ERROR : Line to long\n" );
						return( -1 );
					}

					if ( '#' == zLine[ lStart ] ) {
						continue;
					}

					if ( strncmp( &zLine[ lStart ], "@export_all", strlen( "@export_all" )  ) == 0 ) {
						g_bExportAll = TRUE;
						break;
					}

					/*** Count number of valid characters ***/

					for ( i = lStart, lSize = 0 ; i < 256 &&
									( isalnum( zLine[i] ) || (zLine[i] == '_') ||
										(zLine[i] == '@') || (zLine[i] == '.') ) ; ++i )
					{
						lSize++;
					}

					zLine[ lStart + lSize ] = 0;

					if ( lSize > 0 && i != 256 )
					{
						/***	Expand the export table if needed	***/

						if ( psImage->nExportCount == lExpSize - 1 )
						{
							char**	apzOldExp = psImage->apzExportTable;

							lExpSize += 512;

							if ( psImage->apzExportTable = malloc( lExpSize * sizeof( char* ) ) )
							{
								memcpy( psImage->apzExportTable, apzOldExp, (lExpSize - 512) * sizeof( char* ) );
								free( apzOldExp );
							}
							else
							{
								printf( "ERROR : Out of memory\n" );
								return( -1 );
							}
						}

						if ( psImage->apzExportTable[ psImage->nExportCount ] = malloc( lSize + 2 ) )
						{
							if ( zLine[ lStart ] == '@' )		/* Dont add the '_' to this symbol	*/
							{
								memcpy( psImage->apzExportTable[ psImage->nExportCount ],
												&zLine[ lStart + 1 ], lSize );
							}
							else
							{
								psImage->apzExportTable[ psImage->nExportCount ][0] = '_';
								memcpy( &psImage->apzExportTable[ psImage->nExportCount ][1],
												&zLine[ lStart ], lSize + 1 );
							}
/*							printf( "Added export sym '%s'\n", psImage->apzExportTable[ psImage->nExportCount ] ); */
							psImage->nExportCount++;
						}
						else
						{
							printf( "ERROR : Out of memory\n" );
							return( -1 );
						}
					}
				}
			}
			fclose( hFile );
		}
		else
		{
			printf( "ERROR : Unable to open %s\n", g_pzDefName );
		}
	}

	return( 0 );
}

/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/
char	zStr[] = "Usage: alnk app.obj [app.def] [.so] [-o file] [-i]";
void	PrintManual()
{
	printf( "(%p) - %s\n", zStr, zStr );
}

/******  ****************************************************
 *
 *	NAME
 *
 *	SYNOPSIS
 *
 *	FUNCTION
 *
 *	INPUTS
 *
 *	RESULTS
 *
 *	SEE ALSO
 *
 ****************************************************************************/

int main( int argc, char **argv )
{
	LnkImage_s	*	psImage;
	int					hDllFile;

	if ( argc > 1 )
	{
		if ( psImage = malloc( sizeof( LnkImage_s ) ) )
		{
			int	i;
			memset( psImage, 0, sizeof( LnkImage_s ) );

			for ( i = 1 ; i < argc ; ++i )
			{
				if ( argv[i][0] == '-' )
				{
					switch( argv[i][1] )
					{
						case 'o':
						{
							if ( i < argc - 1 )
							{
								g_pzOutName = argv[++i];
							}
							else
							{
								PrintManual();
								exit( 1 );
							}
							break;
						}
						case 'l':
						{
							char	zName[ 256 ];

							if ( strcmp( argv[i] + 2, "kernel.so" ) == 0 ) {
								zName[0] = '\0';
							} else {
								strcpy( zName, "lib" );
							}
							strcat( zName, argv[i] + 2 );

							g_apzDllNames[ g_nDllCount ] = malloc( strlen( zName ) + 1 );

							if ( NULL == g_apzDllNames[ g_nDllCount ] ) {
								printf( "Out of memory!\n" );
								exit( 1 );
							}

							strcpy( g_apzDllNames[ g_nDllCount ], zName );

							g_nDllCount++;

							break;
						}
						case 'i':
						{
							g_bPrintInfo = TRUE;
							break;
						}
						case 'a':
						{
							g_bExportAll = TRUE;
							break;
						}
					}
				}
				else
				{
					if ( strlen( argv[i] ) > 4 )
					{
						if ( stricmp( &argv[i][ strlen( argv[i] ) -4 ], ".def" ) == 0 )
						{
							g_pzDefName = argv[i];
						} else if ( stricmp( &argv[i][ strlen( argv[i] ) -4 ], ".obj" ) == 0 ) {
							g_pzObjName = argv[i];
						} else {
							g_pzObjName = argv[i];
						}
					}
				}
			}

			if ( strstr( g_pzOutName, "kernel.so" ) != NULL  ) {
				g_nVirtualAddress = 1024 * 1024;
			}
			
			if ( g_bPrintInfo ) {
				printf( "Load libraries\n" );
			}

			psImage = LoadCoffImage( g_pzObjName, g_nDllCount );

			if ( NULL != psImage )
			{
				if ( LoadLibs() == 0 )
				{
					if ( g_bPrintInfo ) {
						printf( "Load export definitions\n" );
					}
					if ( LoadDef( psImage ) == 0 )
					{
						if ( g_bPrintInfo ) {
							printf( "Load main image\n" );
						}

						if ( FixupImage( psImage ) == 0 )
						{
							if ( g_bPrintInfo ) {
								printf( "Save executable\n" );
							}
//							if ( strcmp( g_pzOutName, "alnk" ) != 0 ) {
//								printf( "writing aelf image\n" );
								write_aelf( psImage );
//							} else {
//								WriteImage( psImage );
//							}
						}
					}
				}
			}
		}
	}
	else
	{
		PrintManual();
		exit( 1 );
	}
	return( 0 );
}
