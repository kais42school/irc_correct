/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   bot_main.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bhamoum <bhamoum@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/19 17:15:02 by bhamoum           #+#    #+#             */
/*   Updated: 2026/01/19 17:39:45 by bhamoum          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file bot_main.cpp
 * @brief Entry point for the IRC bot application.
 *
 * This file contains the main function that initializes and starts the IRC bot client.
 * It validates command-line arguments, creates a Bot instance with the specified
 * server, port, password, nickname, and channel, and runs the bot's main event loop.
 */

#include "Bot.hpp"
#include <cstdlib>

/**
 * @brief Entry point for the IRC bot.
 *
 * Validates command-line arguments and starts the IRC bot that connects to the
 * specified server and joins the given channel. The bot will respond to user
 * commands and run indefinitely until the connection is closed.
 *
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return 0 on success, 1 on error
 *
 * Usage: ./ircbot <server> <port> <password> [nickname] [channel]
 * Example: ./ircbot 127.0.0.1 6667 password IRCBot #general
 */
int main(int argc, char **argv)
{
	if (argc < 4)
	{
		std::cerr << "Usage: " << argv[0] << " <server> <port> <password> [nickname] [channel]" << std::endl;
		std::cerr << "Example: " << argv[0] << " 127.0.0.1 6667 password IRCBot #general" << std::endl;
		return 1;
	}

	std::string server = argv[1];
	int port = std::atoi(argv[2]);
	std::string password = argv[3];
	std::string nickname = (argc > 4) ? argv[4] : "IRCBot";
	std::string channel = (argc > 5) ? argv[5] : "#general";

	if (port <= 0 || port > 65535)
	{
		std::cerr << "Invalid port number" << std::endl;
		return 1;
	}

	Bot bot(server, port, password, nickname, channel);
	bot.run();

	return 0;
}
