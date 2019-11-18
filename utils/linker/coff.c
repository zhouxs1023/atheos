#include	<stdio.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<string.h>
#include	<malloc.h>

#include	<exec/mman.h>

#ifndef __GNUC__		/* To make altexe.h work	*/
#define	__GNUC__
#endif

#include "../include/exec/types.h"

#include	"../include/macros.h"

#include	"link.h"

static int ReadCoffHeader( LnkImage_s* psImage, struct COFF_filehdr* psFileHeader, FILE* hFile )
{
	if ( fread( psFileHeader, sizeof(*psFileHeader), 1, hFile ) == 1 )
	{
		if ( COFF_SHORT( psFileHeader->f_magic ) == COFF_I386MAGIC )
		{
			int	nSymCount = COFF_LONG( psFileHeader->f_nsyms );
			psImage->nSectionCount							= COFF_SHORT( psFileHeader->f_nscns );
			psImage->nSymTabFilePos							= COFF_LONG( psFileHeader->f_symptr );
			psImage->nSymCount									=	nSymCount;

			psImage->pasSections = calloc( psImage->nSectionCount, sizeof( LnkSection_s ) );

			if ( NULL != psImage->pasSections )
			{
				psImage->pasSymbols = calloc( psImage->nSymCount, sizeof(LnkSymbol_s) );

				if ( NULL != psImage->pasSymbols )
				{
					return( 0 );
				}
				else
				{
					printf( "Error : No memory for main symbol table\n" );
				}
				free( psImage->pasSections );
				psImage->pasSections = NULL;
			}
			else
			{
				printf( "Error : No memory for section headers\n" );
			}
		}
		else
		{
			printf( "Error : Bad magic number %x\n", COFF_SHORT( psFileHeader->f_magic ) );
		}
	}
	else
	{
		printf( "Error : Could not read object file\n" );
	}

	return( -1 );
}





LnkImage_s*	LoadCoffImage( const char* pzName, int nDllCount )
{
	LnkImage_s*	psImage = NULL;
	FILE*				hFile = fopen( pzName, "rb" );
	struct			COFF_filehdr	sFileHeader;
	uint32			nSymTabPos;
	char*				pSymBuf;
	char* 			pzNameBuf;
	int					nSymBufSize;
	int					nSymNamesSize;
	int					nFileSize = sys_filelength( pzName );

	if ( NULL == hFile ) {
		return( NULL );
	}

	psImage = calloc( sizeof( LnkImage_s ), 1 );

	if ( NULL != psImage )
	{
		psImage->nLibCount = nDllCount;

		ReadCoffHeader( psImage, &sFileHeader, hFile );
/*			if ( ReadCoffHeader( psImage, &sFileHeader, hFile ) == 0 ) */
		if ( 1 )
		{
			uint32	nPhysAddr = g_nVirtualAddress;
			int			i;


			if ( 0 != COFF_SHORT(sFileHeader.f_opthdr) )
			{
				fseek( hFile, COFF_SHORT(sFileHeader.f_opthdr), SEEK_CUR );
			}

			for ( i = 0 ; i < psImage->nSectionCount ; ++i )
			{
				struct COFF_scnhdr	sSecHeader;

				if ( fread( &sSecHeader, sizeof(sSecHeader), 1, hFile ) == 1 )
				{
					LnkSection_s* psSection = &psImage->pasSections[i];

					psSection->nSize	 			= (COFF_LONG( sSecHeader.s_size ) + PAGE_SIZE - 1) & PAGE_MASK;
					psSection->nVirAddr			= COFF_LONG( sSecHeader.s_vaddr );
					psSection->nPhysAddr		= nPhysAddr;
					psSection->nFlags 			= COFF_LONG( sSecHeader.s_flags );
					psSection->nRelocCount	= (int)((uint16)COFF_SHORT( sSecHeader.s_nreloc ));
					psSection->nRelocFilePos=	COFF_LONG( sSecHeader.s_relptr );
					psSection->nFilePos			= COFF_LONG( sSecHeader.s_scnptr );

					psSection->pData 			= malloc( psSection->nSize );
					psSection->pasRelocs 	= calloc( psSection->nRelocCount, sizeof(LnkReloc_s) );

					nPhysAddr += (psSection->nSize + PAGE_SIZE - 1) & PAGE_MASK;
				}
			}

			for ( i = 0 ; i < psImage->nSectionCount ; ++i )
			{
				int	j;

				LnkSection_s* psSection = &psImage->pasSections[i];

				if ( psSection->nFlags & COFF_STYP_BSS ) {
					memset( psSection->pData, 0, psSection->nSize );
				} else {
					fseek( hFile, psSection->nFilePos, SEEK_SET );
					fread( psSection->pData, psSection->nSize, 1, hFile );
				}

				fseek( hFile, psSection->nRelocFilePos, SEEK_SET );

				for ( j = 0 ; j < psSection->nRelocCount ; ++j )
				{
					LnkReloc_s*				psReloc = &psSection->pasRelocs[j];
					struct COFF_reloc	sCoffRel;

					fread( &sCoffRel, sizeof( sCoffRel ), 1, hFile );

					psReloc->nVarAddr 	= COFF_LONG( sCoffRel.r_vaddr ) - psSection->nVirAddr;
					psReloc->nSymIndex	=	COFF_LONG( sCoffRel.r_symndx );
					psReloc->nType			=	COFF_SHORT( sCoffRel.r_type );
/*
					dbprintf( "Rel %d Adr = %x , Ind = %d , Typ = %d\n",
										psReloc->nVarAddr, psReloc->nSymIndex, psReloc->nType );
*/
				}
			}

				/* Load symbol table */

			nSymBufSize = nFileSize - COFF_LONG( sFileHeader.f_symptr );
			nSymNamesSize = nSymBufSize - sizeof(struct COFF_syment) * psImage->nSymCount;
			nSymBufSize = sizeof(struct COFF_syment) * psImage->nSymCount;

			pSymBuf = malloc( nSymBufSize );
			pzNameBuf = malloc( nSymNamesSize );

			if ( NULL != pSymBuf )
			{
				fseek( hFile, COFF_LONG( sFileHeader.f_symptr ), SEEK_SET );

				if ( fread( pSymBuf, nSymBufSize, 1, hFile ) != 1 )
				{
					printf( "Error : Unable to read COFF symbol table\n" );
				}
				if ( fread( pzNameBuf, nSymNamesSize, 1, hFile ) != 1 )
				{
					printf( "Error : Unable to read COFF symbol names\n" );
				}

				for ( i = 0 ; i < psImage->nSymCount ; ++i )
				{
					LnkSymbol_s*				psSymbol = &psImage->pasSymbols[ i ];
					struct COFF_syment*	psCoffSym = ((struct COFF_syment*) pSymBuf) + i;

					psSymbol->nAddress 	= COFF_LONG( psCoffSym->e_value );
					psSymbol->nSecNum		=	COFF_SHORT( psCoffSym->e_scnum ) - 1;
					psSymbol->nType			= COFF_SHORT( psCoffSym->e_type );
					psSymbol->nClass		=	psCoffSym->e_sclass[0];
					psSymbol->nNumAux		=	psCoffSym->e_numaux[0];

/*
					dbprintf( "Sym %d : Adr = %x Sec = %d Typ = %d Cld = %d\n", i,
										psSymbol->nAddress, psSymbol->nSecNum, psSymbol->nType, psSymbol->nClass );
*/
						/*** Make symbol relative to its own section	***/

/*						dbprintf( "Sym %d cls %d\n", i, psSymbol->nClass ); */
					if ( C_NULL != psSymbol->nClass )
					{
						if ( psSymbol->nSecNum >= 0 )
						{
							if ( psSymbol->nSecNum >= psImage->nSectionCount ) {
								printf( "Error : Symbol %d reference non-existing segment %d (%d , %d)\n",
												i, psSymbol->nSecNum, psSymbol->nType, psSymbol->nClass );
							} else {
								LnkSection_s* psSection = &psImage->pasSections[ psSymbol->nSecNum ];
								psSymbol->nAddress -= psSection->nVirAddr;

								if ( psSymbol->nAddress < 0 ) {
									printf( "Error : Symbol %d has address less than 0 (%d)\n", i, psSymbol->nAddress );
								}
								if ( psSymbol->nAddress >= psSection->nSize ) {
/*									printf( "Error : Symbol %d has address greater than sec size (%d)\n", i, psSymbol->nAddress ); */
								}
							}
						}
					}

					/*** Fetch symbold name ***/

					if ( 0 == COFF_LONG( psCoffSym->e.e.e_zeroes ) )
					{
						if ( psSymbol->nClass != C_NULL ) {
							strncpy( psSymbol->zName, pzNameBuf + COFF_LONG( psCoffSym->e.e.e_offset ), 127 );
							psSymbol->zName[127] = 0;
						}	else {
							strcpy( psSymbol->zName, "(NULL)" );
						}
					}
					else
					{
						memcpy( psSymbol->zName, psCoffSym->e.e_name, 8 );
						psSymbol->zName[8] = 0;
					}
				}
				free( pSymBuf );
			}
			else
			{
				dbprintf( "Error : No memory for symbol buffer (%d)\n", nSymBufSize );
			}
		}
		else
		{
			printf( "Error : Invalid COFF header\n" );
		}
	}
	return( psImage );
}
