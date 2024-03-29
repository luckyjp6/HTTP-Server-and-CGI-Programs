#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <unistd.h>
#include <cstring>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket)) {}
    
  void start() { do_read(); }

private:
  void my_parse_request(std::size_t length)
  {
    char *line;

    line = strtok(rq, " "); 
    setenv("REQUEST_METHOD", line, 1);  
    
    line = strtok(NULL, " "); 
    char *query = strchr(line, '?');
    if (query != NULL)
    {
      *query = '\0';  
      query += 1;
      setenv("QUERY_STRING", query, 1);  
    }
    setenv("REQUEST_URI", line, 1);  
    
    line = strtok(NULL, "\r\n"); 
    setenv("SERVER_PROTOCOL", line, 1);   
      
    // Host: nplinux10.cs.nctu.edu.tw:10001
    line = strtok(NULL, " "); 
    line = strtok(NULL, "\r\n"); 
    char *p = strchr(line, ':');
    *p = '\0';
    setenv("HTTP_HOST", line, 1);
    
    setenv("SERVER_ADDR", socket_.local_endpoint().address().to_string().data(), 1);
    
    char prt[20];
    memset(prt, '\0', sizeof(prt));
    sprintf(prt, "%d", socket_.local_endpoint().port());
    setenv("SERVER_PORT", prt, 1);
    
    setenv("REMOTE_ADDR", socket_.remote_endpoint().address().to_string().data(), 1);

    memset(prt, '\0', sizeof(prt));
    sprintf(prt, "%d", socket_.remote_endpoint().port());
    setenv("REMOTE_PORT", prt, 1);
  }
  void do_read()
  {    
    auto self(shared_from_this());
    
    memset(rq, '\0', max_length);    
    socket_.async_read_some(boost::asio::buffer(rq, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            do_write("HTTP:1.1 200 OK\r\n");
            printf("connected\n");

            my_parse_request(length);
            char to[100];
            sprintf(to, ".%s", getenv("REQUEST_URI"));
            printf("to:%s\n", to);

            printf("native handle: %d\n", socket_.native_handle());
            
            if (fork() == 0){
              dup2(socket_.native_handle(), STDOUT_FILENO);
              dup2(socket_.native_handle(), STDIN_FILENO);
              if (execvp(to, nullptr) < 0) 
              {
                printf("exec fail\n");
                socket_.close();
                exit(0);
              }
            }
            else socket_.close();
          }
        });
  }
  void do_write(std::string msg)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(msg.data(), msg.size()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            // do_read();
          }
        });
  }

  tcp::socket socket_;
  enum { max_length = 15000 };
  char data_[max_length];
  char rq[max_length];
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(
      [this](boost::system::error_code ec, tcp::socket socket)
      {
        if (!ec)
        {
          std::make_shared<session>(std::move(socket))->start();
        }

        do_accept();
      });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}