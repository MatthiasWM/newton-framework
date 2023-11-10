
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

extern "C" void InitObjectSystem(void);

extern void **RSSYMviewer;

void hex16(void *src) {
  int n = 16;
  uint8_t *s = (uint8_t*)src;
  printf("%016x: ", s);
  for (int i=0; i<n; i++) {
    printf("%02x ", s[i]);
    if (i==7) printf(" ");
  }
  printf(" ");
  for (int i=0; i<n; i++) {
    uint8_t c = s[i];
    if (c<32 || c>126) c = '.';
    printf("%c", c);
  }
  printf("\n");
}

void hex(void *src, int n) {
  for (int i=0; i<n; i+=16) {
    hex16(((uint8_t*)src) + i);
  }
}

int main(int argc, char **argv)
{
  void *p = *RSSYMviewer;

  hex( ((uint8_t*)p)-1, 64);

  InitObjectSystem();
  printf("Hello world\n");

}
