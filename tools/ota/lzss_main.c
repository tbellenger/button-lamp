/* Tiny CLI around lzss.c encode_file / decode_file. */
#include <stdio.h>
#include <string.h>

int encode_file(char const* in, char const* out);
int decode_file(char const* in, char const* out);

int main(int argc, char** argv) {
  if (argc != 4) {
    fprintf(stderr, "usage: lzss --encode|--decode infile outfile\n");
    return 2;
  }
  int rc;
  if (strcmp(argv[1], "--encode") == 0) rc = encode_file(argv[2], argv[3]);
  else if (strcmp(argv[1], "--decode") == 0) rc = decode_file(argv[2], argv[3]);
  else {
    fprintf(stderr, "mode must be --encode or --decode\n");
    return 2;
  }
  return rc == 0 ? 0 : 1;
}
