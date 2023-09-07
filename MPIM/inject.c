#include <stdio.h>



ssize_t real_read(int fd, void *data, size_t size) {
  return ((real_read_t)dlsym(RTLD_NEXT, "read"))(fd, data, size);
}

__attribute__((constructor))
static void customConstructor(int argc, const char **argv)
 {
     printf("Hello from dylib!\n");
     printf("Dylib injection successful in %s\n", argv[0]);
}
