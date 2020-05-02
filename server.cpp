#include <iostream>
#include <boost/algorithm/string.hpp>

#include "server.h"
#include "compiler.h"

using namespace std;

class LLVMServerSession {
  tcp::socket &socket_;
  CompileContext cc_;
  bool running_;
  ByteArray request_payload_;

  void read_payload(size_t total_size) {
    size_t consumed_size = request_payload_.size();
    if (request_payload_.capacity() <= total_size) {
      // ensure there is room for a terminating zero
      //
      // contrary to expectations, .resize() changes the capacity of
      // the vector, not the size
      request_payload_.resize(total_size + 1);
    }
    size_t remaining_size = total_size - consumed_size;
    if (remaining_size > 0) {
      request_payload_.set_size(total_size); // this changes the size
      asio::read(socket_,
                 asio::buffer(request_payload_.begin() + consumed_size,
                              remaining_size),
                 asio::transfer_exactly(remaining_size));
    }
    // add invisible terminating zero
    // (needed by LLLexer::getNextChar())
    *(request_payload_.begin() + total_size) = 0;
    // note that .size() does not count the terminating zero
  }

  void write_payload(const ByteArray &payload) {
    asio::write(socket_, asio::buffer(payload.begin(), payload.size()));
  }

  void write_ok_response() {
    string response = "OK 0\n";
    asio::write(socket_, asio::buffer(response));
  }

  void write_ok_response(const ByteArray &payload) {
    string response = "OK ";
    response += to_string(payload.size());
    response += "\n";
    asio::write(socket_, asio::buffer(response));
    write_payload(payload);
  }

  void write_error_response(const char *error_message) {
    int len = strlen(error_message);
    string response = "ERROR ";
    response += to_string(len);
    response += "\n";
    asio::write(socket_, asio::buffer(response));
    if (len > 0) {
      ByteArray payload(error_message, error_message + len);
      if (payload.back() != '\n') {
        payload.push_back('\n');
      }
      write_payload(payload);
    }
  }

  size_t process_request(const string &first_line) {
    vector<string> words;
    split(words, first_line, is_any_of(" \t"), token_compress_on);
    string command = words[0];
    to_lower(command);
    if (command == "parse") {
      size_t payload_size = stoi(words[1]);
      cerr << "PARSE " << payload_size << endl;
      read_payload(payload_size);
      cc_.parse(request_payload_);
      write_ok_response();
      return payload_size;
    } else if (command == "opt") {
      cerr << "OPT" << endl;
      cc_.opt();
      write_ok_response();
      return 0;
    }
    else if (command == "dump") {
      cerr << "DUMP" << endl;
      ByteArray payload = cc_.dump();
      write_ok_response(payload);
      return 0;
    }
    else if (command == "link") {
      cerr << "LINK" << endl;
      cc_.link();
      write_ok_response();
      return 0;
    }
    else if (command == "commit") {
      cerr << "COMMIT" << endl;
      cc_.commit();
      write_ok_response();
      return 0;
    }
    else if (command == "call") {
      string funcname = words[1];
      size_t bufsize = stoi(words[2]);
      cerr << "CALL " << funcname << " " << bufsize << endl;
      ByteArray result = cc_.call(funcname, bufsize);
      write_ok_response(result);
      return 0;
    }
    else if (command == "import") {
      string path = words[1];
      cerr << "IMPORT " << path << endl;
      cc_.import(path);
      write_ok_response();
      return 0;
    } else if (command == "quit") {
      cerr << "QUIT" << endl;
      write_ok_response();
      running_ = false;
      return 0;
    }
    else {
      throw std::invalid_argument("invalid command");
    }
  }

public:
  LLVMServerSession(tcp::socket &socket)
      : socket_(socket), running_(true)
    {}

  int start() {
    while (running_) {
      string s;
      asio::dynamic_string_buffer
        <char, string::traits_type, string::allocator_type>
        buf(s);
      size_t bytes_read = asio::read_until(socket_, buf, '\n');
      // contrary to my expectations, read_until() does not stop
      // reading when it reaches '\n'
      //
      // thus if the request has a payload, the s variable most likely
      // contains the initial part of it
      //
      // to increase confusion, bytes_read is set to the length of the
      // first line (including the closing delimiter), not the length
      // of the portion which has been actually read
      string first_line = s.substr(0, bytes_read);
      // store the excess part for later processing
      request_payload_.assign(s.begin() + bytes_read, s.end());
      trim(first_line);
      try {
        size_t consumed_payload_size = process_request(first_line);
        if (s.size() > (bytes_read + consumed_payload_size)) {
          // someone sent too much input
          throw std::runtime_error("protocol violation: excess bytes in the request");
        }
      }
      catch (std::runtime_error &e) {
        // runtime errors cause the server to exit
        write_error_response(e.what());
        break;
      }
      catch (std::exception &e) {
        // all other errors are just reported to the client
        write_error_response(e.what());
      }
    }
    return 0;
  }
};

LLVMServer::LLVMServer(const char *bind_address, int port)
  : bind_address_(bind_address), port_(port),
    acceptor_(io_context_,
              tcp::endpoint(
                asio::ip::make_address_v4(bind_address),
                port)) {}

int LLVMServer::start() {
  cerr << "* llvm-server listening on "
            << bind_address_ << ":" << port_
            << endl;
  for (;;) {
    tcp::socket socket(io_context_);
    acceptor_.accept(socket);
    if (fork() == 0) {
      cerr << "* accepted new connection" << endl;
      LLVMServerSession session(socket);
      int rv = session.start();
      cerr << "* connection closed" << endl;
      return rv;
    }
  }
}
