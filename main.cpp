#include "server.h"

int main(int argc, char **argv)
{
  LLVMServer server("127.0.0.1", 4000);
  return server.start();
}
