#include <iostream>
#include <boost/algorithm/string.hpp>

#include "server.h"
#include "compiler.h"

class LLVMServerSession {
  tcp::socket &socket_;
  CompileContext cc_;
  bool running_;
  ByteArray request_payload_;

  void read_payload(int total_size) {
    int consumed_size = request_payload_.size();
    if (request_payload_.capacity() <= total_size) {
      // reserve an extra byte for terminating zero
      request_payload_.resize(total_size + 1);
    }
    request_payload_.set_size(total_size);
    int remaining_size = total_size - consumed_size;
    if (remaining_size > 0) {
      asio::read(socket_,
                 asio::buffer(request_payload_.begin() + consumed_size,
                              remaining_size),
                 asio::transfer_exactly(remaining_size));
    }
    // add invisible terminating zero
    // (needed by LLLexer::getNextChar())
    *(request_payload_.begin() + total_size) = 0;
  }

  void write_payload(const ByteArray &payload) {
    asio::write(socket_, asio::buffer(payload.begin(), payload.size()));
  }

  void write_ok_response() {
    std::string response = "OK 0\n";
    asio::write(socket_, asio::buffer(response));
  }

  void write_ok_response(const ByteArray &payload) {
    std::string response = "OK ";
    response += std::to_string(payload.size());
    response += "\n";
    asio::write(socket_, asio::buffer(response));
    write_payload(payload);
  }

  void write_error_response(const char *error_message) {
    int len = strlen(error_message);
    std::string response = "ERROR ";
    response += std::to_string(len);
    response += "\n";
    asio::write(socket_, asio::buffer(response));
    ByteArray payload(error_message, error_message+len);
    write_payload(payload);
  }

  void process_request(const std::string &first_line) {
    std::vector<std::string> words;
    split(words, first_line, is_any_of(" \t"), token_compress_on);
    std::string command = words[0];
    to_lower(command);
    if (command == "parse") {
      int payload_size = stoi(words[1]);
      std::cerr << "PARSE " << payload_size << std::endl;
      read_payload(payload_size);
      cc_.parse(request_payload_);
      write_ok_response();
    } else if (command == "opt") {
      std::cerr << "OPT" << std::endl;
      cc_.opt();
      write_ok_response();
    }
    else if (command == "dump") {
      std::cerr << "DUMP" << std::endl;
      ByteArray payload = cc_.dump();
      write_ok_response(payload);
    }
    else if (command == "link") {
      std::cerr << "LINK" << std::endl;
      cc_.link();
      write_ok_response();
    }
    else if (command == "commit") {
      std::cerr << "COMMIT" << std::endl;
      cc_.commit();
      write_ok_response();
    }
    else if (command == "call") {
      std::string funcname = words[1];
      int bufsize = stoi(words[2]);
      std::cerr << "CALL " << funcname << " " << bufsize << std::endl;
      ByteArray result = cc_.call(funcname, bufsize);
      write_ok_response(result);
    }
    else if (command == "import") {
      std::string path = words[1];
      std::cerr << "IMPORT " << path << std::endl;
      cc_.import(path);
      write_ok_response();
    } else if (command == "quit") {
      std::cerr << "QUIT" << std::endl;
      running_ = false;
    }
    else {
      throw std::runtime_error("invalid command");
    }
  }

public:
  LLVMServerSession(tcp::socket &socket)
      : socket_(socket),
        running_(true)
    {}

  int start() {
    while (running_) {
      std::string s;
      asio::dynamic_string_buffer
        <char, std::string::traits_type, std::string::allocator_type>
        buf(s);
      std::size_t bytes_read = asio::read_until(socket_, buf, '\n');
      std::string first_line = s.substr(0, bytes_read);
      request_payload_.assign(s.begin()+bytes_read, s.end());
      trim(first_line);
      try {
        process_request(first_line);
      }
      catch (std::exception &e) {
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
  std::cerr << "llvm-server listening on "
            << bind_address_ << ":" << port_
            << std::endl;
  for (;;) {
    tcp::socket socket(io_context_);
    acceptor_.accept(socket);
    if (fork() == 0) {
      std::cerr << "accepted new connection" << std::endl;
      LLVMServerSession session(socket);
      return session.start();
    }
  }
}
