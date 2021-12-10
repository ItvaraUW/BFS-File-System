// ============================================================================
// fs.c - user FileSytem API
// ============================================================================

#include "bfs.h"
#include "fs.h"

// ============================================================================
// Close the file currently open on file descriptor 'fd'.
// ============================================================================
i32 fsClose(i32 fd) { 
  i32 inum = bfsFdToInum(fd);
  bfsDerefOFT(inum);
  return 0; 
}



// ============================================================================
// Create the file called 'fname'.  Overwrite, if it already exsists.
// On success, return its file descriptor.  On failure, EFNF
// ============================================================================
i32 fsCreate(str fname) {
  i32 inum = bfsCreateFile(fname);
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}



// ============================================================================
// Format the BFS disk by initializing the SuperBlock, Inodes, Directory and 
// Freelist.  On succes, return 0.  On failure, abort
// ============================================================================
i32 fsFormat() {
  FILE* fp = fopen(BFSDISK, "w+b");
  if (fp == NULL) FATAL(EDISKCREATE);

  i32 ret = bfsInitSuper(fp);               // initialize Super block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitInodes(fp);                  // initialize Inodes block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitDir(fp);                     // initialize Dir block
  if (ret != 0) { fclose(fp); FATAL(ret); }

  ret = bfsInitFreeList();                  // initialize Freelist
  if (ret != 0) { fclose(fp); FATAL(ret); }

  fclose(fp);
  return 0;
}


// ============================================================================
// Mount the BFS disk.  It must already exist
// ============================================================================
i32 fsMount() {
  FILE* fp = fopen(BFSDISK, "rb");
  if (fp == NULL) FATAL(ENODISK);           // BFSDISK not found
  fclose(fp);
  return 0;
}



// ============================================================================
// Open the existing file called 'fname'.  On success, return its file 
// descriptor.  On failure, return EFNF
// ============================================================================
i32 fsOpen(str fname) {
  i32 inum = bfsLookupFile(fname);        // lookup 'fname' in Directory
  if (inum == EFNF) return EFNF;
  return bfsInumToFd(inum);
}

// ============================================================================
// Read 'numb' bytes of data from the cursor in the file currently fsOpen'd on
// File Descriptor 'fd' into 'buf'.  On success, return actual number of bytes
// read (may be less than 'numb' if we hit EOF).  On failure, abort
// ============================================================================
i32 fsRead(i32 fd, i32 numb, void* buf) {
  // Make temp buffer
  i8 tempBuf[512];
  // tempNumb is used to keep track of remaining bytes to read
  i32 tempNumb = numb;
  // variables used to find the cursor
  i32 inum = bfsFdToInum(fd);
  i32 indexBlock = fsTell(fd)/512;
  // used to account for cursor
  i32 remainderRemaining = fsTell(fd)%512;
  // byteOffset is used to traverse the buf buffer
  i32 byteOffset = 0;
  // StartingByte is used to check if read is going to EoF
  i32 startingByte = fsTell(fd);
  //printf("Starting Byte: %d\n", startingByte);
  //printf("File Size of fd: %d\n", fsSize(fd));
  while (tempNumb > 0) // grab another block
  {
    //printf("Entering while loop\n");
    bfsRead(inum, indexBlock, tempBuf); // Gets us one block
    if (remainderRemaining > 0)
    {
      //printf("Entering remainder\n");
      // Ignore front buffer by remainder only first time
      i32 numLeft = 512 - remainderRemaining; // Bytes left in tempBuffer
      if (tempNumb > numLeft)
      {
        tempNumb -= numLeft;
      }
      else // tempNumb < numLeft
      {
        numLeft = tempNumb;
        tempNumb = 0;
      }
      // Copy from tempBuf to realBuf
      memcpy(buf + byteOffset, tempBuf + remainderRemaining, numLeft);
      byteOffset += numLeft;
      remainderRemaining = 0;
      fsSeek(fd, numLeft, SEEK_CUR);
    }
    else
    {
      //printf("Entering else [Two possible]\n");
      if (tempNumb > 512)
      {
        //printf("512 Loop\n");
        memcpy(buf + byteOffset, tempBuf, 512);
        tempNumb -= 512;
        byteOffset += 512;
        fsSeek(fd, 512, SEEK_CUR);
      }
      else //final loop
      {
        //printf("Entering Final Loop\n");
        memcpy(buf + byteOffset, tempBuf, tempNumb);
        //printf("tempNumb is %d\n", tempNumb);
        fsSeek(fd, tempNumb, SEEK_CUR);
        tempNumb = 0;
      }
    }
    // adjust index if there are still bytes to be read
    if (tempNumb != 0)
    {
      indexBlock++;
    }
  } 
  // Test case if we are at EoF
  if (startingByte + numb > fsSize(fd))
  {
    //printf("Returning: %d\n", fsSize(fd) - startingByte);
    // Move cursor back if we already passed EoF
    fsSeek(fd, fsSize(fd), SEEK_SET);
    // Return how much we were able to read
    return fsSize(fd) - startingByte;
  }
  //printf("Returning: %d\n", numb);
  // Return numb in most cases that do not deal with EoF.
  return numb;
}


// ============================================================================
// Move the cursor for the file currently open on File Descriptor 'fd' to the
// byte-offset 'offset'.  'whence' can be any of:
//
//  SEEK_SET : set cursor to 'offset'
//  SEEK_CUR : add 'offset' to the current cursor
//  SEEK_END : add 'offset' to the size of the file
//
// On success, return 0.  On failure, abort
// ============================================================================
i32 fsSeek(i32 fd, i32 offset, i32 whence) {

  if (offset < 0) FATAL(EBADCURS);
 
  i32 inum = bfsFdToInum(fd);
  i32 ofte = bfsFindOFTE(inum);
  
  switch(whence) {
    case SEEK_SET:
      g_oft[ofte].curs = offset;
      break;
    case SEEK_CUR:
      g_oft[ofte].curs += offset;
      break;
    case SEEK_END: {
        i32 end = fsSize(fd);
        g_oft[ofte].curs = end + offset;
        break;
      }
    default:
        FATAL(EBADWHENCE);
  }
  return 0;
}



// ============================================================================
// Return the cursor position for the file open on File Descriptor 'fd'
// ============================================================================
i32 fsTell(i32 fd) {
  return bfsTell(fd);
}



// ============================================================================
// Retrieve the current file size in bytes.  This depends on the highest offset
// written to the file, or the highest offset set with the fsSeek function.  On
// success, return the file size.  On failure, abort
// ============================================================================
i32 fsSize(i32 fd) {
  i32 inum = bfsFdToInum(fd);
  return bfsGetSize(inum);
}



// ============================================================================
// Write 'numb' bytes of data from 'buf' into the file currently fsOpen'd on
// filedescriptor 'fd'.  The write starts at the current file offset for the
// destination file.  On success, return 0.  On failure, abort
// ============================================================================
i32 fsWrite(i32 fd, i32 numb, void* buf) {
  // Temporary buffer used to read in from file, then write back into file
  i8 tempBuf[512];
  // tempNumb is used to track how many bytes left to write
  i32 tempNumb = numb;
  // Find the block we need to write to
  i32 inum = bfsFdToInum(fd);
  i32 indexBlock = fsTell(fd)/512;
  // Find where the cursor is, how far into the block, [RemainderRemaining]
  i32 remainderRemaining = fsTell(fd)%512;
  // byteOffset is used to keep track of which bytes we are writing into the file
  i32 byteOffset = 0;
  // blocksExtra is used to check how many extra blocks are required if extending
  i32 blocksExtra = -1;

  // Check if new blocks need to be allocated (overlap, outside)
  // If the cursor + how much we want to write exeeds our current file size, extend.
  if (fsTell(fd) + numb > fsSize(fd))
  {
    if (remainderRemaining > 0)
    {
      i32 checkNumLeft = 512 - remainderRemaining;
      tempNumb -= checkNumLeft;
      // Count partial block as one of the blocks
      blocksExtra++;
    }

    // Count each full block
    blocksExtra += (tempNumb / 512);

    if ((tempNumb % 512) != 0)
    {
      // Count the leftover
      blocksExtra++;
    }

    if (blocksExtra > 0)
    {
      tempNumb = numb - (blocksExtra * 512);
      //printf("indexBlock is: %d\n", indexBlock);
      //printf("blocksExtra is: %d\n", blocksExtra);
      //printf("Pre-Extend Size: %d\n", fsSize(fd));
      bfsExtend(inum, indexBlock + blocksExtra);
      // Accurately set size based on how much is extended
      bfsSetSize(inum, fsSize(fd) + tempNumb);
      //printf("Post-Extend Size: %d\n", fsSize(fd));
    }
  }
  
  // Reset tempNumb to be used later
  tempNumb = numb;

  while (tempNumb > 0) // While we have more things to write
  {
    if (remainderRemaining > 0) // Not starting at beginning of block
    {
      i32 numLeft = 512 - remainderRemaining; // Bytes left in tempBuffer
      //update tempNumb
      if (tempNumb > numLeft)
      {
        tempNumb -= numLeft;
      }
      else // tempNumb < numLeft
      {
        numLeft = tempNumb;
        tempNumb = 0;
      }
      //bioRead into temp buffer
      i32 dbn = bfsFbnToDbn(inum, indexBlock);
      bioRead(dbn, tempBuf);

      // write to temp buffer starting at (remainder)
      // Memcpy (to, from, amount)
      memcpy(tempBuf + remainderRemaining, buf + byteOffset, numLeft);
      byteOffset += numLeft;
      // set remainder to zero
      remainderRemaining = 0;

      // bioWrite back that block
      bioWrite(dbn, tempBuf);

      // Update Cursor
      fsSeek(fd, numLeft, SEEK_CUR);
    }
    else // Main case - Write at beginning of block
    {
      // bioRead into temp buffer
      i32 dbn = bfsFbnToDbn(inum, indexBlock);
      bioRead(dbn, tempBuf);

      // TWO CASES:
      if (tempNumb > 512) // more loops
      {
        // write entire block full of bytes from buffer
        memcpy(tempBuf, buf + byteOffset, 512);
        // update tempNumb
        tempNumb -= 512;
        byteOffset += 512;

        // put block back
        bioWrite(dbn, tempBuf);

        //Update Cursor
        fsSeek(fd, 512, SEEK_CUR);
      }
      else //[tempNumb < 512] Last loop
      {
        // write tempNumb of bytes from buf to tempBuf
        memcpy(tempBuf, buf + byteOffset, tempNumb);

        // Put block back
        bioWrite(dbn, tempBuf);

        //Update Cursor
        fsSeek(fd, tempNumb, SEEK_CUR);

        // update tempNumb (to zero since we have nothing left)
        tempNumb = 0;
      }
    }
    // adjust index if there are still bytes to be read
    if (tempNumb != 0)
    {
      indexBlock++;
    }
  }
  return 0;
}