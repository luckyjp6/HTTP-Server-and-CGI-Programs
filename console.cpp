#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <unistd.h>
#include <cstring>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

#define max_length 50000

using namespace std;
using boost::asio::ip::tcp;

struct q
{
    string host, port, file;
    int fd;
}my_query[5];

string id_name[5] = {"s0", "s1", "s2", "s3", "s4"};
char *buff[5];

void my_escape(string& data) {
    string buffer;
    buffer.reserve(data.size());
    for(size_t pos = 0; pos != data.size(); ++pos) {
        switch(data[pos]) {
            case '&':  buffer.append("&amp;");       break;
            case '\"': buffer.append("&quot;");      break;
            case '\'': buffer.append("&apos;");      break;
            case '<':  buffer.append("&lt;");        break;
            case '>':  buffer.append("&gt;");        break;
            case '\n': buffer.append("&NewLine;");   break;
            default:   buffer.append(&data[pos], 1); break;
        }
    }
    data.swap(buffer);
}

void output_shell(int id, string add)
{
    my_escape(add);
    cout << "<script>document.getElementById('" << id_name[id] << "').innerHTML += '" << add << "';</script>" << endl;
}

void output_command(int id, string add)
{
    my_escape(add);
    cout << "<script>document.getElementById('" << id_name[id] << "').innerHTML += '<b>" << add << "</b>';</script>" << endl;
}

void parse_query(string query)
{
    int eq = 0, ad = 0;
    for (int i = 0; i < 5; i++)
    {
        // host
        eq = query.find('=', eq);
        ad = query.find('&', ad);
        my_query[i].host = query.substr(eq+1, ad-eq-1);
        eq++; ad++;

        // port
        eq = query.find('=', eq);
        ad = query.find('&', ad);
        my_query[i].port = query.substr(eq+1, ad-eq-1);
        eq++; ad++;

        // file
        eq = query.find('=', eq);
        if (i == 5-1) my_query[i].file = query.substr(eq+1);
        else {
            ad = query.find('&', ad);
            my_query[i].file = query.substr(eq+1, ad-eq-1);
        }
        eq++; ad++;
        
        if (my_query[i].host.size() > 0) output_shell(i, "host: " + my_query[i].host + " port: " + my_query[i].port);
        if (my_query[i].file.size() > 0) my_query[i].file = "test_case/" + my_query[i].file;
                
    }
}

class my_client
  : public std::enable_shared_from_this<my_client>
{
public:
    my_client(tcp::socket socket)
    : shell(std::move(socket))
    {
    }

    void start(int index)
    {
        id = index;
        dup2(my_query[id].fd, STDIN_FILENO);
        do_read_shell();
    }

private:
    void do_read_shell()
    {
        // read from np shell
        // output everything until '%' is prompted
        auto self(shared_from_this());
		memset(rq, '\0', max_length);
        shell.async_read_some(boost::asio::buffer(rq, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
			if (!ec)
			{
				output_shell(id, rq);
				if (find(rq, rq+length, '%')) do_read_file();
				else do_read_shell();
			}
			else cout << "failed to read from shell" << endl;
        });
	}
	void do_read_file()
	{
        // read command from file
        string in;        
		getline(cin, in);
		output_command(id, in.data());

		do_write_shell(in);
	}
	void do_write_shell(string in)
	{
		// write the new command to shell            
        auto self(shared_from_this());

        boost::asio::async_write(shell, boost::asio::buffer(in.data(), in.size()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                do_read_shell();
            }
			else cout << "can't write to shell" << endl;
        });
    }
    int id;
    tcp::socket shell;
};

int main(int argc, char* argv[]) 
{
    // print basic html
    // {
    //     cout << "<!DOCTYPE html>\
    //                     <html lang=\"en\">\
    //                         <head>\
    //                             <meta charset=\"UTF-8\" />\
    //                             <title>NP Project 3 Sample Console</title>\
    //                             <link\
    //                             rel=\"stylesheet\"\
    //                             href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\
    //                             integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\
    //                             crossorigin=\"anonymous\"\
    //                             />\
    //                             <link\
    //                             href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\
    //                             rel=\"stylesheet\"\
    //                             />\
    //                             <link\
    //                             rel=\"icon\"\
    //                             type=\"image/png\"\
    //                             href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\
    //                             />\
    //                             <style>\
    //                             * {\
    //                                 font-family: 'Source Code Pro', monospace;\
    //                                 font-size: 1rem !important;\
    //                             }\
    //                             body {\
    //                                 background-color: #212529;\
    //                             }\
    //                             pre {\
    //                                 color: #cccccc;\
    //                             }\
    //                             b {\
    //                                 color: #01b468;\
    //                             }\
    //                             </style>\
    //                         </head>\
    //                         <body>\
    //                             <table class=\"table table-dark table-bordered\">\
    //                             <thead>\
    //                                 <tr>\
    //                                 <th scope=\"col\">nplinux1.cs.nctu.edu.tw:1234</th>\
    //                                 <th scope=\"col\">nplinux2.cs.nctu.edu.tw:5678</th>\
    //                                 </tr>\
    //                             </thead>\
    //                             <tbody>\
    //                                 <tr>\
    //                                 <td><pre id=\"s0\" class=\"mb-0\"></pre></td>&NewLine;\
    //                                 <td><pre id=\"s1\" class=\"mb-0\"></pre></td>&NewLine;\
    //                                 <td><pre id=\"s2\" class=\"mb-0\"></pre></td>&NewLine;\
    //                                 <td><pre id=\"s3\" class=\"mb-0\"></pre></td>&NewLine;\
    //                                 <td><pre id=\"s4\" class=\"mb-0\"></pre></td>\
    //                                 </tr>\
    //                             </tbody>\
    //                             </table>\
    //                         </body>\
    //                         </html>";
    // }
    
    string query = "h0=nplinux1.cs.nctu.edu.tw&p0=1234&f0=t1.txt&h1=nplinux2.cs.nctu.edu.tw&p1=5678&f1=t2.txt&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4="; 
    // string query = getenv("QUERY_STRING");
    parse_query(query);

    boost::asio::io_context io_context;
    boost::asio::io_service io_service;

    // for (int i = 0; i < 1; i++)
    // {
        int i = 0;
        if (my_query[i].file.size() > 0) 
        {
            if ((my_query[i].fd = open(my_query[i].file.data(), O_RDONLY)) > 0)
                cout << "successfully open " << my_query[i].file.data() << endl;
            else cout << "can't open file " << my_query[i].file.data() << endl;
        }
        else 
        {
            cout << "no give file" << endl;
            // break;
            return -1;
        }
        // else break;
        
        tcp::socket socket(io_context);
        tcp::resolver r(io_service);
        tcp::resolver::query q(/*my_query[i].host.data()*/ "google.com", "http");

        auto iter = r.resolve(q);
        decltype(iter) end;
        boost::system::error_code ec = boost::asio::error::host_not_found;
        for (; ec && iter != end; ++iter)
        {
            socket.close();
            socket.connect(*iter, ec);            
        }

        if (ec)
        {
            cout << "can't connect." << endl;
            return -1;
        }

        cout << "connect success: " << endl;
        cout << socket.remote_endpoint().address() << endl;

        make_shared<my_client> (move(socket))->start(i);

    // }
        
    
    io_context.run();
    return 0;
}

// #include <list>
// #include <map>
// using namespace boost;
// // Function pointer type that points to the callback
// // function which is called when a request is complete.
// // Based on the values of the parameters passed to it, it outputs information about the finished request.
// typedef void (*Callback)(unsigned int request_id,        // unique identifier of the request is assigned to the request when it was initiated.
//                          const std::string &response,    // the response data
//                          const system::error_code &ec);  // error information

// // data structure whose purpose is to keep the data related to a particular request while it is being executed
// struct Session
// {
//     Session(asio::io_service &ios,
//             const std::string &raw_ip_address,
//             unsigned short port_num,
//             const std::string &request,
//             unsigned int id,
//             Callback callback) : m_sock(ios),
//                                  m_ep(asio::ip::address::from_string(raw_ip_address),
//                                       port_num),
//                                  m_request(request),
//                                  m_id(id),
//                                  m_callback(callback),
//                                  m_was_cancelled(false) {}

//     asio::ip::tcp::socket m_sock; // Socket used for communication
//     asio::ip::tcp::endpoint m_ep; // Remote endpoint.
//     std::string m_request;        // Request string.

//     // streambuf where the response will be stored.
//     asio::streambuf m_response_buf;
//     std::string m_response; // Response represented as a string.

//     // Contains the description of an error if one occurs during
//     // the request lifecycle.
//     system::error_code m_ec;

//     unsigned int m_id; // Unique ID assigned to the request.

//     // Pointer to the function to be called when the request
//     // completes.
//     Callback m_callback;

//     bool m_was_cancelled;
//     std::mutex m_cancel_guard;
// };

// // class that provides the asynchronous communication functionality.
// class AsyncTCPClient
// {
// public:
//     AsyncTCPClient(int num_of_threads) 
//     {

//     }

//     // initiates a request to the server
//     void emulateLongComputationOp(      //represents the request parameter according to the application layer protocol
//         const std::string &raw_ip_address,  //specify the server to which the request should be sent.
//         unsigned short port_num,            //specify the server to which the request should be sent.
//         Callback callback,                  //callback function, which will be called when the request is complete.
//         unsigned int request_id){}    // unique identifier of the request


//     // // cancels the previously initiated request designated by the request_id argument
//     // void cancelRequest(unsigned int request_id){} //accepts an identifier of the request to be canceled as an argument.


//     // // blocks the calling thread until all the currently running requests complete and deinitializes the client.
//     // void close(){}


// private:
//     // method is called whenever the request completes with any result.
//     // void onRequestComplete(std::shared_ptr<Session> session){}

// private:
//     asio::io_service m_ios;
//     std::map<int, std::shared_ptr<Session>> m_active_sessions;
//     std::mutex m_active_sessions_guard;
//     std::unique_ptr<boost::asio::io_service::work> m_work;
//     std::list<std::unique_ptr<std::thread>> m_threads;
// };

// // a function that will serve as a callback, which we'll pass to the AsyncTCPClient::emulateLongComputationOp() method
// // It outputs the result of the request execution and the response message to the standard output stream if the request is completed successfully
// void handler(unsigned int request_id,
//              const std::string &response,
//              const system::error_code &ec){}

// int main()
// {
//     AsyncTCPClient client(5);

//     // Here we emulate the user's behavior.

//     // creates an instance of the AsyncTCPClient class and then calls its emulateLongComputationOp() method to initiate three asynchronous requests
//     // User initiates a request with id 1.
//     client.emulateLongComputationOp(10, "127.0.0.1", 3333, handler, 1);

//     // Decides to exit the application.
//     client.close();

//     return 0;
// };