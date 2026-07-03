/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: marvin <marvin@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/18 18:04:01 by bhamoum           #+#    #+#             */
/*   Updated: 2026/07/03 12:07:13 by marvin           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <fcntl.h>
#include <errno.h>
#include <cstdio>
#include <cstdlib>

Server::Server(int port, const std::string &password) : _port(port), _password(password)
{
	setupSocket();
}

/* ---------- Command handler implementations ---------- */
bool Server::handlePASS(int sock, ClientInfo &client, const std::string &command, size_t index)
{
	if (command.compare(0, 5, "PASS ") != 0)
	{
		std::string reply = ":ircserv 464 :You must send PASS first\r\n";
		sendAll(sock, reply);
		return false;
	}

	std::string pass = command.substr(5);
	if (pass == _password)
	{
		client.passwordAccepted = true;
		
		return false;
	}

	std::string reply = ":ircserv 464 :Password incorrect\r\n";
	sendAll(sock, reply);
	close(sock);
	_clients.erase(sock);
	_poll_fd.erase(_poll_fd.begin() + index);
	return true;
}

bool Server::handleNICK(int sock, ClientInfo &client, const std::string &command)
{
	if (command.size() <= 5)
	{
		std::string reply = ":ircserv 431 :No nickname given\r\n";
		sendAll(sock, reply);
		return false;
	}

	std::string newNick = command.substr(5);
	if (isNickInUse(newNick))
	{
		std::string reply = ":ircserv 433 " + newNick + " :Nickname is already in use\r\n";
		sendAll(sock, reply);
		return false;
	}

	std::string oldNick = client.nickname;
	client.nickname = newNick;
	client.hasNick = true;

	std::string nickMsg = ":" + oldNick + " NICK " + newNick + "\r\n";
	std::set<int> notified;

	for (std::map<std::string, Channel>::iterator it = _channels.begin(); it != _channels.end(); ++it)
	{
		if (it->second.hasMember(sock))
		{
			it->second.updateNickname(sock, newNick);
			const std::set<int> &members = it->second.getMembers();
			for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit)
			{
				if (notified.find(*mit) == notified.end())
				{
					sendAll(*mit, nickMsg);
					notified.insert(*mit);
				}
			}
		}
	}

	std::cout << "Client " << sock << " changed NICK from " << oldNick << " to " << newNick << std::endl;
	return false;
}


bool Server::handleUSER(int sock, ClientInfo &client, const std::string &command)
{
	if (command.size() <= 5)
	{
		std::string reply = ":ircserv 461 USER :Not enough parameters\r\n";
		sendAll(sock, reply);
		return false;
	}

	std::string args = command.substr(5);
	std::string username;
	size_t spacePos = args.find(' ');
	if (spacePos != std::string::npos)
		username = args.substr(0, spacePos);
	else
		username = args;

	client.username = username;
	client.hasUser = true;
	std::cout << "Client " << sock << " set USER to " << username << std::endl;
	return false;
}

bool Server::handleJOIN(int sock, ClientInfo &client, const std::string &command)
{
	std::string args = command.substr(5);
	std::string channelName = args;
	std::string providedKey = "";
	size_t sep = args.find(' ');
	if (sep != std::string::npos)
	{
		channelName = args.substr(0, sep);
		providedKey = args.substr(sep + 1);
	}

    if (channelName.empty() || channelName[0] != '#')
    {
        sendAll(sock, ":ircserv 403 " + channelName + " :No such channel\r\n");
        return false;
    }
    std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	bool isFirstMember = false;
	if (it == _channels.end())
	{
		_channels.insert(std::make_pair(channelName, Channel(channelName)));
		it = _channels.find(channelName);
		isFirstMember = true;
	}

	Channel &channel = it->second;

	if (!isFirstMember)
	{
		if (channel.isInviteOnly() && !channel.isInvited(sock))
		{
			sendAll(sock, ":ircserv 473 " + client.nickname + " " + channelName + " :Cannot join channel (+i)\r\n");
			return false;
		}

		if (channel.hasKey() && providedKey != channel.getKey())
		{
			sendAll(sock, ":ircserv 475 " + client.nickname + " " + channelName + " :Cannot join channel (+k)\r\n");
			return false;
		}

		if (channel.getMemberLimit() > 0 && (int)channel.getMemberCount() >= channel.getMemberLimit())
		{
			sendAll(sock, ":ircserv 471 " + client.nickname + " " + channelName + " :Cannot join channel (+l)\r\n");
			return false;
		}
	}

	channel.removeInvite(sock);

	if (isFirstMember || channel.getMemberCount() == 0)
	{
		channel.addMember(sock, client.nickname);
		channel.addOperator(sock);
	}
	else
	{
		channel.addMember(sock, client.nickname);
	}

	std::string joinMsg = ":" + client.nickname + " JOIN " + channelName + "\r\n";
	const std::set<int> &members = channel.getMembers();
	for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit)
	{
		int memberSock = *mit;
		sendAll(memberSock, joinMsg);
	}

	std::string namesList = channel.getMemberListWithModes();
	std::string namesReply = ":ircserv 353 " + client.nickname + " = " + channelName + " :" + namesList + "\r\n";
	sendAll(sock, namesReply);
	std::string endNamesReply = ":ircserv 366 " + client.nickname + " " + channelName + " :End of NAMES list\r\n";
	sendAll(sock, endNamesReply);

	if (!channel.getTopic().empty())
	{
		std::string topicReply = ":ircserv 332 " + client.nickname + " " + channelName + " :" + channel.getTopic() + "\r\n";
		sendAll(sock, topicReply);
	}

	std::cout << client.nickname << " joined " << channelName << std::endl;
	return false;
}

bool Server::handlePART(int sock, ClientInfo &client, const std::string &command)
{
	std::string channelName = command.substr(5);

	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end())
	{
		std::string reply = ":ircserv 403 " + channelName + " :No such channel\r\n";
		sendAll(sock, reply);
		return false;
	}

	Channel &channel = it->second;

	std::string partMsg = ":" + client.nickname + " PART " + channelName + "\r\n";
	const std::set<int> &members = channel.getMembers();

	for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit)
	{
		if (*mit != sock)
			sendAll(*mit, partMsg);
	}
	sendAll(sock, partMsg);
	channel.removeMember(sock);
	return false;
}


bool Server::handleTOPIC(int sock, ClientInfo &client, const std::string &command)
{
	size_t space = command.find(' ');
	if (space == std::string::npos)
		return false;

	std::string channelName = command.substr(space + 1);

	
	size_t topicSpace = channelName.find(' ');
	std::string topic;
	if (topicSpace != std::string::npos)
	{
		topic = channelName.substr(topicSpace + 1);
		channelName = channelName.substr(0, topicSpace);
		if (!topic.empty() && topic[0] == ':')
			topic.erase(0, 1);
	}

	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end())
	{
		std::string reply = ":ircserv 403 " + channelName + " :No such channel\r\n";
		sendAll(sock, reply);
		return false;
	}

	Channel &channel = it->second;

	
	if (topicSpace == std::string::npos)
	{
		if (channel.getTopic().empty())
		{
			std::string reply = ":ircserv 331 " + client.nickname + " " + channelName + " :No topic is set\r\n";
			sendAll(sock, reply);
		}
		else
		{
			std::string reply = ":ircserv 332 " + client.nickname + " " + channelName + " :" + channel.getTopic() + "\r\n";
			sendAll(sock, reply);
		}
		return false;
	}

	
	if (channel.isTopicRestricted() && !channel.isOperator(sock))
	{
		std::string reply = ":ircserv 482 " + channelName + " :You're not channel operator\r\n";
		sendAll(sock, reply);
		return false;
	}

	channel.setTopic(topic, client.nickname);
	std::string topicMsg = ":" + client.nickname + " TOPIC " + channelName + " :" + topic + "\r\n";
	const std::set<int> &members = channel.getMembers();
	for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit)
		sendAll(*mit, topicMsg);

	return false;
}

bool Server::handleMODE(int sock, ClientInfo &client, const std::string &command)
{
	size_t firstSpace = command.find(' ');
	if (firstSpace == std::string::npos)
		return false;

	std::string target = command.substr(firstSpace + 1);
	size_t targetEnd = target.find(' ');
	if (targetEnd != std::string::npos)
		target = target.substr(0, targetEnd);

	if (target.empty() || target[0] != '#')
		return false;

	std::map<std::string, Channel>::iterator it = _channels.find(target);
	if (it == _channels.end())
	{
		std::string reply = ":ircserv 403 " + target + " :No such channel\r\n";
		sendAll(sock, reply);
		return false;
	}

	Channel &channel = it->second;

	if (!channel.isOperator(sock))
	{
		std::string reply = ":ircserv 482 " + target + " :You're not channel operator\r\n";
		sendAll(sock, reply);
		return false;
	}

	size_t modeStart = command.find(target) + target.size();
	if (modeStart >= command.size() || command[modeStart] != ' ')
		return false;

	modeStart++;

	std::string modeStr;
	size_t modeEnd = modeStart;
	while (modeEnd < command.size() && command[modeEnd] != ' ')
		modeEnd++;

	modeStr = command.substr(modeStart, modeEnd - modeStart);
	std::string params = (modeEnd < command.size()) ? command.substr(modeEnd + 1) : "";

	bool add = true;

	for (size_t i = 0; i < modeStr.size(); ++i)
	{
		char mode = modeStr[i];

		if (mode == '+')
		{
			add = true;
		}
		else if (mode == '-')
		{
			add = false;
		}
		else if (mode == 'i')
		{
			channel.setInviteOnly(add);
		}
		else if (mode == 't')
		{
			channel.setTopicRestricted(add);
		}
		else if (mode == 'k')
		{
                    
			if (add)
			{
				size_t paramSpace = params.find(' ');
				std::string key = (paramSpace != std::string::npos) 
					? params.substr(0, paramSpace) 
					: params;
				if (!key.empty())
					channel.setKey(key);
			}
			else
			{
				channel.clearKey();
			}
		}
		else if (mode == 'o')
		{
			size_t paramSpace = params.find(' ');
			std::string nick = (paramSpace != std::string::npos) 
				? params.substr(0, paramSpace) 
				: params;
			if (!nick.empty())
			{
				for (std::map<int, ClientInfo>::iterator cit = _clients.begin(); 
				     cit != _clients.end(); ++cit)
				{
					if (cit->second.nickname == nick && channel.hasMember(cit->first))
					{
						if (add)
							channel.addOperator(cit->first);
						else
							channel.removeOperator(cit->first);
						break;
					}
				}
			}
		}
		else if (mode == 'l')
		{
			if (add)
			{
				size_t paramSpace = params.find(' ');
				std::string limitStr = (paramSpace != std::string::npos) 
					? params.substr(0, paramSpace) 
					: params;
				if (!limitStr.empty())
				{
					int limit = atoi(limitStr.c_str());
					channel.setMemberLimit(limit);
				}
			}
			else
			{
				channel.setMemberLimit(0);
			}
		}
	}

	std::string modeMsg = ":" + client.nickname + " MODE " + target + " " + modeStr;
	if (!params.empty())
		modeMsg += " " + params;
	modeMsg += "\r\n";

	const std::set<int> &members = channel.getMembers();
	for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit)
		sendAll(*mit, modeMsg);

	return false;
}

bool Server::handleKICK(int sock, ClientInfo &client, const std::string &command)
{
	// KICK #channel nick [reason]
	size_t firstSpace = command.find(' ');
	if (firstSpace == std::string::npos)
		return false;

	size_t secondSpace = command.find(' ', firstSpace + 1);
	std::string channelName = command.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	std::string target = (secondSpace != std::string::npos) ? command.substr(secondSpace + 1) : "";

	size_t thirdSpace = target.find(' ');
	std::string reason = "";
	if (thirdSpace != std::string::npos)
	{
		reason = target.substr(thirdSpace + 1);
		target = target.substr(0, thirdSpace);
		if (!reason.empty() && reason[0] == ':')
			reason.erase(0, 1);
	}

	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end())
	{
		std::string reply = ":ircserv 403 " + channelName + " :No such channel\r\n";
		sendAll(sock, reply);
		return false;
	}

	Channel &channel = it->second;

	if (!channel.isOperator(sock))
	{
		std::string reply = ":ircserv 482 " + channelName + " :You're not channel operator\r\n";
		sendAll(sock, reply);
		return false;
	}

	int targetSock = -1;
	for (std::map<int, ClientInfo>::iterator cit = _clients.begin(); cit != _clients.end(); ++cit)
	{
		if (cit->second.nickname == target && channel.hasMember(cit->first))
		{
			targetSock = cit->first;
			break;
		}
	}

	if (targetSock == -1)
	{
		std::string reply = ":ircserv 401 " + target + " :No such nick\r\n";
		sendAll(sock, reply);
		return false;
	}

	std::string kickMsg = ":" + client.nickname + " KICK " + channelName + " " + target;
	if (!reason.empty())
		kickMsg += " :" + reason;
	kickMsg += "\r\n";

	const std::set<int> &members = channel.getMembers();
	for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit)
		sendAll(*mit, kickMsg);

	channel.removeMember(targetSock);

	return false;
}


bool Server::handleINVITE(int sock, ClientInfo &client, const std::string &command)
{
	size_t space = command.find(' ');
	if (space == std::string::npos)
		return false;

	std::string args = command.substr(space + 1);
	space = args.find(' ');
	if (space == std::string::npos)
		return false;

	std::string target = args.substr(0, space);
	std::string channelName = args.substr(space + 1);

	std::map<std::string, Channel>::iterator it = _channels.find(channelName);
	if (it == _channels.end())
	{
		std::string reply = ":ircserv 403 " + channelName + " :No such channel\r\n";
		sendAll(sock, reply);
		return false;
	}

	Channel &channel = it->second;

	if (channel.isInviteOnly() && !channel.isOperator(sock))
	{
		std::string reply = ":ircserv 482 " + channelName + " :You're not channel operator\r\n";
		sendAll(sock, reply);
		return false;
	}

	int targetSock = -1;
	for (std::map<int, ClientInfo>::iterator cit = _clients.begin(); cit != _clients.end(); ++cit)
	{
		if (cit->second.nickname == target)
		{
			targetSock = cit->first;
			break;
		}
	}

	if (targetSock == -1)
	{
		std::string reply = ":ircserv 401 " + target + " :No such nick\r\n";
		sendAll(sock, reply);
		return false;
	}

	std::string inviteMsg = ":" + client.nickname + " INVITE " + target + " " + channelName + "\r\n";
	sendAll(targetSock, inviteMsg);
	channel.addInvite(targetSock);

	std::string confirmMsg = ":ircserv 341 " + client.nickname + " " + target + " " + channelName + "\r\n";
	sendAll(sock, confirmMsg);

	return false;
}

bool Server::handleLIST(int sock, ClientInfo &client, const std::string &command)
{
	std::string args = command.substr(4);

	std::string startMsg = ":ircserv 321 " + client.nickname + " Channel :Users Name\r\n";
	sendAll(sock, startMsg);

	if (args.empty() || args[0] == ' ')
	{
		for (std::map<std::string, Channel>::const_iterator it = _channels.begin();
			 it != _channels.end(); ++it)
		{
			const Channel &channel = it->second;
			char countStr[32];
			snprintf(countStr, sizeof(countStr), "%u", (unsigned int)channel.getMemberCount());
			std::string listMsg = ":ircserv 322 " + client.nickname + " " + channel.getName() + " " +
								  countStr + " :" + channel.getTopic() + "\r\n";
			sendAll(sock, listMsg);
		}
	}
	else
	{
		std::string channelName = args;
		if (channelName[0] == ' ')
			channelName = channelName.substr(1);

		std::map<std::string, Channel>::const_iterator it = _channels.find(channelName);
		if (it != _channels.end())
		{
			const Channel &channel = it->second;
			char countStr[32];
			snprintf(countStr, sizeof(countStr), "%u", (unsigned int)channel.getMemberCount());
			std::string listMsg = ":ircserv 322 " + client.nickname + " " + channel.getName() + " " +
								  countStr + " :" + channel.getTopic() + "\r\n";
			sendAll(sock, listMsg);
		}
	}

	std::string endMsg = ":ircserv 323 " + client.nickname + " :End of LIST\r\n";
	sendAll(sock, endMsg);

	return false;
}

bool Server::handlePRIVMSG(int sock, ClientInfo &client, const std::string &command)
{
	size_t firstSpace = command.find(' ');
	if (firstSpace == std::string::npos)
		return false;

	size_t secondSpace = command.find(' ', firstSpace + 1);
	if (secondSpace == std::string::npos)
		return false;

	std::string target = command.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	std::string message = command.substr(secondSpace + 1);
	if (!message.empty() && message[0] == ':')
		message.erase(0, 1);

	std::string msgToSend = ":" + client.nickname + " PRIVMSG " + target + " :" + message + "\r\n";

	/* -------- Channel message -------- */
	if (!target.empty() && target[0] == '#')
	{
		std::map<std::string, Channel>::iterator it = _channels.find(target);
		if (it == _channels.end())
		{
			std::string reply = ":ircserv 403 " + target + " :No such channel\r\n";
			sendAll(sock, reply);
			return false;
		}

		Channel &channel = it->second;

		if (!channel.hasMember(sock))
		{
			std::string reply = ":ircserv 442 " + target + " :You're not on that channel\r\n";
			sendAll(sock, reply);
			return false;
		}

		const std::set<int> &members = channel.getMembers();
		for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit)
		{
			if (*mit != sock)
				sendAll(*mit, msgToSend);
		}
	}
	/* -------- User message -------- */
	else
	{
		bool found = false;
		for (std::map<int, ClientInfo>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		{
			if (it->second.nickname == target)
			{
				sendAll(it->second.socket, msgToSend);
				found = true;
				break;
			}
		}

		if (!found)
		{
			std::string reply = ":ircserv 401 " + target + " :No such nick\r\n";
			sendAll(sock, reply);
		}
	}

	return false;
}

bool Server::handleNOTICE(int sock, ClientInfo &client, const std::string &command)
{
	size_t firstSpace = command.find(' ');
	if (firstSpace == std::string::npos)
		return false;

	size_t secondSpace = command.find(' ', firstSpace + 1);
	if (secondSpace == std::string::npos)
		return false;

	std::string target = command.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	std::string message = command.substr(secondSpace + 1);
	if (!message.empty() && message[0] == ':')
		message.erase(0, 1);

	std::string msgToSend = ":" + client.nickname + " NOTICE " + target + " :" + message + "\r\n";

	/* -------- Channel notice -------- */
	if (!target.empty() && target[0] == '#')
	{
		std::map<std::string, Channel>::iterator it = _channels.find(target);
		if (it == _channels.end())
			return false; // NOTICE never replies with an error, per RFC 2812

		Channel &channel = it->second;
		if (!channel.hasMember(sock))
			return false;

		const std::set<int> &members = channel.getMembers();
		for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit)
		{
			if (*mit != sock)
				sendAll(*mit, msgToSend);
		}
	}
	/* -------- User notice -------- */
	else
	{
		for (std::map<int, ClientInfo>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		{
			if (it->second.nickname == target)
			{
				sendAll(it->second.socket, msgToSend);
				break;
			}
		}
		// silently dropped if the nick doesn't exist — no error reply for NOTICE
	}

	return false;
}

bool Server::handleQUIT(int sock, ClientInfo &client, const std::string &command, size_t index)
{
	std::string quitMsg = ":" + client.nickname + " QUIT";
	if (command.size() > 5)
		quitMsg += " :" + command.substr(5);
	else
		quitMsg += " :Client Quit";
	quitMsg += "\r\n";

	for (std::map<std::string, Channel>::iterator cit = _channels.begin(); cit != _channels.end(); ++cit)
	{
		Channel &channel = cit->second;
		if (channel.hasMember(sock))
		{
			const std::set<int> &members = channel.getMembers();
			for (std::set<int>::const_iterator mit = members.begin(); mit != members.end(); ++mit)
			{
				if (*mit != sock)
					sendAll(*mit, quitMsg);
			}
			channel.removeMember(sock);
		}
	}

	std::cout << "Client quit: " << client.nickname << std::endl;
	close(sock);
	_clients.erase(sock);
	_poll_fd.erase(_poll_fd.begin() + index);
	return true;
}
Server::~Server()
{
	for (size_t i = 0; i < _poll_fd.size(); ++i)
	{
		if (_poll_fd[i].fd != -1)
			close(_poll_fd[i].fd);
	}
	std::cout << "Server shut down." << std::endl;
}

void Server::setupSocket()
{
	_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_server_fd < 0)
		throw std::runtime_error("Socket creation failed");

	if (setNonBlocking(_server_fd) < 0)
		throw std::runtime_error("Failed to set non-blocking on server socket");

	int opt = 1;
	if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		throw std::runtime_error("setsockopt failed");

	sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(_port);

	if (bind(_server_fd, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
		throw std::runtime_error("Bind failed");

	if (listen(_server_fd, 10) < 0)
		throw std::runtime_error("Listen failed");

	std::cout << "Server listening on port " << _port << std::endl;

	pollfd pfd;
	pfd.fd = _server_fd;
	pfd.events = POLLIN;
	_poll_fd.push_back(pfd);
}

void Server::acceptNewClient()
{
	int new_socket = accept(_server_fd, NULL, NULL);
	if (new_socket < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			std::cerr << "Accept failed: " << strerror(errno) << std::endl;
		return;
	}

	if (setNonBlocking(new_socket) < 0)
	{
		std::cerr << "Failed to set non-blocking on client socket" << std::endl;
		close(new_socket);
		return;
	}

	pollfd pfd;
	pfd.fd = new_socket;
	pfd.events = POLLIN;
	_poll_fd.push_back(pfd);

	ClientInfo client;
	client.socket = new_socket;
	client.passwordAccepted = false;
	client.hasNick = false;
	client.hasUser = false;
	client.isRegistered = false;
	_clients[new_socket] = client;

	std::cout << "New client connected: " << new_socket << std::endl;
}

bool Server::handleClientMessage(size_t index)
{
	int sock = _poll_fd[index].fd;
	char buffer[BUFFER_SIZE];

	ssize_t bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
	if (bytes_read == 0)
	{
		std::cout << "Client disconnected: " << sock << std::endl;
		close(sock);
		_clients.erase(sock);
		_poll_fd.erase(_poll_fd.begin() + index);
		return true;
	}
	else if (bytes_read < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			{
				return false;
			}
		std::cerr << "Read error on " << sock << ": " << strerror(errno) << std::endl;
		close(sock);
		_clients.erase(sock);
		_poll_fd.erase(_poll_fd.begin() + index);
		return true;
	}

	ClientInfo &client = _clients[sock];

	client.buffer.append(buffer, bytes_read);

	size_t pos;
	while ((pos = client.buffer.find("\r\n")) != std::string::npos)
	{
		std::string command = client.buffer.substr(0, pos);
		client.buffer.erase(0, pos + 2);

		std::cout << "Command from " << sock << ": " << command << std::endl;
        
		if (!client.passwordAccepted)
		{
			bool removed = handlePASS(sock, client, command, index);
			if (removed)
				return true;
			continue;
		}

        
		if (command.compare(0, 4, "PING") == 0)
		{
			std::string token = (command.size() > 5) ? command.substr(5) : "ping";
			sendAll(sock, ":ircserv PONG " + token + "\r\n");
			continue;
		}

        
		if (command.compare(0, 4, "NICK") == 0)
		{
			if (handleNICK(sock, client, command))
				return true;
			continue;
		}

        
		if (command.compare(0, 4, "USER") == 0)
		{
			if (handleUSER(sock, client, command))
				return true;
			
			
			if (!client.isRegistered && client.hasNick && client.hasUser)
			{
				client.isRegistered = true;
				std::string welcome = ":ircserv 001 " + client.nickname + " :Welcome to the IRC server\r\n";
				sendAll(sock, welcome);
				std::cout << "Client " << sock << " fully registered as " << client.nickname << std::endl;
			}
			continue;
		}

        
		if (!client.isRegistered && client.hasNick && client.hasUser)
		{
			client.isRegistered = true;
			std::string welcome = ":ircserv 001 " + client.nickname + " :Welcome to the IRC server\r\n";
			sendAll(sock, welcome);
			std::cout << "Client " << sock << " fully registered as " << client.nickname << std::endl;
		}

        
		if (!client.isRegistered)
		{
			std::string cmd;
			size_t space = command.find(' ');
			if (space != std::string::npos)
				cmd = command.substr(0, space);
			else
				cmd = command;

			if (cmd == "JOIN" || cmd == "PART" || cmd == "PRIVMSG")
			{
				std::string reply = ":ircserv 451 " + cmd + " :You have not registered\r\n";
				sendAll(sock, reply);
				continue;
			}
		}

        
		if (command.compare(0, 4, "JOIN") == 0)
		{
			if (handleJOIN(sock, client, command))
				return true;
			continue;
		}

        
		if (command.compare(0, 4, "PART") == 0)
		{
			if (handlePART(sock, client, command))
				return true;
			continue;
		}

        
		if (command.compare(0, 7, "PRIVMSG") == 0)
		{
			if (handlePRIVMSG(sock, client, command))
				return true;
			continue;
		}

        
		if (command.compare(0, 6, "NOTICE") == 0)
		{
			if (handleNOTICE(sock, client, command))
				return true;
			continue;
		}

        
		if (command.compare(0, 5, "TOPIC") == 0)
		{
			if (handleTOPIC(sock, client, command))
				return true;
			continue;
		}

        
		if (command.compare(0, 4, "MODE") == 0)
		{
			if (handleMODE(sock, client, command))
				return true;
			continue;
		}

        
		if (command.compare(0, 4, "KICK") == 0)
		{
			if (handleKICK(sock, client, command))
				return true;
			continue;
		}

        
		if (command.compare(0, 6, "INVITE") == 0)
		{
			if (handleINVITE(sock, client, command))
				return true;
			continue;
		}

        
		if (command.compare(0, 4, "LIST") == 0)
		{
			if (handleLIST(sock, client, command))
				return true;
			continue;
		}

        
		if (command.compare(0, 3, "DCC") == 0)
		{
			if (handleDCC(sock, client, command))
				return true;
			continue;
		}

        
		if (command.compare(0, 4, "QUIT") == 0)
		{
			if (handleQUIT(sock, client, command, index))
				return true;
			continue;
		}

	}

	return false;
}

void Server::run()
{
	while (true)
	{
		_dcc_manager.checkTimeouts();

		std::map<int, DCCTransfer> &transfers = _dcc_manager.getTransfers();
		for (std::map<int, DCCTransfer>::iterator it = transfers.begin(); it != transfers.end(); ++it)
		{
			if (it->second.state == DCC_CONNECTED && it->second.data_socket >= 0)
				processDCCTransfer(it->first);
		}

		int poll_count = poll(_poll_fd.data(), _poll_fd.size(), -1);
		if (poll_count < 0)
		{
			std::cerr << "Poll error" << std::endl;
			break;
		}

			
		size_t i = 0;
		while (i < _poll_fd.size())
		{
			short re = _poll_fd[i].revents;
			int fd = _poll_fd[i].fd;

			if (re & (POLLHUP | POLLERR | POLLNVAL))
			{
					
				if (fd != _server_fd)
				{
					close(fd);
					_clients.erase(fd);
				}
				_poll_fd.erase(_poll_fd.begin() + i);
					continue;
			}

					
			if (_dcc_listening_sockets.find(fd) != _dcc_listening_sockets.end())
			{
				if (re & POLLIN)
				{
					processDCCListening();
				}
				++i;
				continue;
			}

			if (re & POLLIN)
			{
				if (fd == _server_fd)
					acceptNewClient();
				else
				{
					bool removed = handleClientMessage(i);
					if (removed)
							continue;
				}
			}
			++i;
		}
	}
}

bool Server::isNickInUse(const std::string &nick) const
{
	for (std::map<int, ClientInfo>::const_iterator it = _clients.begin();
		 it != _clients.end(); ++it)
	{
		if (it->second.hasNick && it->second.nickname == nick)
			return true;
	}
	return false;
}

bool Server::sendAll(int sock, const std::string &data)
{
	const char *buf = data.c_str();
	size_t total = 0;
	size_t len = data.size();

	while (total < len)
	{
		ssize_t sent = send(sock, buf + total, len - total, 0);
		if (sent < 0)
		{
			if (errno == EINTR)
				continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
						return false;
			return false;
		}
		total += sent;
	}
	return true;
}

int Server::setNonBlocking(int fd)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		return -1;
	return 0;
}

// int Server::setNonBlocking(int fd)
// {
// 	int flags = fcntl(fd, F_GETFL, 0);
// 	if (flags == -1)
// 		return -1;
// 	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
// 		return -1;
// 	return 0;
// }

std::string Server::getClientIPAddress(int sock)
{
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);

	if (getpeername(sock, (struct sockaddr *)&addr, &addr_len) < 0)
		return "0.0.0.0";

	return inet_ntoa(addr.sin_addr);
}

bool Server::createDCCListeningSocket(int transfer_id)
{
	DCCTransfer *transfer = _dcc_manager.getTransfer(transfer_id);
	if (!transfer)
		return false;

	int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock < 0)
	{
		std::cerr << "Failed to create DCC listening socket" << std::endl;
		return false;
	}

	if (setNonBlocking(listen_sock) < 0)
	{
		close(listen_sock);
		return false;
	}

	int port = _dcc_manager.getAvailablePort();
	if (port < 0)
	{
		close(listen_sock);
		return false;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		std::cerr << "Failed to bind DCC socket" << std::endl;
		close(listen_sock);
		return false;
	}

	if (listen(listen_sock, 1) < 0)
	{
		close(listen_sock);
		return false;
	}

	transfer->listening_socket = listen_sock;
	transfer->listening_port = port;
	transfer->state = DCC_LISTENING;

	pollfd pfd;
	pfd.fd = listen_sock;
	pfd.events = POLLIN;
	_poll_fd.push_back(pfd);

	_dcc_listening_sockets[listen_sock] = transfer_id;

	std::cout << "DCC: Listening for transfer " << transfer_id << " on port " << port << std::endl;
	return true;
}

void Server::processDCCListening()
{
	std::map<int, int>::iterator it = _dcc_listening_sockets.begin();

	while (it != _dcc_listening_sockets.end())
	{
		int listen_sock = it->first;
		int transfer_id = it->second;

		for (size_t i = 0; i < _poll_fd.size(); ++i)
		{
			if (_poll_fd[i].fd == listen_sock)
			{
				if (_poll_fd[i].revents & POLLIN)
				{
					struct sockaddr_in client_addr;
					socklen_t client_len = sizeof(client_addr);
					int data_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);

					if (data_sock >= 0)
					{
						setNonBlocking(data_sock);
						DCCTransfer *transfer = _dcc_manager.getTransfer(transfer_id);
						if (transfer)
						{
							transfer->data_socket = data_sock;
							transfer->state = DCC_CONNECTED;
							std::cout << "DCC: Receiver connected for transfer " << transfer_id << std::endl;

							transfer->file_handle = fopen(transfer->filepath.c_str(), "rb");
							if (!transfer->file_handle)
							{
								std::cerr << "DCC: Failed to open file " << transfer->filepath << std::endl;
								transfer->state = DCC_FAILED;
								close(data_sock);
								transfer->data_socket = -1;
							}
						}
					}
				}
				break;
			}
		}

		++it;
	}
}

void Server::processDCCTransfer(int transfer_id)
{
	DCCTransfer *transfer = _dcc_manager.getTransfer(transfer_id);
	if (!transfer || transfer->data_socket < 0)
		return;

	if (transfer->state != DCC_CONNECTED)
		return;

	for (size_t i = 0; i < _poll_fd.size(); ++i)
	{
		if (_poll_fd[i].fd == transfer->data_socket)
		{
					if (_poll_fd[i].revents & POLLOUT)
					{
				char buffer[4096];
				size_t bytes_read = fread(buffer, 1, sizeof(buffer), transfer->file_handle);

				if (bytes_read > 0)
				{
					ssize_t sent = send(transfer->data_socket, buffer, bytes_read, 0);
					if (sent > 0)
					{
						transfer->bytes_sent += sent;
						transfer->last_activity = time(NULL);
						std::cout << "DCC: Sent " << sent << " bytes (total: " << transfer->bytes_sent << "/"
						          << transfer->filesize << ")" << std::endl;

						if (transfer->bytes_sent >= transfer->filesize)
						{
							transfer->state = DCC_COMPLETED;
							std::cout << "DCC: Transfer " << transfer_id << " completed" << std::endl;
						}
					}
				}
				else if (feof(transfer->file_handle))
				{
					transfer->state = DCC_COMPLETED;
				}
			}
			break;
		}
	}
}

bool Server::handleDCC(int sock, ClientInfo &client, const std::string &command)
{
	if (command.compare(0, 7, "DCC SEN") != 0)
		return false;

    
	size_t space1 = command.find(' ');
	if (space1 == std::string::npos)
		return false;

	size_t space2 = command.find(' ', space1 + 1);
	if (space2 == std::string::npos)
		return false;

	size_t space3 = command.find(' ', space2 + 1);
	std::string receiver_nick = command.substr(space2 + 1, (space3 != std::string::npos) ? space3 - space2 - 1 : std::string::npos);
	std::string filename = (space3 != std::string::npos) ? command.substr(space3 + 1) : "";

	if (receiver_nick.empty() || filename.empty())
	{
		sendAll(sock, "NOTICE " + client.nickname + " :Invalid DCC syntax\r\n");
		return false;
	}

    
	bool receiver_found = false;
	int receiver_sock = -1;
	for (std::map<int, ClientInfo>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second.nickname == receiver_nick)
		{
			receiver_found = true;
			receiver_sock = it->first;
			break;
		}
	}

	if (!receiver_found)
	{
		sendAll(sock, ":ircserv 401 " + receiver_nick + " :No such nick/channel\r\n");
		return false;
	}

    
	FILE *f = fopen(filename.c_str(), "rb");
	if (!f)
	{
		sendAll(sock, "NOTICE " + client.nickname + " :Cannot open file\r\n");
		return false;
	}
	fseek(f, 0, SEEK_END);
	size_t filesize = ftell(f);
	fclose(f);

    
	int transfer_id = _dcc_manager.createTransfer(client.nickname, receiver_nick, filename, filesize);
	DCCTransfer *transfer = _dcc_manager.getTransfer(transfer_id);
	if (transfer)
	{
		transfer->filepath = filename;
		transfer->is_sender = true;
	}

    
	if (!createDCCListeningSocket(transfer_id))
	{
		sendAll(sock, "NOTICE " + client.nickname + " :DCC setup failed\r\n");
		_dcc_manager.removeTransfer(transfer_id);
		return false;
	}

	std::string ip = getClientIPAddress(sock);
    
	unsigned int ip_num = inet_addr(ip.c_str());
	char dcc_msg[512];
	snprintf(dcc_msg, sizeof(dcc_msg), ":%s PRIVMSG %s :\001DCC SEND %s %u %d %zu\001\r\n", client.nickname.c_str(),
	         receiver_nick.c_str(), filename.c_str(), ip_num, transfer->listening_port, filesize);

	sendAll(receiver_sock, dcc_msg);

	sendAll(sock, "NOTICE " + client.nickname + " :DCC SEND initiated to " + receiver_nick + "\r\n");
	std::cout << "DCC: Transfer " << transfer_id << " initiated from " << client.nickname << " to " << receiver_nick
	          << std::endl;

	return false;
}
