#include <string>
#include <boost/asio.hpp>

using namespace std;
using namespace boost;
using boost::asio::ip::tcp;

class LLVMServer {
  string bind_address_;
  int port_;
  asio::io_context io_context_;
  tcp::acceptor acceptor_;

public:
  LLVMServer(const char *bind_address, int port);
  int start();
};
