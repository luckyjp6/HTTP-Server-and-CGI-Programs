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

void output_topic(int id, string add)
{
	printf("<script>document.getElementById('t%d').innerHTML += '%s';</script>\n", id, add.data());
	fflush(stdout);
}

void output_shell(int id, string add)
{
	// add += "\n";
    my_escape(add);
	printf("<script>document.getElementById('s%d').innerHTML += '%s';</script>\n", id, add.data());
	fflush(stdout);
}

void output_command(int id, string add)
{
	// add += "\n";
    my_escape(add);
    printf("<script>document.getElementById('s%d').innerHTML += '<b>%s</b>';</script>\n", id, add.data());
	fflush(stdout);
}

void parse_query(string query)
{
	cout << "query: " << query << endl;
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
        
        if (my_query[i].host.size() > 0) output_topic(i, my_query[i].host + ":" + my_query[i].port);
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
		cout << "go to read shell" << endl;
        do_read_shell();
    }

private:
	void close_client()
	{
		shell.close();
		close(my_query[id].fd);
		exit(0);
	}
    void do_read_shell()
    {
        // read from np shell
        // output everything until '%' is prompted
		// char shell_msg[max_length];
		// memset(shell_msg, '\0', max_length);
		// int length = read(shell.native_handle(), shell_msg, max_length);
		// if (length < 0)
		// {
		// 	cout << "failed to read" << endl;
		// 	exit(0);
		// }
		// if (length == 0)
		// {
		// 	cout << "connection closed by server" << endl;
		// 	exit(0);
		// }
		// string shell_msg_s(shell_msg);
		// output_shell(id, shell_msg_s);
		// if (shell_msg_s.find('%') != string::npos) do_read_file();
		// else do_read_shell();
        auto self(shared_from_this());
		memset(shell_msg, '\0', max_length);
        shell.async_read_some(boost::asio::buffer(shell_msg, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
			if (!ec)
			{
				if (length == 0)
				{
					cout << "connection closed by server!" << endl;
					exit(0);
				}
				string shell_msg_s(shell_msg);
				output_shell(id, shell_msg_s);
				if (shell_msg_s.find('%') != string::npos) do_read_file();
				else do_read_shell();
			}
			else 
			{
				cout << "failed to read from shell" << endl;
				exit(0);
			}
        });
	}
	void do_read_file()
	{
		if (cin.eof()) 
		{
			cout << "end of filen";
			close_client();
		}
        // read command from file
        string in;        
		getline(cin, in);
		output_command(id, in + "\n");
		// if (in == "exit") do_read_shell();
		// else 
		do_write_shell(in);
	}

	void do_write_shell(string in)
	{
		in += "\n";
		// write the new command to shell            
		// if (write(shell.native_handle(), in.data(), in.size()) <= 0)
		// {
		// 	cout << "failed to write" << endl;
		// 	exit(0);
		// }
		// do_read_shell();

        auto self(shared_from_this());

        boost::asio::async_write(shell, boost::asio::buffer(in.data(), in.size()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                do_read_shell();
            }
			else {
				cout << "can't write to shell" << endl;
				exit(0);
			}
        });
    }
    int id;
	char shell_msg[max_length];
    tcp::socket shell;
};

int main() 
{
    // print basic html
    {
		cout << "Content-type: text/html\r\n\r\n";
        cout << "<!DOCTYPE html>\
                        <html lang=\"en\">\
                            <head>\
                                <meta charset=\"UTF-8\" />\
                                <title>NP Project 3 Sample Console</title>\
                                <link\
                                rel=\"stylesheet\"\
                                href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\
                                integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\
                                crossorigin=\"anonymous\"\
                                />\
                                <link\
                                href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\
                                rel=\"stylesheet\"\
                                />\
                                <link\
                                rel=\"icon\"\
                                type=\"image/png\"\
                                href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\
                                />\
                                <style>\
                                * {\
                                    font-family: 'Source Code Pro', monospace;\
                                    font-size: 1rem !important;\
                                }\
                                body {\
                                    background-color: #212529;\
                                }\
                                pre {\
                                    color: #cccccc;\
                                }\
                                b {\
                                    color: #01b468;\
                                }\
                                </style>\
                            </head>\
                            <body>\
                                <table class=\"table table-dark table-bordered\">\
                                <thead>\
                                    <tr>\
                                    <th scope=\"col\"<pre id=\"t0\" class=\"mb-0\"></pre></th>\
                                    <th scope=\"col\"<pre id=\"t1\" class=\"mb-0\"></pre></th>\
                                    <th scope=\"col\"<pre id=\"t2\" class=\"mb-0\"></pre></th>\
                                    <th scope=\"col\"<pre id=\"t3\" class=\"mb-0\"></pre></th>\
                                    <th scope=\"col\"<pre id=\"t4\" class=\"mb-0\"></pre></th>\
                                    </tr>\
                                </thead>\
                                <tbody>\
                                    <tr>\
                                    <td><pre id=\"s0\" class=\"mb-0\"></pre></td>&NewLine;\
                                    <td><pre id=\"s1\" class=\"mb-0\"></pre></td>&NewLine;\
                                    <td><pre id=\"s2\" class=\"mb-0\"></pre></td>&NewLine;\
                                    <td><pre id=\"s3\" class=\"mb-0\"></pre></td>&NewLine;\
                                    <td><pre id=\"s4\" class=\"mb-0\"></pre></td>\
                                    </tr>\
                                </tbody>\
                                </table>\
                            </body>\
                            </html>" << endl;
    }
    
    // setenv("QUERY_STRING", "h0=nplinux1.cs.nctu.edu.tw&p0=1234&f0=t1.txt&h1=&p1=&f1=&h2=&p2=&f2=&h3=&p3=&f3=&h4=&p4=&f4=", 1); 
    string query = getenv("QUERY_STRING");
    parse_query(query);

    boost::asio::io_context io_context;
    boost::asio::io_service io_service;

    for (int i = 0; i < 1; i++)
    {
		if (fork() == 0)
		{
			cout << "start reading file " << my_query[i].file.data() << endl;
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
			
			tcp::socket socket(io_context);
			tcp::resolver r(io_service);
			tcp::resolver::query q(my_query[i].host.data(), my_query[i].port.data());

			auto iter = r.resolve(q);
			decltype(iter) end;
			boost::system::error_code ec = boost::asio::error::host_not_found;
			cout << "start connect" << endl;
			for (; ec && iter != end; ++iter)
			{
				socket.connect(*iter, ec);
				if (ec) socket.close();
			}

			if (ec)
			{
				cout << "can't connect" << endl;
				return -1;
			}

			cout << "connect success: " << socket.remote_endpoint().address() << ":" << socket.remote_endpoint().port() << endl;

			make_shared<my_client> (move(socket))->start(i);
			break;
		}
		
    }
        
    
    io_context.run();
    return 0;
}