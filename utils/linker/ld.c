#include	<stdio.h>
#include	<unistd.h>
#include	<errno.h>
#include	<process.h>
#include	<malloc.h>

int	main( int argc, char** argv )
{
	char* pzMode = getenv( "ALTOSMODE" );
	char*	pzVerify = getenv( "LD_VERIFY" );
	char*	pzLeveTemp = getenv( "LD_LEAVE_TEMP" );
	int		bVerify;

	if ( NULL != pzVerify && 'y' == pzVerify[0] ) {
		bVerify = 1;
	} else {
		bVerify = 0;
	}


	if ( !(NULL != pzMode && stricmp( pzMode, "no" ) == 0) )
	{
		char**	ldargv;
		char*		aldargv[256];

//		printf( "Linker enter AltOs mode\n" );

		ldargv = malloc( (argc + 10) * sizeof( char*) );

		if ( NULL != ldargv )
		{
			char*	pzOutName = "a.out";
			char	zTempObjName[ 256 ];
			char*	pzTempObjName;

			int	i;
			int	j = 0;
			int	k = 0;

			int	nResult;

			if ( NULL == pzLeveTemp || strcmp( pzLeveTemp, "no" ) == 0 ) {
				pzTempObjName = tmpnam( zTempObjName );
			} else {
				pzTempObjName = "image.obj";

				printf( "leaves temp file %s\n", pzTempObjName );
			}

			if ( NULL == pzTempObjName ) {
				printf( "error : failed to create temp obj file : %s\n", strerror( errno ) );
				exit( 1 );
			}
			
			aldargv[ k++ ] = "alnk";
 			aldargv[ k++ ] = pzTempObjName; /* "~ld2ald~.obj"; */

			ldargv[ j++ ] = argv[0];
/*			ldargv[ j++ ] = "--no-force-exe-suffix"; */
			ldargv[ j++ ] = "-Ur";
			ldargv[ j++ ] = "-d";
/*			ldargv[ j++ ] = "-X"; */
/*			ldargv[ j++ ] = "-s"; */
			ldargv[ j++ ] = "--split-by-reloc";
			ldargv[ j++ ] = "65535";

			for ( i = 1 ; i < argc ; ++i )
			{
				if ( i > 1 && strcmp( argv[ i - 1 ], "-o" ) == 0 )
				{
					ldargv[j++] = pzTempObjName; /* "~ld2ald~.obj"; */

					aldargv[k++] = "-o";
					aldargv[k++] = argv[i];
					pzOutName		 = argv[i];
					continue;
				}

				if ( strstr( argv[i], ".so" ) || strstr( argv[i], ".def" ) || strcmp( argv[i], "-a" ) == 0 ) {
					aldargv[k++] = argv[i];
					continue;
				}
				ldargv[j++] = argv[i];
			}
/*
			if ( strstr( pzOutName, "kernel.so" ) == NULL && strstr( pzOutName, "libuser.so" ) == NULL )
			{
				aldargv[ k++ ] = "-luser.so";
			}
			*/
			ldargv[ j++ ] = NULL;
			aldargv[ k++ ] = NULL;

#if 1
			if ( bVerify )
			{
				int	nChar;

				printf( "Link %d?\n", pzOutName );

				nChar = getchar();

				if ( nChar != 'y' ) {
					exit( 0 );
				}
			}
#endif
		/* --force-exe-suffix */

/*			spawnvp( P_WAIT, "echo", ldargv ); */
			nResult = spawnvp( P_WAIT, "gld", ldargv );
			if ( 0 == nResult ) {
/*				spawnvp( P_WAIT, "echo", aldargv ); */
				nResult = spawnvp( P_WAIT, "alnk", aldargv );
			}
			if ( NULL == pzLeveTemp || strcmp( pzLeveTemp, "no" ) == 0 ) {
				unlink( pzTempObjName );
			}
			return( nResult );
		}
		else
		{
			printf( "Out of memory!\n" );
			exit( 1 );
		}
		return( 0 );
	}
	else
	{
		char**	ldargv;
		int	i;

		ldargv = malloc( (argc + 1) * sizeof( char*) );

		for ( i = 0 ; i < argc ; ++i ) {
			ldargv[i] = argv[i];
		}
		ldargv[ argc ] = NULL;

		printf( "Linker enter DOS mode\n" );

		exit( spawnvp( P_WAIT, "gld", ldargv ) );
	}
	return( 0 );
}
