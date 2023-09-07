#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef ssize_t (*real_read_t)(int, void *, size_t);

ssize_t real_read(int fd, void *data, size_t size) {
  return ((real_read_t)dlsym(RTLD_NEXT, "read"))(fd, data, size);
}

void foo()
{
    char temp[1024] = "120123";
    // scanf("%s", temp);
    // snprintf(temp, sizeof(temp), "data-%d\n", 123);
    size_t tlen = strnlen(temp, sizeof(temp));
    real_read_t aa = NULL;
    aa = (real_read_t)dlsym(RTLD_NEXT, "snprintf");
    printf("Hello from foo %zu %p\n", tlen, aa);
}




int main()
{
    printf("Hello from main!\n");
    foo();
}

