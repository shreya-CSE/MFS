/*
	NAME: SHREYA CHINDEPALLI
	ID: 1001845703

	NAME: AAROHI BANSAL
	ID: 1001851146

*/

// The MIT License (MIT)
//
// Copyright (c) 2020 Trevor Bakker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>


// #define MAX_NUM_ARGUMENTS 3

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 11     // Mav shell only supports four arguments

#define MAX_HISTORY_COUNT 15     // Maximum commands stored and shown as  history

#define MAX_NUM_CHILD_PIDS 100     // Number of Child PIDs to track 

#define MAX_DIRS 16     // Maximum directories expected in Change Dir command 

#define MAX_FILE_NAME 11     // Maximum file or dir name in Change Dir command 

// Directory structure to extract directory data
//
struct __attribute__ ((__packed__)) DirectoryEntry
{
	char DIR_Name[11];
	uint8_t DIR_Attr;
	uint8_t unused1[8];
	uint16_t ClusterHigh;
	uint8_t unused2[4];
	uint16_t ClusterLow;
	uint32_t size;
};

struct DirectoryEntry dir[16]; // all directory content 

//Boot Sector and BPB Stats
int16_t BPB_BytesPerSec; 
int8_t BPB_SecPerClus; 
int16_t BPB_RsvdSecCnt;
int8_t BPB_NumFATS;
int32_t BPB_FATSz32;
int32_t BPB_RootClus;

int32_t RootDirSectors = 0; // for FAT32 this is always ZERO
int32_t FirstDataSector; 

int32_t RootDirFileReadPos;
int32_t CurrentDirFileReadPos;
int32_t ParentDirFileReadPos;

FILE *fp = NULL;
int open_flag = 0;

void utils_mem_free(char **tokens, int token_cnt);
int LBAtoOffset(int32_t sector);
int16_t NextLB(int32_t sector);
int compare_dirnames(char *IMG_Name, char *input);
int compare_filenames(char *IMG_Name, char *input);
int compare_filenames_undel(char *IMG_Name, char *input);

// Function Name: utils_mem_free
//
// arg1: char **tokens : array of strings for which memory to be freed
// arg2: int token_cnt : number of elements in the tokens string array
// 
// return value: no return value (void)
//
// Description: Functional will loop through and free the memory of each string
// in the array.  
//
void utils_mem_free(char **tokens, int token_cnt)
{
	int cnt;
	for(cnt=0; cnt<token_cnt; cnt++)
	{
		if(tokens[cnt] != NULL)
		{
			free(tokens[cnt]);
		}
	}
	return;
}

int compare_filenames_undel(char *IMG_Name, char *input)
{

  char expanded_name[11];
  char inp_str[11];

  strcpy(inp_str, input);

  memset( expanded_name, ' ', 11 );

  char *token = strtok( inp_str, "." );

  strncpy( expanded_name, token, strlen( token ) );

  token = strtok( NULL, "." );

  if( token )
  {
    strncpy( (char*)(expanded_name+7), token, strlen(token ) );
  }

  expanded_name[10] = '\0';

  int i;
  for( i = 0; i < 10; i++ )
  {
    expanded_name[i] = toupper( expanded_name[i] );
  }

  if( strncmp( expanded_name, IMG_Name, 10 ) == 0 )
  {
    return 1;
  }

  return 0;
}

int compare_filenames(char *IMG_Name, char *input)
{

  char expanded_name[12];
  char inp_str[12];

  strcpy(inp_str, input);

  memset( expanded_name, ' ', 12 );

  char *token = strtok( inp_str, "." );

  strncpy( expanded_name, token, strlen( token ) );

  token = strtok( NULL, "." );

  if( token )
  {
    strncpy( (char*)(expanded_name+8), token, strlen(token ) );
  }

  expanded_name[11] = '\0';

  int i;
  for( i = 0; i < 11; i++ )
  {
    expanded_name[i] = toupper( expanded_name[i] );
  }

  if( strncmp( expanded_name, IMG_Name, 11 ) == 0 )
  {
    return 1;
  }

  return 0;
}

int compare_dirnames(char *IMG_Name, char *input)
{
  char temp_name[12];

  strcpy(temp_name, input);
  int j;
  for(j=strlen(temp_name); j< strlen(IMG_Name); j++)
  {
    temp_name[j] = ' ';
  }

  temp_name[11] = '\0';

  int i;
  for( i = 0; i < 11; i++ )
  {
    temp_name[i] = toupper( temp_name[i] );
  }

  if( strncmp( temp_name, IMG_Name, 11 ) == 0 )
  {
    return 1;
  }

  return 0;
}

/*
 * Function       : NextLB
 * Purpose        : Given a logical block address, look up into the first FAT
 * and return the logical blcok address of the block in the file. If there is
 * no further blocks then return -1
 */
int16_t NextLB(int32_t sector)
{
  uint32_t FATAddress = (BPB_BytesPerSec * BPB_RsvdSecCnt) + (sector * 4);
  int16_t val;
  fseek(fp, FATAddress, SEEK_SET);
  fread(&val, 2, 1, fp);
  return val;
}

/*
 * Function       : LBAtoOffset
 * Parameters     : The current sector number that points to a block of data
 * Returns        : The value of the address for the block of data
 * Description    : Find the starting address of a block of data given the
 * sector number corresponding to that data block 
 */

int LBAtoOffset(int32_t sector)
{
  return (( sector - 2 ) * BPB_BytesPerSec) + (BPB_BytesPerSec * BPB_RsvdSecCnt) + (BPB_NumFATS * BPB_FATSz32 * BPB_BytesPerSec);
}

int main()
{

  char *command_string = (char*) malloc( MAX_COMMAND_SIZE );
  int child_pids[MAX_NUM_CHILD_PIDS];
  int child_pids_count=0;
  char command_list[MAX_HISTORY_COUNT][MAX_COMMAND_SIZE];
  int command_count = 0;
  int cnt;
  char *token[MAX_NUM_ARGUMENTS];
  int token_count = 0;
  char *temp_tok=NULL;
  int32_t FirstSectorofCluster; 

  while( 1 )
  {
    token_count = 0;

    // Print out the mfs prompt
    printf ("mfs> ");

    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (command_string, MAX_COMMAND_SIZE, stdin) );

    /* Parse input */
    int hist_num=-1;  //capture command history number

    // Pointer to point to the token
    // parsed by strsep
    char *argument_ptr=NULL;

    //remove new line character
    if(command_string[strlen(command_string) - 1] == '\n')
    {
      command_string[strlen(command_string) - 1] = '\0';
    }

    //
    //blank input
    //shell will print another prompt
    if(strlen(command_string) == 0)
    {
      continue;
    }

    //remove any end of line blank spaces
    while(command_string[strlen(command_string) - 1] == ' ')
    {
      command_string[strlen(command_string) - 1] = '\0';
    }

    char *working_string  = strdup( command_string );


    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *head_ptr = working_string;
    char *quit_ptr = "quit";
    char *exit_ptr = "exit";

    for(cnt=0; cnt<MAX_NUM_ARGUMENTS; cnt++)
    {
      token[cnt] = NULL;
    }

    // Tokenize the input strings with whitespace used as the delimiter
    while (((argument_ptr=strsep(&working_string, WHITESPACE)) != NULL) && (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( argument_ptr, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
      token_count++;
    }

    free( working_string );
    free( head_ptr );

    if (token[0] != NULL) 
    {
      if ((strcmp(token[0], quit_ptr) == 0) || (strcmp(token[0], exit_ptr) == 0)) {
        free(command_string);
        utils_mem_free(token, MAX_NUM_ARGUMENTS);
        if(fp)
        { 
          fclose(fp);
        }
	return 0;
      }
      if(strcmp(token[0], "open") == 0 && token[1] == NULL) 
      {
        printf("Filename needed -> SYNTAX: open <FILENAME>\n");
      }
      else if(strcmp(token[0], "open") == 0 && token[1] != NULL)
      {
        if(open_flag == 1) // file is already open
        {
          printf("File is already open\n");
          continue;
        }
        else
        {
          fp = fopen(token[1], "r+");
          if(fp != NULL)
          {
            open_flag = 1; // set file open status
          }
          else
          {
            printf("Unable to open file: %s\n", token[1]);
            continue;
          }
          // READ all necessary BPB values
          // read Bytes Per Sector
          fseek(fp, 11, SEEK_SET);
          fread(&BPB_BytesPerSec, 2, 1, fp); 
          // read Sectors per Cluster
          fseek(fp, 13, SEEK_SET);
          fread(&BPB_SecPerClus, 1, 1, fp); 
          // read Reserved Sectors Count
          fseek(fp, 14, SEEK_SET);
          fread(&BPB_RsvdSecCnt, 2, 1, fp); 
          // read Number of FATS
          fseek(fp, 16, SEEK_SET);
          fread(&BPB_NumFATS, 1, 1, fp); 
          // read FAT32 size - count of sectors occupied by one FAT
          fseek(fp, 36, SEEK_SET);
          fread(&BPB_FATSz32, 4, 1, fp); 
          // read Root Cluster number
          fseek(fp, 44, SEEK_SET);
          fread(&BPB_RootClus, 4, 1, fp); 
          //
          //compute first data sector of data area
          FirstDataSector = BPB_RsvdSecCnt + (BPB_NumFATS * BPB_FATSz32) + RootDirSectors;
          FirstSectorofCluster = ((BPB_RootClus - 2) * BPB_SecPerClus) + FirstDataSector;
          //printf("FirstSectorofCluster: 0x%08X : %d\n", FirstSectorofCluster, FirstSectorofCluster);

          // ensure all directory positions are set and tracked
          RootDirFileReadPos = FirstSectorofCluster; // RootDir of file system
          //CurrentDirFileReadPos = RootDirFileReadPos; // Current directory
          CurrentDirFileReadPos = RootDirFileReadPos * BPB_BytesPerSec; // Current directory
          ParentDirFileReadPos = -1; // dot dot directory - for root it will be -1
        }
      }
      else if(strcmp(token[0], "close") ==0)
      {
        if(fp)
        {
          fclose(fp);
          fp = NULL;
          open_flag = 0; // reset file open status flag
        }
	else
	{
	  printf("Error: File system image must be opened first.\n");
	}
      }
      else if(strcmp(token[0], "info") == 0)
      {
        if(!fp)
        {
          printf("Error: Image file not open.\n");
          continue;
        }
	if (open_flag == 1)
	{
          printf("BPB_BytesPerSec: 0x%08X : %d\n", BPB_BytesPerSec, BPB_BytesPerSec);
          printf("BPB_SecPerClus: 0x%08X : %d\n", BPB_SecPerClus, BPB_SecPerClus);
          printf("BPB_RsvdSecCnt: 0x%08X : %d\n", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
          printf("BPB_NumFATS: 0x%08X : %d\n", BPB_NumFATS, BPB_NumFATS);
          printf("BPB_FATSz32: 0x%08X : %d\n", BPB_FATSz32, BPB_FATSz32);
          //// additional values
          printf("BPB_RootClus: 0x%08X : %d\n", BPB_RootClus, BPB_RootClus);
          printf("FirstDataSector: 0x%08X : %d\n", FirstDataSector, FirstDataSector);
	}
	else 
        {
	  printf("Error: File system image must be opened first\n");
        }
      }
      else if(strcmp(token[0], "get") == 0)
      {
        if(!fp)
        {
          printf("Error: File system image must be opened first\n");
          continue;
        }

        if(token[1] == NULL)
        {
          printf("Error: Input file name is missing\n");
          continue;
        }

        fseek(fp, CurrentDirFileReadPos, SEEK_SET);
        fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp); 
        int i, offset;
        int16_t nextLB;
        char temp_str[12];
	int size_left;
	int size_read;
	int ClusSize;
	int found = 0;
        char *str=NULL;
        for(i=0; i < 16; i++)
        {
          if((dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x20) && dir[i].size > 0) // file or directory
          {
            strncpy(temp_str, dir[i].DIR_Name, 11);
            temp_str[11] = '\0';

            if(compare_filenames(temp_str, token[1]) == 1)
            { 
	      size_read = 0;
	      size_left = dir[i].size - size_read;
	      ClusSize = BPB_SecPerClus*BPB_BytesPerSec;
              FILE *fp_w=NULL;
              fp_w = fopen(token[1], "w");
              str = malloc(ClusSize);
              nextLB = dir[i].ClusterLow;
              while(nextLB != -1)
              {
                offset = LBAtoOffset(nextLB);
                fseek(fp, offset, SEEK_SET);
	        if(size_left <= ClusSize)
	        {
                  fread(str, size_left, 1, fp); 
                  fwrite(str, size_left, 1, fp_w);
		  size_read = size_read +size_left;
		  size_left = dir[i].size - size_read;
		}
		else 
		{
		  fread(str, ClusSize, 1, fp);
		  fwrite(str, ClusSize, 1, fp_w);
		  size_read = size_read + ClusSize;
		  size_left = dir[i].size - size_read;
		}
		nextLB = NextLB(nextLB);
              }
              fclose(fp_w);
	      printf("File [%s] found and retrieved\n", token[1]);
	      found = 1;
	    }
          }
        }
	if (found == 0) 
	{
	  printf("Error: File not found\n");
	}
      }
      else if(strcmp(token[0], "cd") == 0 )      
      {
        if (!fp)
        {
	  printf("Error: File system image must be opened first.\n");
	  continue;
	}
	if (token[1] == NULL)
	{
	  printf("Error: Input file name is missing\n");
	  continue;
        }	
	if(token[1][0] == '/') // look from rootdir
	{
	  char dir_tokens[MAX_DIRS][MAX_FILE_NAME];
	  int dir_cnt;
	  int i=0;
	
          temp_tok = strtok(token[1], "/");
	  while(temp_tok != NULL)
	  {
	    strcpy(dir_tokens[i], temp_tok);
	    if(dir_tokens[i][strlen(dir_tokens[i])] != '\0')
	    {
	      dir_tokens[i][strlen(dir_tokens[i])] = '\0';
	    }
	    temp_tok = strtok(NULL, "/");
	    i++;
	  }       
	  dir_cnt = i;
	  i=0;
	  int found=0;
	  int32_t TempDirPos;
	
          int32_t DirSecReadPos = RootDirFileReadPos * BPB_BytesPerSec; //initially setting to root dir location
	  int32_t TempParentDirFileReadPos = DirSecReadPos;   // setting parent directory to Root Directory
	  int j;

          while(i < dir_cnt)
          {
            fseek(fp, DirSecReadPos, SEEK_SET);
            fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp); 
            j=0;
            found = 0;
            while(found == 0 && j < 16)
            {
              if((dir[j].DIR_Attr == 0x10) && (compare_dirnames(dir[j].DIR_Name, dir_tokens[i]) == 1) && found == 0) // directory
              {
                //find the location of sector for the found directory
                TempParentDirFileReadPos = DirSecReadPos;   // setting parent directory to current directory
		DirSecReadPos = LBAtoOffset(dir[j].ClusterLow);
                TempDirPos = dir[j].ClusterLow;
                found = 1;
              }
              j++;
            } 
            if(found == 0)
            {
              // dir not found
              break;
            }
            i++;
          }
          if(found == 1 && i == dir_cnt)
          {
            CurrentDirFileReadPos = DirSecReadPos; // Current directory
	    ParentDirFileReadPos = TempParentDirFileReadPos;  // setting parent directory to current directory
          }
          else
          {
            printf("Unable to find directory\n");
          }
        }
        else if(token[1][0] == '.' && token[1][1] == '.') // look from parent dir
        {
	  char dir_tokens[MAX_DIRS][MAX_FILE_NAME];
	  int dir_cnt;
	  int i = 0;
	  
	  temp_tok = strtok(token[1], "/");
	  while (temp_tok != NULL)
	  {
	    strcpy(dir_tokens[i], temp_tok);
	    if (dir_tokens[i][strlen(dir_tokens[i])] != '\0')
	    {
	      dir_tokens[i][strlen(dir_tokens[i])] = '\0';
	    }
	    temp_tok = strtok(NULL, "/");
	    i++;
	  }
	  dir_cnt = i;
	  
	  int found = 0;
	  int32_t TempDirPos;

	  int32_t DirSecReadPos = ParentDirFileReadPos;  //setting to parent dir or current dir location
	  int32_t TempParentDirFileReadPos = DirSecReadPos;  // setting parent directory ro Root directory
	  if ((dir_cnt == 1) && (strcmp(dir_tokens[0], "..") == 0)) // only change dir to parent directory
	  {
	    ParentDirFileReadPos = TempParentDirFileReadPos;  // setting oarent dir or current dir location
	    CurrentDirFileReadPos = DirSecReadPos;   // Current directory
	    int j;
	    char temp_str[12];
	    
	    fseek(fp, DirSecReadPos, SEEK_SET);
	    fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
	    for (j = 0; j < 16; j++)
	    {
	      if (dir[j].DIR_Attr == 0x01 || dir[j].DIR_Attr == 0x10 || dir[j].DIR_Attr == 0x20)
	      {
                strncpy(temp_str, dir[j].DIR_Name, 11);
		temp_str[11] = '\0';
	      }
	    }
	  } 
          else
	  {
	    int j;
	    i = 1;
	    
            while (i < dir_cnt)
	    {
	      fseek(fp, DirSecReadPos, SEEK_SET);
	      fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
	      j = 0;
	      found = 0;
	      while (found == 0 && j < 16)
	      {
		if ((dir[j].DIR_Attr == 0x10) && (compare_dirnames(dir[j].DIR_Name, dir_tokens[i]) == 1) && found == 0) // directory
		{
		  TempParentDirFileReadPos = DirSecReadPos;    // setting parent directory ro current directory
		  DirSecReadPos = LBAtoOffset(dir[j].ClusterLow);
		  TempDirPos = dir[j].ClusterLow;
		  found = 1;
		}
		j++;
	      }
	      if (found == 0)
	      {
	        break;	
	      }
	      i++;
	    }
	    if (found == 1 && i == dir_cnt)
	    {
	      CurrentDirFileReadPos = DirSecReadPos;  // Current directory 
	      ParentDirFileReadPos = TempParentDirFileReadPos;	// Setting parent directory to current directory
	    }
	    else 
	    {
	      printf("Unable to find directory\n");
	    }
	  }    
        }
        else // look in current dir 
        {
	  char dir_tokens[MAX_DIRS][MAX_FILE_NAME];
	  int dir_cnt;
	  int i = 0;

	  temp_tok = strtok(token[1], "/");
	  while (temp_tok != NULL)
	  {
	    strcpy(dir_tokens[i], temp_tok);
	    if (dir_tokens[i][strlen(dir_tokens[i])] != '\0')
	    {
	      dir_tokens[i][strlen(dir_tokens[i])] != '\0';
	    }
	    temp_tok = strtok(NULL, "/");
	    i++;	
	  }
	  dir_cnt = i;
	  int found = 0;
	  int32_t TempDirPos;
	  int32_t DirSecReadPos = CurrentDirFileReadPos;
          int32_t TempParentDirFileReadPos = DirSecReadPos;

	  if ((dir_cnt ==1) && (strcmp(dir_tokens[0], ".") == 0)) 
	  {
	     // nothing to be done
	  }
	  else
	  {
	    int j;
	    if (strcmp(dir_tokens[0], ".") == 0)
	    {
	      i = 1;
	    }
            else 
	    {
	      i = 0;
	    }
	    while (i < dir_cnt)
	    {
	      fseek(fp, DirSecReadPos, SEEK_SET);
	      fread(&dir[0], sizeof(struct DirectoryEntry) , 16, fp);	
	      j = 0;
	      found = 0;
	      while (found == 0 && j < 16)
	      {	
                if((dir[j].DIR_Attr == 0x10) && (compare_dirnames(dir[j].DIR_Name, dir_tokens[i]) == 1) && found == 0)
		{      	
		  TempParentDirFileReadPos = DirSecReadPos;
		  DirSecReadPos = LBAtoOffset(dir[j].ClusterLow);
		  TempDirPos = dir[j].ClusterLow;
		  found = 1;
		}
		j++;
	      }
	      if (found == 0)
	      { 
	        // dir not found
		break;
	      }
	      i++;
	    }	
	    if (found == 1 && i == dir_cnt)
	    {
	      CurrentDirFileReadPos = DirSecReadPos;  // CUrrent DIrectory
	      ParentDirFileReadPos = TempParentDirFileReadPos;    // setting parent directory to current directory
	    }
	    else 
	    {
	      printf("Unable to find directory\n");
	    }
	  }
        }
      }
      else if(strcmp(token[0], "stat") == 0)
      {
	if(!fp) 
	{
	  printf("Error: File system image must be opened first\n");
	  continue;
	}
	if (open_flag == 1) 
	{
	  if (token[1] == NULL) 
	  {
	    printf("Error: Input file name is missing\n");
	    continue;
	  }
	  else 
	  {
	    fseek(fp, CurrentDirFileReadPos, SEEK_SET);
	    fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
	    char temp_str[12];
	    int i = 0;
	    int found_ctr = 0;
	    char filename_copy[12];
	    for (i = 0; i < 16; i++)
	    {   
	      strncpy(temp_str, dir[i].DIR_Name, 11);
	      temp_str[11] = '\0';

	      if ((compare_dirnames(temp_str, token[1]) == 1) || (compare_filenames(temp_str, token[1]) == 1))
	      {
	        if(dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20)
		{
		  strncpy(temp_str, dir[i].DIR_Name, 11);
		  temp_str[11] = '\0';
		    
		  printf("Starting Cluster number: %d\n", dir[i].ClusterLow);
		  printf("Size = %d\n", dir[i].size);
	 	  printf("Attribute = 0x%x\n", dir[i].DIR_Attr);
		  found_ctr++;
                }
              }
	    }
	    if (found_ctr == 0)
	    {
	      printf("Error: File not found\n");
	    }
	  }
	}
	/*else
	{
          printf("Error: File system image must be opened first\n");
	}*/
      }
      else if(strcmp(token[0], "ls") == 0)
      {   
        if (!fp) 
	{
	  printf("Error: File system image must be opened first.\n");
	  continue;
	}   
        fseek(fp, CurrentDirFileReadPos, SEEK_SET);
        fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp); 
        int i, offset;
        int16_t nextLB;
        char temp_str[12];
        char str[12];
        for(i=0; i < 16; i++)
        {
          if((dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20) && (dir[i].DIR_Name[0] != 0xffffffe5)) // file or directory
          {
            strncpy(temp_str, dir[i].DIR_Name, 11);
            temp_str[11] = '\0';
            printf("%s\n", temp_str);
          }
        }
      }
      else if(strcmp(token[0], "read") == 0)
      {
        if(fp == NULL)
  	{
          printf("Error: File System image must be opened first.\n");
  	}
	if (token[1] == NULL || token[2] == NULL || token[3] == NULL) {
  	  if(token[1] == NULL)
  	  {
  	    printf("Error: No filename is entered to be read.\n");
  	  }
	  else if (token[2] == NULL) 
	  {
	    printf("Error: Position is not entered.\n");
	  }
	  else if (token[3] == NULL) 
	  {
	    printf("Error: Number of bytes is not entered.\n");
	  }
	}
  	else
  	{
	  int position = atoi(token[2]);
	  int noOfBytes = atoi(token[3]);
          fseek(fp, CurrentDirFileReadPos, SEEK_SET);
          fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp); 
          int i;
          int16_t nextLB;
          char temp_str[12];
	  int size_left;
	  int size_read;
	  int ClusSize;
	  int found = 0;
          char *str=NULL;
          int start_cluster;
          int remaining_bytes;
	  int match_i;
	  for(i=0; i < 16; i++)
	  {
	    if(((dir[i].DIR_Attr == 0x01) || (dir[i].DIR_Attr == 0x20)) && (dir[i].DIR_Name[0] != 0xffffffe5) && dir[i].size > 0)
	    {
	      strncpy(temp_str, dir[i].DIR_Name, 11);
	      temp_str[11] = '\0';
	      if (compare_filenames(temp_str, token[1]) == 1)     // to find the file that matches
	      {      
	        //printf("name : %s\n", temp_str);

          if((position+noOfBytes) > dir[i].size)
          {
            printf("Error: File size is smaller than requested read size\n");
          }

          dir[i].size = position + noOfBytes;

          size_read = 0;
          size_left = dir[i].size - size_read;
          ClusSize = BPB_SecPerClus*BPB_BytesPerSec;
          start_cluster  = position/ClusSize;

          //skip so many clusters
          int k;
          int offset;
          int bytes_to_write;
          str = malloc(ClusSize);
          FILE *fp_w=NULL;
          fp_w = fopen(token[1], "w");
          nextLB = dir[i].ClusterLow;
          for(k=0; k<start_cluster; k++)
          {
  	        nextLB = NextLB(nextLB);
          }
          if(nextLB != -1) //write remining bytes
          {
            remaining_bytes = position - (start_cluster * ClusSize);
	          offset = LBAtoOffset(nextLB) + remaining_bytes;

            bytes_to_write = ClusSize - remaining_bytes;
            fseek(fp, offset, SEEK_SET);
            fread(str, bytes_to_write, 1, fp);
            fwrite(str, bytes_to_write, 1, fp_w);
  	        nextLB = NextLB(nextLB);
            while(nextLB != -1)
            {
              offset = LBAtoOffset(nextLB);
              fseek(fp, offset, SEEK_SET);
	            if(size_left <= ClusSize)
	            {
                  fread(str, size_left, 1, fp); 
                  fwrite(str, size_left, 1, fp_w);
		              size_read = size_read +size_left;
		              size_left = dir[i].size - size_read;
		          }
		          else 
		          {
		             fread(str, ClusSize, 1, fp);
		             fwrite(str, ClusSize, 1, fp_w);
		             size_read = size_read + ClusSize;
		             size_left = dir[i].size - size_read;
		          }
	           	nextLB = NextLB(nextLB);
            }
             fclose(fp_w);
             printf("File [%s] found and bytes retrieved\n", token[1]);
             found = 1;
          }
        }
      }
     }
     if (found == 0)
     {
       printf("Error: File not found\n");
     }
    }
   }
      else if(strcmp(token[0], "del") == 0) 
      { 
        if (!fp)
	{
	  printf("Enter: File system image must be opened first\n");
	  continue;
        }
        if (open_flag == 1) 
	{
	  if (token[1] == NULL) 
	  {
	    printf("Error: Input file name is missing\n");
	    continue;
	  }
	  else 
	  {
	    fseek(fp, CurrentDirFileReadPos, SEEK_SET);
	    fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
	    int i = 0;
	    char temp_str[12];
	    int new_pos = 0;
	    int del_flag = 0; 
	    for (i = 0; i < 16; i++)
	    {
	      strncpy(temp_str, dir[i].DIR_Name, 11);
	      temp_str[11] = '\0';
	      if (compare_filenames(temp_str, token[1]) == 1) 
	      {
	        new_pos = CurrentDirFileReadPos + (i * sizeof(struct DirectoryEntry));
	        del_flag = 1;
	      }	
	    }
            if (del_flag == 1)
	    {
	      int fLetter = 0xffffffe5;
	      fseek(fp, new_pos , SEEK_SET);
	      fwrite(&fLetter, 1, 1, fp);
	    }
            else
            {
	      printf("Error: File not found\n");
            }
          }
        } 	
      }
      else if (strcmp(token[0], "undel") ==0) 
      {
        if (!fp) 
	{
	  printf("Error: File system image must be opened first\n");
	  continue;
	}
	if (open_flag == 1) 
	{
	  if (token[1] == NULL) 
	  {
	    printf("Error: Input file name is missing\n");
	    continue;
	  }
	  else
	  {
	    fseek(fp, CurrentDirFileReadPos, SEEK_SET);
	    fread(&dir[0], sizeof(struct DirectoryEntry), 16, fp);
	    int i = 0;
	    char temp_str[12];
	    int new_pos = 0;
	    int undel_flag = 0;
	    char undel_str[12];
	    char token_copy[12];
	    strcpy(undel_str, token[1]);       // copy of token[1] ->undel_str ->filename to be undeleted
	    strcpy(token_copy, token[1]);        // copy of token[1] -> token_copy -> filename to be undeleted
	    for (i = 0; i < 16; i++) 
	    {
              strncpy(temp_str, dir[i].DIR_Name, 11);       //  name from directory -> temp_str
	      temp_str[11] = '\0';           // temp_str (has ?)
	      if ((compare_filenames_undel(&temp_str[1], &undel_str[1]) == 1) && (temp_str[0] == 0xffffffe5))
	      { // Compares the name from directory (temp_str[1]) with the copy of token[1]  and checks if the f
	        new_pos = CurrentDirFileReadPos + (i * sizeof(struct DirectoryEntry));
	        undel_flag = 1;
              }
            }		    
	    if(undel_flag == 1)
	    {
              fseek(fp, new_pos, SEEK_SET);
              char upperToken = toupper(token_copy[0]);
              fwrite(&upperToken, 1, 1, fp);
            }
            else 
	    {
	      printf("Error: File not found\n");
	    }
	  }		
        }
      }		    
    }

     //free up allocated memory for the token string array
    int cnt1;
    for(cnt1=0; cnt1<MAX_NUM_ARGUMENTS; cnt1++)
    {
      if(token[cnt1] != NULL)
      {
        free(token[cnt1]);
      }
    }
  }

  if(fp)
  {
    fclose(fp);
  }
  free(command_string);
  return 0;
}

