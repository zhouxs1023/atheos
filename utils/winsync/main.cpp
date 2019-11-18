#include <windows.h>
#include <winbase.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#include <vector>

#include "inodeext.h"


//FILE*	g_hLogFile = NULL;

int read_old_info_file( const char* a_pzPath, std::vector<FileNodeInfo>* pcList )
{
    int     nNumEntries;
    char		zPath[ MAX_PATH ];

    strcpy( zPath, a_pzPath );
    strcat( zPath, "/" );
    strcat( zPath, LFD_OLD_INODE_FILE_NAME );

    FILE*	hFile = fopen( zPath, "rb" );

    if ( hFile == NULL ) {
	return( -1 );
    }

    if ( fread( &nNumEntries, sizeof( nNumEntries ), 1 , hFile ) != 1 ) {
	printf( "Error: Failed to read file info header\n" );
	fclose( hFile );
	return( -1 );
    }
  
    int nError = 0;

    for ( int i = 0 ; i < nNumEntries ; ++i )
    {
  	OldFileNodeInfo	sOldNode;
  	FileNodeInfo	  sNode;

	if ( fread( &sOldNode, sizeof( sOldNode ), 1, hFile ) != 1 ) {
	    nError = -1;
	    printf( "Failed to load inode %d\n", i );
	    break;
	}
	sNode.nSize  = sOldNode.nSize;
	sNode.nMode  = sOldNode.nMode;
	sNode.nMTime = sOldNode.nMTime;
	sNode.nATime = sOldNode.nATime;
	sNode.nCTime = sOldNode.nCTime;
	sNode.nUID   = 0;
	sNode.nGID   = 0;

	strcpy( sNode.zShortName, sOldNode.zShortName );
	strcpy( sNode.zLongName, sOldNode.zLongName );

	pcList->push_back( sNode );
    }
    fclose( hFile );
    return( nError );
    return( 0 );
}

int read_info_file( const char* a_pzPath, std::vector<FileNodeInfo>* pcList )
{
    FileInfoHeader_s sHeader;
    char				     zPath[ MAX_PATH ];

    strcpy( zPath, a_pzPath );
    strcat( zPath, "/" );
    strcat( zPath, LFD_INODE_FILE_NAME );

    FILE*	hFile = fopen( zPath, "rb" );

    if ( hFile == NULL ) {
	return( -1 );
    }

    if ( fread( &sHeader, sizeof( sHeader ), 1 , hFile ) != 1 ) {
	printf( "Error: Failed to read file info header\n" );
	fclose( hFile );
	return( -1 );
    }
  
    int nError = 0;

    for ( int i = 0 ; i < sHeader.nNumEntries ; ++i )
    {
  	FileNodeInfo	sNode;

	if ( fread( &sNode, sizeof( sNode ), 1, hFile ) != 1 ) {
	    nError = -1;
	    printf( "Failed to load inode %d of %d\n", i, sHeader.nNumEntries );
	    break;
	}
	pcList->push_back( sNode );
    }
    fclose( hFile );
    return( nError );
}

int write_inode_file( std::vector<FileNodeInfo>* pcList, const char* a_pzPath )
{
    FileInfoHeader_s sHeader;

    memset( &sHeader, 0, sizeof( sHeader ) );

    strcpy( sHeader.zWarning, LFD_INODE_WARNING );
    sHeader.nVersion = LFD_CUR_INODE_FILE_VERSION;
    sHeader.nNumEntries = pcList->size();

    char				zPath[ MAX_PATH ];

    strcpy( zPath, a_pzPath );
    strcat( zPath, "/" );
    strcat( zPath, LFD_INODE_FILE_NAME );

    FILE*	hFile = fopen( zPath, "wb" );

    if ( hFile == NULL ) {
	printf( "Error: Failed to create inode file: %s\n", zPath );
	return( -1 );
    }

    if ( fwrite( &sHeader, sizeof( sHeader ), 1, hFile ) != 1 ) {
	printf( "Error: Failed to write inode table header\n" );
	fclose( hFile );
	return( -1 );
    }

    int nError = 0;
    for ( int i = 0 ; i < pcList->size() ; ++i ) {
	FileNodeInfo sNode = (*pcList)[i];
	if ( fwrite( &sNode, sizeof( FileNodeInfo ), 1, hFile ) != 1 ) {
	    printf( "Error: Failed to write entry %d of %d to inode table\n", i, pcList->size() );
	    nError = -1;
	    break;
	}
    }
    fclose( hFile );
    return( nError );
}

int read_directory( const char* a_pzPath, std::vector<FileNodeInfo>* pcList )
{
    WIN32_FIND_DATA sFindFileData;
    HANDLE	    hDir;
    char	    zPath[ MAX_PATH ];

    strcpy( zPath, a_pzPath );
    strcat( zPath, "/*.*" );
	
    for ( hDir = FindFirstFile( zPath, &sFindFileData ) ; INVALID_HANDLE_VALUE != hDir ;  )
    {
	if ( strcmp( sFindFileData.cFileName, "." ) == 0 || strcmp( sFindFileData.cFileName, ".." ) == 0 ) {
	    if ( FindNextFile( hDir, &sFindFileData ) == 0 ) {
		break;
	    }
	    continue;
	}
	struct stat sStat;

	strcpy( zPath, a_pzPath );
	strcat( zPath, "/" );
	strcat( zPath, sFindFileData.cFileName );

	if ( stat( zPath, &sStat ) < 0 ) {
	    printf( "Error : Could not stat() %s\n", zPath );
	    break;
	}

	FileNodeInfo sNode;

	strcpy( sNode.zLongName, sFindFileData.cFileName );
	strcpy( sNode.zShortName, sFindFileData.cAlternateFileName );

	if ( '\0' == sNode.zShortName[0] ) {
	    int	i = 0;
	    strcpy( sNode.zShortName, sNode.zLongName );
	    while( sNode.zLongName[i] = tolower( sNode.zShortName[i] ) ) {
		i++;
	    }
	}

	sNode.nSize = sStat.st_size;
	sNode.nMode = (( sStat.st_mode & _S_IFDIR ) ? DOS_S_IFDIR : DOS_S_IFREG) | 0777;
	sNode.nUID = 0;
	sNode.nGID = 0;
	sNode.nCTime = sStat.st_ctime;
	sNode.nATime = sStat.st_atime;
	sNode.nMTime = sStat.st_mtime;

	pcList->push_back( sNode );

	if ( FindNextFile( hDir, &sFindFileData ) == 0 ) {
	    break;
	}
    }
    FindClose( hDir );
    return( 0 );
}

void merge_file_lists( std::vector<FileNodeInfo>* pcDirList, std::vector<FileNodeInfo>* pcFileList )
{
    for ( int i = 0 ; i < pcDirList->size() ; ++i ) {
	bool bFound = false;
	for ( int j = 0 ; j < pcFileList->size() ; ++j ) {
	    if ( stricmp( (*pcDirList)[i].zShortName, (*pcFileList)[j].zShortName ) == 0 ) {
		(*pcDirList)[i].nMode  = (*pcFileList)[j].nMode;
		(*pcDirList)[i].nUID   = (*pcFileList)[j].nUID;
		(*pcDirList)[i].nGID   = (*pcFileList)[j].nGID;
		(*pcDirList)[i].nMTime = (*pcFileList)[j].nMTime;
		(*pcDirList)[i].nATime = (*pcFileList)[j].nATime;
		(*pcDirList)[i].nCTime = (*pcFileList)[j].nCTime;
		strcpy( (*pcDirList)[i].zLongName, (*pcFileList)[j].zLongName );

		pcFileList->erase( pcFileList->begin() + j );
		bFound = true;
		break;
	    }
	}
	if ( bFound == false ) {
	    for ( int j = 0 ; j < pcFileList->size() ; ++j ) {
		if ( stricmp( (*pcDirList)[i].zLongName, (*pcFileList)[j].zLongName ) == 0 ) {
		    (*pcDirList)[i].nMode  = (*pcFileList)[j].nMode;
		    (*pcDirList)[i].nUID   = (*pcFileList)[j].nUID;
		    (*pcDirList)[i].nGID   = (*pcFileList)[j].nGID;
		    (*pcDirList)[i].nMTime = (*pcFileList)[j].nMTime;
		    (*pcDirList)[i].nATime = (*pcFileList)[j].nATime;
		    (*pcDirList)[i].nCTime = (*pcFileList)[j].nCTime;
		    pcFileList->erase( pcFileList->begin() + j );
		    bFound = true;
		    break;
		}
	    }
	}
	if ( bFound == false ) {
	    printf( "File %-12s -> %s was not found in the inode table\n", (*pcDirList)[i].zShortName, (*pcDirList)[i].zLongName );
	}
    }
}

void rename_files( const char* pzPath, std::vector<FileNodeInfo>* pcList )
{
    for ( int i = 0 ; i < pcList->size() ; ++i ) {
	char zPath[MAX_PATH];

	strcpy( zPath, pzPath );
	strcat( zPath, "/" );
	strcat( zPath, (*pcList)[i].zShortName );

  	WIN32_FIND_DATA sFindFileData;
	HANDLE		hDir;
    
	hDir = FindFirstFile( zPath, &sFindFileData );

	if ( hDir == INVALID_HANDLE_VALUE ) {
	    printf( "Error: Failed to get file infor for %s\n", zPath );
	    continue;
	}
	FindClose( hDir );
	if ( strcmp( sFindFileData.cFileName, (*pcList)[i].zLongName ) != 0 ) {
	    char zNewPath[MAX_PATH];
	    char zOldName[MAX_PATH];

	    strcpy( zOldName, sFindFileData.cFileName );
	    strcpy( zNewPath, pzPath );
	    strcat( zNewPath, "/" );
	    strcat( zNewPath, (*pcList)[i].zLongName );

	    if ( rename( zPath, zNewPath ) != 0 ) {
		printf( "Error: Failed to rename %s to %s\n", zPath, zNewPath );
	    }

	    hDir = FindFirstFile( zNewPath, &sFindFileData );
	    if ( hDir == INVALID_HANDLE_VALUE ) {
		printf( "Error: Failed to get file info for %s after renaming it\n", zNewPath );
		continue;
	    }
	    FindClose( hDir );
	    strcpy( (*pcList)[i].zShortName, sFindFileData.cAlternateFileName );
	    printf( "Renaming %s to %s (%s)\n", zOldName, (*pcList)[i].zLongName, (*pcList)[i].zShortName );
	}
    }
}

void dump_file_list( const std::vector<FileNodeInfo>& cList )
{
    for ( int i = 0 ; i < cList.size() ; ++i ) {
	printf( "%-12s %10d %s\n", cList[i].zShortName, cList[i].nSize, cList[i].zLongName );
    }
}

void SyncDir( const char* a_pzPath, bool bRenameFiles, bool bRecursive )
{
    std::vector<FileNodeInfo> sTable;
    std::vector<FileNodeInfo> sDir;

    printf( "Sync files in %s\n", a_pzPath );
//	fprintf( g_hLogFile, "Sync files in %s\n", a_pzPath );


    if ( read_info_file( a_pzPath, &sTable ) != 0 ) {
	sTable.clear();
	printf( "No new inode table, check for old\n" );
	read_old_info_file( a_pzPath, &sTable );
    }
    read_directory( a_pzPath, &sDir );

    merge_file_lists( &sDir, &sTable );

    if ( sTable.size() > 0 ) {
	printf( "Files found in inode table, but not in directory: \n" );
	dump_file_list( sTable );
	printf( "\n\n" );
    }
    if ( bRenameFiles ) {
	rename_files( a_pzPath, &sDir );
    }
    write_inode_file( &sDir, a_pzPath );

    if ( bRecursive ) {
	for ( int i = 0 ; i < sDir.size() ; ++i ) {
	    if ( DOS_S_ISDIR( sDir[i].nMode ) ) {
		char	zPath[ MAX_PATH ];
		strcpy( zPath, a_pzPath );
		strcat( zPath, "/" );
		strcat( zPath, sDir[i].zShortName );
		SyncDir( zPath, bRenameFiles, true );
	    }
	}
    }
}

void ListDir( const char* a_pzPath, bool bReqursive )
{
    std::vector<FileNodeInfo> sTable;
    std::vector<FileNodeInfo> sDir;

    printf( "Files in %s\n", a_pzPath );

    if ( read_info_file( a_pzPath, &sTable ) != 0 ) {
	sTable.clear();
	printf( "No new inode table, check for old\n" );
	read_old_info_file( a_pzPath, &sTable );
    }
    read_directory( a_pzPath, &sDir );

    dump_file_list( sTable );
    printf( "\n\n" );

    merge_file_lists( &sDir, &sTable );

    printf( "Files found in inode table, but not in directory: \n" );
    dump_file_list( sTable );

    if ( bReqursive ) {
	for ( int i = 0 ; i < sDir.size() ; ++i ) {
	    if ( DOS_S_ISDIR( sDir[i].nMode ) ) {
		char		zPath[ MAX_PATH ];
		strcpy( zPath, a_pzPath );
		strcat( zPath, "/" );
		strcat( zPath, sDir[i].zShortName );
		ListDir( zPath, true );
	    }
	}
    }
}

void usage()
{
    printf( "Usage: winsync [-h][-l][-r][--no-rename][path]\n" );
    printf( "-h            print this help page\n" );
    printf( "-l            list content of inode table (don't rename files or update tables)\n" );
    printf( "-r            requrse into subdirectories\n" );
    printf( "--no-rename   don't rename files, only update the inode table\n" );
}

int	main( int argc, char** argv )
{
    int	i;
    bool bReqursive = false;
    bool bList = false;
    bool bRename = true;
    char zPath[MAX_PATH] = ".";

    for ( i = 1 ; i < argc ; ++i ) {
	if ( strcmp( argv[i], "-h" ) == 0 ) {
	    usage();
	    exit(1);
	} else if ( strcmp( argv[i], "-l" ) == 0 ) {
	    bList = true;
	} else if ( strcmp( argv[i], "-r" ) == 0 ) {
	    bReqursive = true;
	} else if ( strcmp( argv[i], "--no-rename" ) == 0 ) {
	    bRename = false;
	} else {
	    if ( argv[i][0] == '-' ) {
		usage();
		exit(1);
	    } else {
		strcpy( zPath, argv[i] );
	    }
	}
    }

//	g_hLogFile = fopen( "wsync.log", "w" );

//	if ( NULL == g_hLogFile ) {
//		printf( "ERROR : Could not open log file!\n" );
//		exit( 1 );
//	}

    if ( bList == false ) {
	SyncDir( zPath, bRename, bReqursive );
    } else {
	ListDir( zPath, bReqursive );
    }
//	fclose( g_hLogFile );

    return( 0 );
}
