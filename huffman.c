#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

#define FILE_BUFFER_SIZE 1024

typedef unsigned char u8;

typedef struct bitSequence bitSequence;
struct bitSequence {
  u8 * bits;
  size_t num;
};

bitSequence * bsCreate(size_t n) {
  bitSequence * b = malloc(sizeof(bitSequence));
  b->bits = malloc(n);
  b->num = n;
  return b;
}
void bsDelete(bitSequence * b) {
  free(b->bits);
  free(b);
}
void bsAppend(bitSequence * b, u8 bit) {
  u8 * newBits = malloc(b->num+1);
  memcpy(newBits, b->bits, b->num);
  free(b->bits);
  b->bits = newBits;
  b->bits[b->num] = bit;
  b->num++;
}
void bsPrepend(bitSequence * b, u8 bit) {
  u8 * newBits = malloc(b->num+1);
  memcpy(newBits+1, b->bits, b->num);
  free(b->bits);
  b->bits = newBits;
  b->bits[0] = bit;
  b->num++;
}


typedef struct huffmanTree huffmanTree;
struct huffmanTree {
  u8 value;
  long frequency;
  huffmanTree * left;
  huffmanTree * right;
};
int htEncode(huffmanTree * t, u8 c, bitSequence * bits);
u8 htDecode(huffmanTree * t, bitSequence * b);
void htPrint(huffmanTree * t, int depth);
int htCompare(const void * a, const void * b); 
huffmanTree * htCreate(u8* values, unsigned int * frequencies, unsigned short n);
void htDelete(huffmanTree * t);

int htEncode(huffmanTree * t, u8 c, bitSequence * bits) {
  if(t == NULL) return 0;
  if(t->right == NULL && t->left ==NULL && t->value == c) return 1;
  if(htEncode(t->right, c, bits)) {
    bsPrepend(bits, 1);
    return 1;
  }
  else if(htEncode(t->left, c, bits)) {
    bsPrepend(bits, 0);
    return 1;
  }
  else {
    return 0;
  }
}
u8 htDecode(huffmanTree * t, bitSequence * b) {
  huffmanTree * curNode = t;
  for(size_t i = 0; i < b->num; i++) {
    if(curNode == NULL) {
      //stop, drop, and scream
      printf("you done messed up");
      exit(1);
    }
    if(b->bits[i]) {
      curNode = curNode->right;
    }
    else curNode = curNode->left;
  }
  return curNode->value;
}
void htPrint(huffmanTree * t, int depth) {
  if(t == NULL) {
    //printf("%*s%s\n", depth*5, "", "NULL");
    return;
  }
  htPrint(t->right, depth+1);
  printf("%*s{%c, %d}\n", depth*5, "", t->value, t->frequency);
  htPrint(t->left, depth+1);
}
huffmanTree * htCreate(u8 * values, unsigned int * frequencies, unsigned short n) {//does not need to be sorted, but order matters in the event two characters share same frequency
  huffmanTree ** partialTrees = malloc(sizeof(huffmanTree*) * n);
  for(int i = 0; i < n; i++) {
    huffmanTree * leaf = malloc(sizeof(huffmanTree));
    *leaf = (huffmanTree){
      values[i],
      frequencies[i],
      NULL,
      NULL
    };
    partialTrees[i] = leaf;
  }
  while(1) {
    int count = 0;
    int leastFreq = -1;
    int secondLeastFreq = -1;
    for(int i = 0; i < n; i++) {
      if(partialTrees[i] != NULL) {
        if(leastFreq == -1 || partialTrees[i]->frequency < partialTrees[leastFreq]->frequency) {
          secondLeastFreq = leastFreq;
          leastFreq = i;
        }
        else if(secondLeastFreq == -1 || partialTrees[i]->frequency < partialTrees[secondLeastFreq]->frequency) {
          secondLeastFreq = i;
        }
        count++;
      }
    }
    if(count == 1) {
      huffmanTree * retval = partialTrees[leastFreq];
      free(partialTrees);//all nodes still exist, this just removes extraneous references
      return retval;//if count > 0, leastFreq points to a valid huffmanTree
    }
    huffmanTree * internalTree = malloc(sizeof(huffmanTree));
    internalTree->value = '\0';
    internalTree->frequency = partialTrees[leastFreq]->frequency + partialTrees[secondLeastFreq]->frequency;
    internalTree->right = partialTrees[secondLeastFreq];
    internalTree->left = partialTrees[leastFreq];
    partialTrees[leastFreq] = NULL;
    partialTrees[secondLeastFreq] = internalTree;
  }
}
void htDelete(huffmanTree * t) {
  if(t->left != NULL) {
    htDelete(t->left);
  }
  if(t->right != NULL) {
    htDelete(t->right);
  }
  free(t);
}


int encode(const char * inputFile, const char * outputFile) {
  FILE * fInput = fopen(inputFile, "r");
  FILE * fOutput = fopen(outputFile, "w");
  if(fInput == NULL) {
    printf("file '%s' not found\n", inputFile);
    exit(1);
  }
  if(fOutput == NULL) {
    printf("file '%s' not found\n", outputFile);
    exit(1);
  }
  u8 ifBuf[FILE_BUFFER_SIZE];
  unsigned int freqs[256] = {0};
  long numBytes = 0;
  while(1) {
    int numRead = fread(ifBuf, sizeof(u8), FILE_BUFFER_SIZE, fInput);
    for(int i = 0; i < numRead; i++) {
      freqs[ifBuf[i]]++;
      if(freqs[ifBuf[i]] > UINT_MAX/2) {
        for(int k = 0; k < 256; k++) {
          freqs[k] = freqs[k]/2;
        }
      }
    }
    numBytes += numRead;
    if(numRead != FILE_BUFFER_SIZE) {
      if(ferror(fInput)) {
        printf("file read error!");
        exit(1);
      }
      break;
    }
  }
  u8 vals[256] = {'\0'};
  unsigned int shortenedFreqs[256] = {0};
  unsigned short freqCount = 0;
  for(int i = 0; i < 256; i++) {
    if(freqs[i] > 0) {
      vals[freqCount] = i;
      shortenedFreqs[freqCount] = freqs[i];
      freqCount++;
    }
  }
  //write metadata to output file
  fwrite(&numBytes, sizeof(long), 1, fOutput);
  fwrite(&freqCount, sizeof(unsigned short), 1, fOutput);
  fwrite(vals, sizeof(u8), freqCount, fOutput);
  fwrite(shortenedFreqs, sizeof(unsigned int), freqCount, fOutput);

  huffmanTree * t = htCreate(vals, shortenedFreqs, freqCount);
  //htPrint(t, 0);

  fseek(fInput, 0, SEEK_SET);
  u8 runningByte = '\0';
  int curBit = 0;
  long bytesWritten = 0;
  while(1) {
    int numRead = fread(ifBuf, sizeof(u8), FILE_BUFFER_SIZE, fInput);
    for(int i = 0; i < numRead; i++) {
      bitSequence * bitSeq = bsCreate(0);
      htEncode(t, ifBuf[i], bitSeq);
      for(int k = 0; k < bitSeq->num; k++) {
        runningByte = runningByte | (bitSeq->bits[k] << (7-curBit));
        curBit++;
        if(curBit == 8) {
          curBit = 0;
          //printf("runningByte = %hhX\n", runningByte);
          fputc(runningByte, fOutput);
          runningByte = '\0';
          bytesWritten++;
        }
      }

      /*
      printf("bitSeq for char '%c': ", ifBuf[i]);
      for(int i = 0; i < bitSeq->num; i++) {
        printf("%d ", bitSeq->bits[i]);
      }
      puts("");
      */
    }
    if(numRead != FILE_BUFFER_SIZE) {
      if(ferror(fInput)) {
        printf("File read error!");
        exit(1);
      }
      break;
    }
  }
  if(curBit != 0) {//half finished byte
    fputc(runningByte, fOutput);
    bytesWritten++;
  }
  printf("Read %ld bytes, wrote %ld bytes\n", numBytes, bytesWritten);
  fclose(fOutput);
  fclose(fInput);
}

int decode(const char * inputFile, const char * outputFile) {
  FILE * fInput = fopen(inputFile, "r");
  FILE * fOutput = fopen(outputFile, "w");
  long numBytes;
  unsigned short numFreq;
  u8 vals[256];
  unsigned int freqs[256];
  fread(&numBytes, sizeof(long), 1, fInput);
  fread(&numFreq, sizeof(unsigned short), 1, fInput);
  fread(vals, sizeof(u8), numFreq, fInput);
  fread(freqs, sizeof(unsigned int), numFreq, fInput);

  huffmanTree * t = htCreate(vals, freqs, numFreq);

  //htPrint(t, 0);
  
  int curByte = fgetc(fInput);
  int curBit = 0;
  huffmanTree * curTree = t;
  long decodedBytes = 0;
  long bytesRead = 0;
  while(curByte != EOF) {
    if(curTree == NULL) {
      printf("corrupt input file!");
      exit(1);
    }
    if(curBit == 8) {
      curBit = 0;
      curByte = fgetc(fInput);
      bytesRead++;
    }
    if(curTree->left == NULL || curTree->right == NULL) {
      decodedBytes++;
      //printf("%c", curTree->value);
      fputc(curTree->value, fOutput);
      curTree = t;
      if(decodedBytes == numBytes) break;
    }
    if(curByte & (1<<(7-curBit))) {
      curTree = curTree->right;
    }
    else {
      curTree = curTree->left;
    }
    curBit++;
    if(curByte == EOF) {
      printf("EOF REACHED");
    }
  }
  printf("Read %ld bytes, wrote %ld bytes\n", bytesRead, numBytes);
}

void quitBadInput(const char * argv0) {
  printf("usage: %s encode|decode <input file> <output file>\n", argv0);
  exit(0);
}


int main(int argc, char* argv[]) {
  /*
  srand(time(NULL));
  int frequencies[96];
  char values[96];
  for(int i = 0; i < 96; i++) {
    frequencies[i] = rand()%1000 + 1;
    values[i] = i+32;
  }
  huffmanTree * t1 = htCreate(values, frequencies, 96);
  htPrint(t1, 0);
  for(int k = 0; k < 96; k++) {
    bitSequence * bits = bsCreate(0);
    htEncode(t1, 32+k, bits);
    printf("char '%c' has bits: ", 32+k);
    for(int i = 0; i < bits->num; i++) {
      printf("%d ", bits->bits[i]);
    }
    puts("");
    bsDelete(bits);
  }
  exit(1);
  */

  if(argc < 4 || argc > 4) {
    quitBadInput(argv[0]);
  }
  if(!strcmp(argv[2], argv[3])) {
    printf("input file cannot be output file");
    exit(0);
  }
  if(!strcmp(argv[1], "encode")) {
    encode(argv[2], argv[3]);
  }
  else if(!strcmp(argv[1], "decode")) {
    decode(argv[2], argv[3]);
  }
  else quitBadInput(argv[0]);

}
