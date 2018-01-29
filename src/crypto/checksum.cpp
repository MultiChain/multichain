// Copyright 2018 RecordsKeeper
// Code to calculate the Checksum

#include <stdlib.h>
#include <stdio.h>

unsigned checksum(void *buffer, size_t len, unsigned int seed)
{
      unsigned char *buf = (unsigned char *)buffer;
      size_t i;

      for (i = 0; i < len; ++i)
            seed += (unsigned int)(*buf++);
      return seed;
}

unsigned calculateFileChecksum(char *file) {

      FILE *fp;
      size_t len;
      char buf[4096];

      if (NULL == (fp = fopen(file, "rb")))
      {
            printf("Unable to open %s for reading\n", file);
            return -1;
      }
      len = fread(buf, sizeof(char), sizeof(buf), fp);
      return checksum(buf, len, 0));
}