#include	<stdio.h>
#include	<ctype.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<string.h>
#include	<malloc.h>
#include	<sys/errno.h>

#ifndef __GNUC__		/* To make altexe.h work	*/
#define	__GNUC__
#endif

#include "../include/exec/types.h"
#include	<atheos/altexec.h>
#include	"../include/macros.h"

#include	"link.h"

static int	ReadImageHeader( LnkImage_s* psImage, FILE* hFile )
{
	int					nError = 0;
	ChunkHdr_s	sChunkHdr;
	ExeImage_s	sExeHdr;

	nError = fread( &sChunkHdr, sizeof( ChunkHdr_s ), 1, hFile );

	if ( 1 == nError )
	{
		if ( sChunkHdr.ch_nType == CHUNK_EXEHEADER )
		{
			nError = fread( &sExeHdr, sizeof( ExeImage_s ), 1, hFile );

			if ( 1 == nError )
			{
				if ( ALTEXE_MAGIC == sExeHdr.im_nMagic && (3 == sExeHdr.im_nVersion || 4 == sExeHdr.im_nVersion) )
				{
					psImage->nSectionCount	= sExeHdr.im_nSecCount;
					psImage->nSymCount			= sExeHdr.im_nSymCount;
/*
					psImage->im_nTextSize		=	sExeHdr.im_nCodeSize;
					psImage->im_nDataSize		=	sExeHdr.im_nDataSize;
*/
					strcpy( psImage->zName, sExeHdr.im_zName );

					psImage->pasSymbols = calloc( sizeof( LnkSymbol_s ), psImage->nSymCount );
					return( 0 );
				}	else {
					printf( "Error : Dll has invalid magic number\n" );
					nError = -ENOEXEC;
				}
			}	else {
				printf( "Error : Could not read exec header\n" );
				nError = -EIO;
			}
		}	else {
			printf( "Error : Invalid exec header\n" );
			nError = -ENOEXEC;
		}
	} else {
		printf( "Error : Failed to load exec chunk header\n" );
		nError = -ENOEXEC;
	}
	return( nError );
}

static int	ReadSymbols( LnkImage_s* psImage, FILE* hFile, int lChunkSize, int lTabNum )
{
	char*						pNames = NULL;
	int32						i;

/*
		printf( "Symtab %d has %d symbols\n", lTabNum, sInpHdr.ih_nSymCount );
*/
	int32	lStringTabSize = lChunkSize - psImage->nSymCount * sizeof( ExeSymbol_s );

	if ( (lStringTabSize == 0) ||
				(pNames = malloc( lStringTabSize )) )
	{
		ExeSymbol_s*	pasExeSym = malloc( psImage->nSymCount * sizeof( ExeSymbol_s ) );

		if ( NULL != pasExeSym )
		{
			if ( fread( pasExeSym, psImage->nSymCount * sizeof( ExeSymbol_s ), 1, hFile ) == 1 )
			{
				for ( i = 0 ; i < psImage->nSymCount ; ++i )
				{
					psImage->pasSymbols[i].nNameOffset	=	pasExeSym[i].s_nNameOffset;
					psImage->pasSymbols[i].nSecNum			=	pasExeSym[i].s_nSecNum;
					psImage->pasSymbols[i].nAddress			=	pasExeSym[i].s_nAddress;
					psImage->pasSymbols[i].nType				=	pasExeSym[i].s_nType;
					psImage->pasSymbols[i].nClass				=	pasExeSym[i].s_nClass;
					psImage->pasSymbols[i].nNumAux			=	pasExeSym[i].s_nNumAux;
				}
			}
			else
			{
				free( pasExeSym );
				return( -EIO );
			}
			free( pasExeSym );
		}
/*
				else
				{
					for ( i = 0 ; i < psTable->st_nSymCount ; ++i )
					{
						ExeSymbol_s		sExeSym;

						if ( Read( nFile, &sExeSym, sizeof( ExeSymbol_s ) ) == sizeof( ExeSymbol_s ) )
						{
							psTable->st_pasSymbols[i].s_pzName			=	NULL;
							psTable->st_pasSymbols[i].s_lNameOffset	=	sExeSym.s_lNameOffset;
							psTable->st_pasSymbols[i].s_lSecNum			=	sExeSym.s_wSecNum;
							psTable->st_pasSymbols[i].s_nAddress		=	sExeSym.s_lAddress;
							psTable->st_pasSymbols[i].s_wType				=	sExeSym.s_wType;
							psTable->st_pasSymbols[i].s_wClass			=	sExeSym.s_wClass;
							psTable->st_pasSymbols[i].s_wNumAux			=	sExeSym.s_wNumAux;
						}
						else
						{
							return( -EIO );
						}
					}
				}
*/
				/***	Load symbol name table, and fixup name pointers	***/

		if ( lStringTabSize != 0 )
		{
			fread( pNames, lStringTabSize, 1, hFile );

			for ( i = 0 ; i < psImage->nSymCount ; ++i )
			{
				if ( -1 != psImage->pasSymbols[i].nNameOffset )
				{
					strcpy( psImage->pasSymbols[i].zName,
									&pNames[ psImage->pasSymbols[i].nNameOffset ] );

/*					printf( "Load dll<%s> sym '%s'\n", psImage->zName, psImage->pasSymbols[i].zName ); */
				}
				else
				{
					psImage->pasSymbols[i].zName[0] = '\0';
				}
			}
			free( pNames );
		}
	}
	return( 0 );
}

LnkImage_s* LoadDllExports( FILE* hFile )
{
	status_t	nError = 0;
	LnkImage_s*	psImage = calloc( sizeof( LnkImage_s ), 1 );

	if ( NULL == psImage ) {
		return( NULL );
	}

	nError = ReadImageHeader( psImage, hFile );

	if ( 0 == nError )
	{
/*		nError = AllocImageTables( psImage ); */

		if ( 0 == nError )
		{
			ChunkHdr_s	sChunkHdr;
			int					nSectionsRead	= 0;
			int					nRelocsRead 	= 0;
			int					nSymTabsRead 	= 0;

			for(;;)
			{
				nError = fread( &sChunkHdr, sizeof( ChunkHdr_s ), 1, hFile );

				if ( 0 == nError ) {
					break;
				}
				if ( 1 == nError )
				{
					nError = 0;

					switch( sChunkHdr.ch_nType )
					{
						case CHUNK_SECTION:
							fseek( hFile, sChunkHdr.ch_nSize - 8, SEEK_CUR );
							nSectionsRead++;
							break;
						case CHUNK_RELOCS:
							fseek( hFile, sChunkHdr.ch_nSize - 8, SEEK_CUR );
/*
							psImage->im_pasSections[ nRelocsRead ].se_nRelocFilePos =
								fseek( nFile, sChunkHdr.ch_nSize - 8, OFFSET_CURRENT );
							psImage->im_pasSections[ nRelocsRead ].se_nRelocFilePos -= sChunkHdr.ch_nSize - 8;
*/
							nRelocsRead++;

							break;
						case CHUNK_SYMBOLS:
							if ( ReadSymbols( psImage, hFile, sChunkHdr.ch_nSize - 8, nSymTabsRead ) == 0 ) {
								nSymTabsRead++;
							}
							break;
						default:
							printf( "WARNING : Unknown chunk %lx\n", sChunkHdr.ch_nType );
							fseek( hFile, sChunkHdr.ch_nSize - 8, SEEK_CUR );
							break;
					}
/*
					if ( nSectionsRead == psImage->im_nSecCount &&
							 nRelocsRead == psImage->im_nSecCount &&
							 nSymTabsRead == psImage->im_nSymTabCount )
					{
						break;
					}
*/
					if ( 1 == nSymTabsRead ) {
						break;
					}
				}
				else
				{
					if ( nError > 0 ) {
						printf( "ERROR : Extra bytes at end of executable %d\n", nError );
						nError = -ENOEXEC;
					}
					break;
				}
			}
/*
			if ( nSectionsRead == psImage->im_nSecCount &&
					 nRelocsRead == psImage->im_nSecCount &&
					 nSymTabsRead == psImage->im_nSymTabCount )

			{
				int	i;

	      File_s*	psFile = GetFileDesc( FindProcess( NULL ), nFile );

				psImage->im_psInode = psFile->f_psInode;
				psImage->im_psInode->i_nCount++;

				if ( BuildRelocMaps( psImage ) == 0 )
				{
				}

			}

	else {
	      printf( "ERROR : Invalid number of chunks\n" );
				nError = -ENOEXEC;
			}
*/
		}
	}

	if ( 0 != nError )
	{
		printf( "Failed to load dll (%d)\n", nError );
		psImage = NULL;
	}
	return( psImage );
}
