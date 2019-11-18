#include	<stdio.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<string.h>
#include	<malloc.h>

#include	<atheos/aelf.h>
#include	<atheos/kernel.h>

#include	<exec/mman.h>


#include "../include/exec/types.h"

#include	"../include/macros.h"

#include	"link.h"




LnkImage_s* load_aelf_lib( FILE* hFile, const char* pzImageName )
{
	
	AElf32_ElfHdr_s					sElfHeader;
	AElf32_SectionHeader_s*	pasSections;
	int											nError;
	LnkImage_s*							psImage;
	
	nError = fread( &sElfHeader, sizeof( sElfHeader ), 1, hFile );

	if ( 1 != nError ) {
		printf( "failed to read elf header\n" );
		return( NULL );
	}

	if ( AELFMAG0 != sElfHeader.e_anIdent[ AEI_MAG0 ] || AELFMAG1 != sElfHeader.e_anIdent[ AEI_MAG1 ] ||
			 AELFMAG2 != sElfHeader.e_anIdent[ AEI_MAG2 ] || AELFMAG3 != sElfHeader.e_anIdent[ AEI_MAG3 ] )
	{
		printf( "ERROR : Elf file has bad magic\n" );
		return( NULL );
	}

	if ( AELFCLASS32 != sElfHeader.e_anIdent[ AEI_CLASS ] || AELFDATALSB != sElfHeader.e_anIdent[ AEI_DATA ] ) {
		printf( "ERROR : elf file has wrong bitsize, or endian type\n" );
		return( NULL );
	}

	if ( sElfHeader.e_nSecHdrSize != sizeof( AElf32_SectionHeader_s ) )
	{
		printf( "ERROR : elf file has invalid section header size %d\n", sElfHeader.e_nSecHdrSize );
		return( NULL );
	}

	psImage = calloc( sizeof( LnkImage_s ), 1 );

	if ( NULL == psImage ) {
		printf( "error : out of memory\n" );
		return( NULL );
	}
	
	pasSections = malloc( sElfHeader.e_nSecHdrCount * sElfHeader.e_nSecHdrSize );

	if ( NULL == pasSections ) {
		printf( "ERROR : load_elf_image() ran out of memory\n" );
		return( NULL );
	}

	fseek( hFile, sElfHeader.e_nSecHdrOff, SEEK_SET );
	
	nError = fread( pasSections, sElfHeader.e_nSecHdrCount * sElfHeader.e_nSecHdrSize, 1, hFile );

	if ( 1 == nError )
	{
		int	nSymCount 		= 0;
		BOOL	bDynSymTabFound = FALSE;
		int	i;

		for ( i = 0 ; i < sElfHeader.e_nSecHdrCount ; ++i )
		{
			switch( pasSections[i].sh_nType )
			{
				case SHT_DYNSYM:
					bDynSymTabFound = TRUE;
						/*** FALL THROUGH ***/
				case SHT_SYMTAB:
					nSymCount = pasSections[i].sh_nSize / pasSections[i].sh_nEntrySize;
					break;
			}
		}

		psImage->nSymCount			= nSymCount;

		strncpy( psImage->zName, pzImageName, MAX_NAME_LENGTH );
		psImage->zName[ MAX_NAME_LENGTH - 1 ] = '\0';

//		if ( 0 == nError )
		{
			char*	pSymNames = NULL;

			for ( i = 0 ; i < sElfHeader.e_nSecHdrCount ; ++i )
			{
				switch( pasSections[i].sh_nType )
				{
					case SHT_STRTAB:
					{
						fseek( hFile, pasSections[i].sh_nOffset, SEEK_SET );
						pSymNames = malloc( pasSections[i].sh_nSize );

						if ( NULL == pSymNames ) {
							printf( "load_elf_image() : no memory for string table\n" );
							return( NULL );	// FIX ME FIX ME : Do some cleanup ?
						}
						fread( pSymNames, pasSections[i].sh_nSize, 1, hFile );	// FIX ME : Check for error
						break;
					}
				}
			}

			if ( NULL == pSymNames ) {
				printf( "error : dll containe no string section!\n" );
				return( NULL );
			}
			
			
			for ( i = 0 ; i < sElfHeader.e_nSecHdrCount ; ++i )
			{
				switch( pasSections[i].sh_nType )
				{
					case SHT_DYNSYM:
					case SHT_SYMTAB:
					{
						if ( FALSE == bDynSymTabFound || SHT_DYNSYM == pasSections[i].sh_nType )
						{
							int		nSymCount = pasSections[i].sh_nSize / pasSections[i].sh_nEntrySize;
							AElf32_Symbol_s*	pasExeSym = malloc( pasSections[i].sh_nSize );

							psImage->pasSymbols = malloc( nSymCount * sizeof( LnkSymbol_s ) );
						
							fseek( hFile, pasSections[i].sh_nOffset, SEEK_SET );
						
							if ( NULL != pasExeSym )
							{
								if ( fread( pasExeSym, pasSections[i].sh_nSize, 1, hFile ) == 1 )
								{
									int	j;
								
									for ( j = 0 ; j < nSymCount ; ++j )
									{
//									psImage->pasSymbols[j].pzName			=	&pSymNames[ pasExeSym[j].st_nName ];
										strcpy( psImage->pasSymbols[j].zName, &pSymNames[ pasExeSym[j].st_nName ] );
										psImage->pasSymbols[j].nNameOffset	=	pasExeSym[j].st_nName;
										psImage->pasSymbols[j].nSecNum			=	pasExeSym[j].st_nSecIndex;
										psImage->pasSymbols[j].nAddress			=	pasExeSym[j].st_nValue;
										psImage->pasSymbols[j].nType				=	pasExeSym[j].st_nInfo;

//									printf( "Sym %d has name %d\n", j, psImage->pasSymbols[j].zName );
									}
								}
								else
								{
									free( pasExeSym );
									return( NULL );
								}
								free( pasExeSym );
							}
							else
							{
								int	j;
							
								for ( j = 0 ; j < psImage->nSymCount ; ++j )
								{
									AElf32_Symbol_s		sExeSym;

									if ( fread( &sExeSym, sizeof( sExeSym ), 1, hFile ) == 1 )
									{
//									psImage->pasSymbols[j].pzName			=	&pSymNames[ pasExeSym[j].st_nName ];
										strcpy( psImage->pasSymbols[j].zName, &pSymNames[ pasExeSym[j].st_nName ] );
										psImage->pasSymbols[j].nNameOffset=	sExeSym.st_nName;
										psImage->pasSymbols[j].nSecNum		=	sExeSym.st_nSecIndex;
										psImage->pasSymbols[j].nAddress		=	sExeSym.st_nValue;
										psImage->pasSymbols[j].nType			=	sExeSym.st_nInfo;
									}
									else
									{
										return( NULL );
									}
								}
							}
						}
						break;
					}
				}
			}
		}
	}
//	printf( "dll %s loaded\n", pzImageName );
	return( psImage );
}
