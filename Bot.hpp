/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Bot.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bhamoum <bhamoum@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/19 17:15:02 by bhamoum           #+#    #+#             */
/*   Updated: 2026/01/19 18:50:07 by bhamoum          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef BOT_HPP
#define BOT_HPP

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <ctime>
#include <sstream>

/**
 * @class Bot
 * @brief IRC bot client that connects to a server and responds to user commands.
 *
 * This bot implements an IRC client that can:
 * - Connect to an IRC server with password authentication
 * - Register with a nickname and join channels
 * - Respond to user commands prefixed with '!' (!help, !time, !echo, etc.)
 * - Welcome new users joining channels
 * - Handle standard IRC protocol messages (PING, PRIVMSG, JOIN, etc.)
 *
 * The bot uses non-blocking sockets and poll() for I/O multiplexing.
 */
class Bot
{
private:
	std::string _server;
	int _port;
	std::string _password;
	std::string _nickname;
	std::string _channel;
	int _socket;
	std::string _buffer;
	bool _connected;
	bool _registered;
	time_t _start_time;

	/**
	 * @brief Establish TCP connection to the IRC server and perform initial handshake.
	 *
	 * Returns the socket descriptor on success or -1 on failure.
	 */
	int connect();

	/**
	 * @brief Check whether the bot currently has an active socket connection.
	 *
	 * @return true if connected, false otherwise
	 */
	bool isConnected() const;

	/**
	 * @brief Send a raw IRC protocol line to the server.
	 *
	 * Ensures CRLF termination and handles partial writes.
	 *
	 * @param message Raw message string
	 * @return true on success, false on failure
	 */
	bool sendRaw(const std::string &message);

	/**
	 * @brief Helper to send an IRC command (with CRLF appended).
	 *
	 * @param command Command string
	 * @return true on success, false on failure
	 */
	bool sendCommand(const std::string &command);

	/**
	 * @brief Close connection and reset internal state.
	 */
	void disconnect();

	/**
	 * @brief Handle a single incoming IRC message line from the server.
	 *
	 * Parses the line and dispatches to command handlers or responds to PING.
	 *
	 * @param line One IRC protocol line (without CRLF)
	 */
	void handleMessage(const std::string &line);

	/**
	 * @brief Parse a PRIVMSG or similar command for bot-invoked actions.
	 *
	 * @param prefix Message prefix (sender)
	 * @param command Command verb
	 * @param args Command arguments
	 */
	void parseCommand(const std::string &prefix, const std::string &command, const std::string &args);

	/**
	 * @brief Implementation of the `!help` bot command.
	 *
	 * @param target Channel or user to send the response to
	 */
	void cmdHelp(const std::string &target);

	/**
	 * @brief Implementation of the `!time` bot command.
	 *
	 * @param target Channel or user to send the response to
	 */
	void cmdTime(const std::string &target);

	/**
	 * @brief Implementation of the `!echo` bot command.
	 *
	 * @param target Channel or user to send the response to
	 * @param args The text to echo
	 */
	void cmdEcho(const std::string &target, const std::string &args);

	/**
	 * @brief Implementation of the `!uptime` bot command.
	 *
	 * @param target Channel or user to send the response to
	 */
	void cmdUptime(const std::string &target);

	/**
	 * @brief Implementation of the `!join` bot command.
	 *
	 * @param target Channel or user to send the response to
	 * @param args Channel to join
	 */
	void cmdJoin(const std::string &target, const std::string &args);

	/**
	 * @brief Implementation of the `!users` bot command.
	 *
	 * @param target Channel or user to send the response to
	 * @param args Channel name whose users to list
	 */
	void cmdUsers(const std::string &target, const std::string &args);

	/**
	 * @brief Trim whitespace from both ends of a string.
	 *
	 * @param str Input string
	 * @return Trimmed string
	 */
	std::string trim(const std::string &str);

	/**
	 * @brief Split a string by a single-character delimiter.
	 *
	 * @param str Input string
	 * @param delimiter Character to split on
	 * @return Vector of tokens
	 */
	std::vector<std::string> split(const std::string &str, char delimiter);

	/**
	 * @brief Set a file descriptor to non-blocking mode.
	 *
	 * @param fd File descriptor to set
	 * @return 0 on success, -1 on error
	 */
	int setNonBlocking(int fd);

public:
	/**
	 * @brief Construct a new Bot instance with connection parameters.
	 *
	 * @param server Server hostname or IP address
	 * @param port Server port number
	 * @param password Server password for authentication
	 * @param nickname Bot's IRC nickname
	 * @param channel Default channel to join after registration
	 */
	Bot(const std::string &server, int port, const std::string &password, 
	    const std::string &nickname, const std::string &channel);
	
	/**
	 * @brief Destroy the Bot instance and disconnect from server.
	 */
	~Bot();
	
	/**
	 * @brief Main bot event loop that connects to server and processes messages.
	 *
	 * Handles connection establishment, authentication, message reception,
	 * command parsing, and graceful disconnection. Runs until the connection
	 * is closed or an error occurs.
	 */
	void run();
};

#endif
