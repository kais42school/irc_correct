/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Bot.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: marvin <marvin@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/19 17:15:02 by bhamoum           #+#    #+#             */
/*   Updated: 2026/07/03 12:06:51 by marvin           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Bot.hpp"
#include <fcntl.h>
#include <errno.h>
#include <cstdlib>

Bot::Bot(const std::string &server, int port, const std::string &password,
         const std::string &nickname, const std::string &channel)
	: _server(server), _port(port), _password(password), _nickname(nickname),
	  _channel(channel), _socket(-1), _connected(false), _registered(false)
{
	_start_time = time(NULL);
}

Bot::~Bot()
{
	disconnect();
}

int Bot::setNonBlocking(int fd)
{
	return fcntl(fd, F_SETFL, O_NONBLOCK);
}

// int Bot::setNonBlocking(int fd)
// {
// 	int flags = fcntl(fd, F_GETFL, 0);
// 	if (flags == -1)
// 		return -1;
// 	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
// }

int Bot::connect()
{
	struct sockaddr_in addr;
	
	_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (_socket < 0)
	{
		std::cerr << "Failed to create socket" << std::endl;
		return -1;
	}

	if (setNonBlocking(_socket) < 0)
	{
		std::cerr << "Failed to set non-blocking" << std::endl;
		close(_socket);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(_port);
	if (inet_pton(AF_INET, _server.c_str(), &addr.sin_addr) <= 0)
	{
		std::cerr << "Invalid server address" << std::endl;
		close(_socket);
		return -1;
	}

	int ret = ::connect(_socket, (struct sockaddr *)&addr, sizeof(addr));
	if (ret < 0)
	{
		if (errno != EINPROGRESS)
		{
			std::cerr << "Connection failed: " << strerror(errno) << std::endl;
			close(_socket);
			return -1;
		}
	}

	_connected = true;
	std::cout << "[BOT] Connecting to " << _server << ":" << _port << std::endl;
	return 0;
}

bool Bot::isConnected() const
{
	return _connected && _socket >= 0;
}

void Bot::disconnect()
{
	if (_socket >= 0)
		close(_socket);
	_socket = -1;
	_connected = false;
	_registered = false;
}

bool Bot::sendRaw(const std::string &message)
{
	if (!isConnected())
		return false;

	std::string msg = message + "\r\n";
	ssize_t total = msg.length();
	ssize_t sent = 0;

	while (sent < total)
	{
		ssize_t n = send(_socket, msg.c_str() + sent, total - sent, 0);
		if (n < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
				continue;
			std::cerr << "[BOT] Send error: " << strerror(errno) << std::endl;
			return false;
		}
		sent += n;
	}
	return true;
}

bool Bot::sendCommand(const std::string &command)
{
	std::cout << "[BOT] >> " << command << std::endl;
	return sendRaw(command);
}

std::string Bot::trim(const std::string &str)
{
	size_t start = str.find_first_not_of(" \t\r\n");
	if (start == std::string::npos)
		return "";
	size_t end = str.find_last_not_of(" \t\r\n");
	return str.substr(start, end - start + 1);
}

std::vector<std::string> Bot::split(const std::string &str, char delimiter)
{
	std::vector<std::string> tokens;
	std::stringstream ss(str);
	std::string token;

	while (std::getline(ss, token, delimiter))
		tokens.push_back(token);
	return tokens;
}

void Bot::cmdHelp(const std::string &target)
{
	std::string response = "Available commands: !help, !time, !uptime, !echo <msg>, !join <channel>, !users <channel>";
	sendCommand("PRIVMSG " + target + " :" + response);
}

void Bot::cmdTime(const std::string &target)
{
	time_t now = time(NULL);
	std::string timestr(ctime(&now));
	timestr = trim(timestr);
	sendCommand("PRIVMSG " + target + " :Current server time: " + timestr);
}

void Bot::cmdEcho(const std::string &target, const std::string &args)
{
	sendCommand("PRIVMSG " + target + " :Echo: " + args);
}

void Bot::cmdUptime(const std::string &target)
{
	time_t now = time(NULL);
	long uptime = now - _start_time;
	long hours = uptime / 3600;
	long minutes = (uptime % 3600) / 60;
	long seconds = uptime % 60;

	std::stringstream ss;
	ss << "Bot uptime: " << hours << "h " << minutes << "m " << seconds << "s";
	sendCommand("PRIVMSG " + target + " :" + ss.str());
}

void Bot::cmdJoin(const std::string &target, const std::string &args)
{
	std::string channel = trim(args);
	if (channel.empty())
	{
		sendCommand("PRIVMSG " + target + " :Usage: !join <channel>");
		return;
	}
	if (channel[0] != '#')
		channel = "#" + channel;
	sendCommand("JOIN " + channel);
	sendCommand("PRIVMSG " + target + " :Joined " + channel);
}

void Bot::cmdUsers(const std::string &target, const std::string &args)
{
	(void)target;
	std::string channel = trim(args);
	if (channel.empty())
		channel = _channel;
	if (channel[0] != '#')
		channel = "#" + channel;
	sendCommand("NAMES " + channel);
}

void Bot::parseCommand(const std::string &prefix, const std::string &command, const std::string &args)
{
	std::string target = prefix;
	if (prefix.find('#') != std::string::npos)
		target = prefix;

	if (command == "help")
		cmdHelp(target);
	else if (command == "time")
		cmdTime(target);
	else if (command == "echo")
		cmdEcho(target, args);
	else if (command == "uptime")
		cmdUptime(target);
	else if (command == "join")
		cmdJoin(target, args);
	else if (command == "users")
		cmdUsers(target, args);
	else
		sendCommand("PRIVMSG " + target + " :Unknown command. Use !help for available commands");
}

void Bot::handleMessage(const std::string &line)
{
	std::cout << "[BOT] << " << line << std::endl;

	if (line.empty())
		return;

	std::string prefix;
	std::string rest;
	std::string command;
	std::string args;

	if (line[0] == ':')
	{
		size_t space1 = line.find(' ');
		if (space1 == std::string::npos)
			return;

		prefix = line.substr(1, space1 - 1);
		rest = line.substr(space1 + 1);
	}
	else
	{
		prefix = "";
		rest = line;
	}

	size_t space2 = rest.find(' ');
	command = (space2 != std::string::npos) ? rest.substr(0, space2) : rest;
	args = (space2 != std::string::npos) ? rest.substr(space2 + 1) : "";

	std::cout << "[BOT] Parsed: prefix=" << prefix << " command=" << command << " args=" << args << std::endl;

	// Handle 001 (welcome)
	if (command == "001")
	{
		_registered = true;
		std::cout << "[BOT] Registration successful, joining channel" << std::endl;
		sendCommand("JOIN " + _channel);
	}
	else if (command == "PRIVMSG")
	{
		size_t space3 = args.find(' ');
		if (space3 != std::string::npos)
		{
			std::string target_chan = args.substr(0, space3);
			std::string msg_part = args.substr(space3 + 1);

			std::string msg = (msg_part[0] == ':') ? msg_part.substr(1) : msg_part;

			size_t bang = prefix.find('!');
			std::string sender = (bang != std::string::npos) ? prefix.substr(0, bang) : prefix;

			std::cout << "[BOT] PRIVMSG from " << sender << " to " << target_chan << ": " << msg << std::endl;

			if (!msg.empty() && msg[0] == '!')
			{
				size_t space4 = msg.find(' ');
				std::string cmd_name = msg.substr(1, (space4 != std::string::npos) ? space4 - 1 : std::string::npos);
				std::string cmd_args = (space4 != std::string::npos) ? msg.substr(space4 + 1) : "";

				std::cout << "[BOT] Executing command: " << cmd_name << std::endl;

				std::string respond_to = (target_chan[0] == '#') ? target_chan : sender;
				parseCommand(respond_to, cmd_name, cmd_args);
			}
		}
	}
	else if (command == "JOIN")
	{
		if (prefix != _nickname)
		{
			std::cout << "[BOT] User " << prefix << " joined " << args << std::endl;
			sendCommand("PRIVMSG " + args + " :Welcome " + prefix + "! Type !help for bot commands.");
		}
	}
}

void Bot::run()
{
	if (connect() < 0)
	{
		std::cerr << "[BOT] Connection failed" << std::endl;
		return;
	}

	pollfd pfd;
	pfd.fd = _socket;
	pfd.events = POLLIN | POLLOUT;

	bool connection_ready = false;
	bool auth_sent = false;

	std::cout << "[BOT] Running..." << std::endl;

	while (isConnected())
	{
		pfd.revents = 0;
		int poll_ret = poll(&pfd, 1, 5000); // 5 second timeout

		if (poll_ret < 0)
		{
			std::cerr << "[BOT] Poll error" << std::endl;
			break;
		}

		if (poll_ret == 0)
		{
			// Timeout
			continue;
		}

		// Check for errors
		if (pfd.revents & POLLERR)
		{
			std::cerr << "[BOT] Socket error" << std::endl;
			break;
		}

		if (pfd.revents & POLLHUP)
		{
			std::cout << "[BOT] Server closed connection" << std::endl;
			break;
		}

		// Connection ready for writing
		if (pfd.revents & POLLOUT)
		{
			if (!connection_ready)
			{
				connection_ready = true;
				std::cout << "[BOT] Connected to server" << std::endl;
			}
			
			// Send authentication once connection is ready
			if (!auth_sent)
			{
				auth_sent = true;
				sendCommand("PASS " + _password);
				sendCommand("NICK " + _nickname);
				sendCommand("USER " + _nickname + " 0 * :IRC Bot");
			}
		}

		// Read data
		if (pfd.revents & POLLIN)
		{
			char buf[512];
			ssize_t n = recv(_socket, buf, sizeof(buf) - 1, 0);

			if (n <= 0)
			{
				if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
					continue;
				std::cout << "[BOT] Disconnected" << std::endl;
				break;
			}

			buf[n] = '\0';
			_buffer += buf;

			// Process complete lines
			size_t pos;
			while ((pos = _buffer.find("\r\n")) != std::string::npos)
			{
				std::string line = _buffer.substr(0, pos);
				_buffer = _buffer.substr(pos + 2);
				if (!line.empty())
					handleMessage(line);
			}
		}
	}

	disconnect();
	std::cout << "[BOT] Shutting down" << std::endl;
}
