#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <boost/asio.hpp>

#define max_length 50000

using namespace std;
using boost::asio::ip::tcp;

boost::asio::io_context io_context;

struct q
{
    string host, port, file;
    int fd;
}my_query[5];

void Setenv(string env, string var)
{
	// cout << "setenv received: "<< env << " and " << var << endl;
    SetEnvironmentVariable(TEXT(env.data()), TEXT(var.data()));
}
void Getenv(string want, string &result)
{
    char s[4096];
    int length = GetEnvironmentVariable(TEXT(want.data()), s, 4096);
	// cout << "getenv received: " << s << endl;
	result.assign(s, s+length);
}

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
            case '\r': break;
			case '\n': buffer.append("&NewLine;");   break;
			
            default:   buffer.append(&data[pos], 1); break;
        }
    }
    data.swap(buffer);
}

class my_client
  : public std::enable_shared_from_this<my_client>
{
public:
    my_client(tcp::socket shell_, boost::shared_ptr<tcp::socket> web_) 
    : shell(std::move(shell_)), web(web_)
    {
    }

    void start(int index)
    {
        id = index;
        // dup2(my_query[id].fd, STDIN_FILENO);
		cout << "start reading file " << my_query[id].file.data() << endl;

		in.open(my_query[id].file.data(), ios::in);
		if (!in) cout << "can't open file " << my_query[id].file.data() << endl;
		else cout << "successfully open " << my_query[id].file.data() << endl;
        
		do_read_shell();
    }

private:
	void close_client()
	{
		shell.close();
		close(my_query[id].fd);
		shell.close();
		return;
	}
    void do_read_file()
	{
		cout << "read from file " << id << endl;
		if (in.eof()) 
		{
			cout << "end of file" << endl;
			close_client();
			return;
		}
		
        // read command from file
        string input;        
		getline(in, input);
		input += "\n";
		output_command(input);
		
		do_write_shell(input);
	}
	void do_read_shell()
    {
        auto self(shared_from_this());
		memset(shell_msg, '\0', max_length);
        shell.async_read_some(boost::asio::buffer(shell_msg, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
			cout << "in read shell handler " << id << endl;
			if (!ec)
			{
				string shell_msg_s(shell_msg);
				output_shell(shell_msg_s);
				if (shell_msg_s.find('%') != string::npos) do_read_file(); // % is presented
				else do_read_shell(); // waiting for %
			}
			else 
			{
				if (length == 0) cout << "connection closed by server! " <<  id << endl;				
				else cout << "failed to read from shell " << id << endl;

				close_client();
				return;
			}
        });
	}
	void do_write_shell(string msg)
	{
        auto self(shared_from_this());

        boost::asio::async_write(shell, boost::asio::buffer(msg.data(), msg.size()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                do_read_shell();
            }
			else {
				cout << "can't write to shell " << id << endl;
				close_client();
				return;
			}
        });
    }
    void do_write_web(string msg)
	{
		auto self(shared_from_this());

        boost::asio::async_write(*web, boost::asio::buffer(msg.data(), msg.size()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (ec)
			{
				cout << "can't write to shell" << id << endl;
				close_client();
				return;
			}
        });
	}

	void output_shell(string add)
	{
		my_escape(add);
		char msg[max_length];
		sprintf(msg, "<script>document.getElementById('s%d').innerHTML += '%s';</script>\n", id, add.data());
		do_write_web(msg);
	}
	void output_command(string add)
	{
		my_escape(add);
		char msg[max_length];
		sprintf(msg, "<script>document.getElementById('s%d').innerHTML += '<b>%s</b>';</script>\n", id, add.data());
		do_write_web(msg);
	}

	int id;
	ifstream in;
	char shell_msg[max_length];
    tcp::socket shell;
	boost::shared_ptr<tcp::socket> web;
};

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(boost::shared_ptr<tcp::socket> socket)
    : web(socket) {}
    
  void start() { do_read(); }

private:
	void my_parse_request(std::size_t length)
	{
		char *line;
		string s;

		// GET /test.cgi?a=b&c=d HTTP/1.1
		line = strtok(rq, " "); 
		s.assign(line, line+strlen(line));
		Setenv("REQUEST_METHOD", s); 
	// Getenv("REQUEST_METHOD", s);
	// printf("REQUEST_METHOD: %s\n", s.data());
		line = strtok(NULL, " "); 
		char *query = strchr(line, '?');
		if (query != NULL)
		{
		*query = '\0';  
		query += 1;
		s.assign(query, query+strlen(query));
		Setenv("QUERY_STRING", s);  
	// Getenv("QUERY_STRING", s);
	// printf("QUERY_STRING: %s\n", s.data()); 
		}
		s.assign(line, line+strlen(line));
		Setenv("REQUEST_URI", s);
	// Getenv("REQUEST_URI", s);  
	// printf("REQUEST_URI: %s\n", s.data());
		line = strtok(NULL, "\r\n"); 
		s.assign(line, line+strlen(line));
		Setenv("SERVER_PROTOCOL", s);
	// Getenv("SERVER_PROTOCOL", s);   
	// printf("SERVER_PROTOCOL: %s\n", s.data());
		// Host: nplinux10.cs.nctu.edu.tw:10001
		line = strtok(NULL, " "); 
		line = strtok(NULL, "\r\n"); 
		char *p = strchr(line, ':');
		*p = '\0';
		s.assign(line, line+strlen(line));
		Setenv("HTTP_HOST", s);
	// Getenv("HTTP_HOST", s);
	// printf("HTTP_HOST: %s\n", s.data());

		Setenv("SERVER_ADDR", web->local_endpoint().address().to_string());
	// Getenv("SERVER_ADDR", s);
	// printf("SERVER_ADDR: %s\n", s.data());
		char prt[20];
		memset(prt, '\0', sizeof(prt));
		sprintf(prt, "%d", web->local_endpoint().port());
		s.assign(prt, prt+strlen(prt));
		Setenv("SERVER_PORT", s);
	// Getenv("SERVER_PORT", s);
	// printf("SERVER_PORT: %s\n", s.data());

		Setenv("REMOTE_ADDR", web->remote_endpoint().address().to_string());
	// Getenv("REMOTE_ADDR", s);
	// printf("REMOTE_ADDR: %s\n", s.data());
		memset(prt, '\0', sizeof(prt));
		sprintf(prt, "%d", web->remote_endpoint().port());
		s.assign(prt, prt+strlen(prt));
		Setenv("REMOTE_PORT", s);
	// Getenv("REMOTE_PORT", s);
	// printf("REMOTE_PORT: %s\n", s.data());

	}
	void do_read()
	{    
		auto self(shared_from_this());
		
		memset(rq, '\0', max_length);    
		web->async_read_some(boost::asio::buffer(rq, max_length),
			[this, self](boost::system::error_code ec, std::size_t length)
			{
			if (!ec)
			{
				// printf("received: %s\n", rq);
				do_write("HTTP:1.1 200 OK\r\n");
				printf("connected\n");

				my_parse_request(length);
				string query;
				Getenv("REQUEST_URI", query);
				cout << "query " << query << endl;
				if (query == "/panel.cgi") print_panel();
				else if (query == "/console.cgi") do_console();
			}
			});
	}
	void do_write(string msg)
  {
    auto self(shared_from_this());
    boost::asio::async_write(*web, boost::asio::buffer(msg.data(), msg.size()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
			fflush(stdout);
        });
  }

	void print_panel()
	{
		// basic html of panel
		do_write("Content-type: text/html\r\n\r\n"); 
		do_write("<!DOCTYPE html> \
					<html lang='en'>\
					<head>\
						<meta charset='UTF-8' />\
						<title>NP Project 3 Panel</title>\
						<link\
							rel='stylesheet'\
							href='https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css'\
							integrity='sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2'\
							crossorigin='anonymous'\
							/>\
							<link\
							href='https://fonts.googleapis.com/css?family=Source+Code+Pro'\
							rel='stylesheet'\
							/>\
							<link data-default-icon='data:image/jpeg;base64,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' \
							data-badged-icon='data:image/jpeg;base64,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'\
							rel='icon' \
							href='data:image/jpeg;base64,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'>\
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
						<style>\
						* {\
							font-family: 'Source Code Pro', monospace;\
						}\
						</style>\
					</head>\
					<body class='bg-secondary pt-5'>\
						<form action='console.cgi' method='GET'>\
							<table class='table mx-auto bg-light' style='width: inherit'>\
								<thead class='thead-dark'>\
								<tr>\
									<th scope='col'>#</th>\
									<th scope='col'>Host</th>\
									<th scope='col'>Port</th>\
									<th scope='col'>Input File</th>\
								</tr>\
								</thead>\
								<tbody>\n");
						
		for (int i = 0; i < 5; i++)
		{
			char msg[max_length];
			memset(msg, '\0', max_length);
			sprintf(msg, "<tr>\
				<th scope='row' class='align-middle'>Session %d</th>\
				<td>\
				<div class='input-group'>\
					<select name='h%d' class='custom-select'>\
					<option></option><option value='nplinux1.cs.nctu.edu.tw'>nplinux1</option><option value='nplinux2.cs.nctu.edu.tw'>nplinux2</option><option value='nplinux3.cs.nctu.edu.tw'>nplinux3</option><option value='nplinux4.cs.nctu.edu.tw'>nplinux4</option><option value='nplinux5.cs.nctu.edu.tw'>nplinux5</option><option value='nplinux6.cs.nctu.edu.tw'>nplinux6</option><option value='nplinux7.cs.nctu.edu.tw'>nplinux7</option><option value='nplinux8.cs.nctu.edu.tw'>nplinux8</option><option value='nplinux9.cs.nctu.edu.tw'>nplinux9</option><option value='nplinux10.cs.nctu.edu.tw'>nplinux10</option><option value='nplinux11.cs.nctu.edu.tw'>nplinux11</option><option value='nplinux12.cs.nctu.edu.tw'>nplinux12</option>\
					</select>\
					<div class='input-group-append'>\
					<span class='input-group-text'>.cs.nctu.edu.tw</span>\
					</div>\
				</div>\
				</td>\
				<td>\
				<input name='p%d' type='text' class='form-control' size='5' />\
				</td>\
				<td>\
				<select name='f%d' class='custom-select'>\
					<option></option><option value='t1.txt'>t1.txt</option><option value='t2.txt'>t2.txt</option><option value='t3.txt'>t3.txt</option><option value='t4.txt'>t4.txt</option><option value='t5.txt'>t5.txt</option>\
				</select>\
				</td>\
			</tr>", i+1, i, i, i);
			do_write(string (msg));
		}
		do_write("<tr>\
				<td colspan='3'></td>\
				<td>\
				<button type='submit' class='btn btn-info btn-block'>Run</button>\
				</td>\
			</tr>\
			</tbody>\
		</table>\
		</form>\
	</body>\
	</html>");
	
	}
	void parse_query()
	{
		string query;
		Getenv("QUERY_STRING", query);
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
	void do_console()
	{
		print_console();
		parse_query();

		int num_child = 0;
		for (int i = 0; i < 5; i++)
		{
			
			if (my_query[i].file.size() <= 0) break;
			num_child ++;
			
			tcp::socket shell(io_context);
			tcp::resolver r(io_context);
			tcp::resolver::query q(my_query[i].host.data(), my_query[i].port.data());

			auto iter = r.resolve(q);
			decltype(iter) end;
			boost::system::error_code ec = boost::asio::error::host_not_found;
			cout << "start connect" << endl;
			for (; ec && iter != end; ++iter)
			{
				shell.connect(*iter, ec);
				if (ec) shell.close();
			}

			if (ec)
			{
				cout << "can't connect" << endl;
				return;
			}

			cout << "connect success: " << shell.remote_endpoint().address() << ":" << shell.remote_endpoint().port() << endl;

			make_shared<my_client> (std::move(shell), web)->start(i);	
		}
	}
	void print_console()
	{
		do_write("Content-type: text/html\r\n\r\n");
		do_write("<!DOCTYPE html>\
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
							<link data-default-icon='data:image/jpeg;base64,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' \
							data-badged-icon='data:image/jpeg;base64,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'\
							rel='icon' \
							href='data:image/jpeg;base64,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'>\
							\
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
						</html>\n");
	}
	void output_topic(int id, string add)
	{
		my_escape(add);
		char msg[max_length];
		sprintf(msg, "<script>document.getElementById('t%d').innerHTML += '%s';</script>\n", id, add.data());
		// add = "<script>document.getElementById('t" + (char)(id+'0') + "').innerHTML += '" + %s + "';</script>\n";add.data());
		do_write(msg);
	}
	
	boost::shared_ptr<tcp::socket> web;
	// tcp::socket web;
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
		cout << "do accept" << endl;
		acceptor_.async_accept(
		[this](boost::system::error_code ec, tcp::socket socket)
		{
			if (!ec)
			{
				boost::shared_ptr<tcp::socket> sock_ptr(new tcp::socket (std::move(socket)));
				std::make_shared<session>(sock_ptr)->start();
			}

			do_accept();
		});
	}

	tcp::acceptor acceptor_;
};

int main(int argc, char **argv)
{   
    if (argc != 2)
    {
        printf("Usage: cgi_server.exe [port]\n");
        return -1;
    }

    try 
    {
        server s(io_context, atoi(argv[1]));
        io_context.run();
    }
    catch (exception &e)
    {
        cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}