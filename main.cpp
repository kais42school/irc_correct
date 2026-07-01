/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bhamoum <bhamoum@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/18 18:05:25 by bhamoum           #+#    #+#             */
/*   Updated: 2026/01/18 21:41:31 by bhamoum          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file main.cpp
 * @brief Entry point for the IRC server application.
 *
 * This file contains the main function that initializes and starts the IRC server.
 * It validates command-line arguments, creates a Server instance with the specified
 * port and password, and runs the server's main event loop.
 */

#include "Server.hpp"
#include <cstdlib>

/**
 * @brief Entry point for the IRC server.
 *
 * Validates command-line arguments and starts the IRC server on the specified
 * port with the given password. The server will run indefinitely until manually
 * stopped or a fatal error occurs.
 *
 * @param ac Number of command-line arguments
 * @param av Array of command-line argument strings
 * @return 0 on success, 1 on error
 *
 * Usage: ./ircserv <port> <password>
 */
int main(int ac, char **av)
{
	if (ac != 3)
		return (std::cerr << "Usage ./ircserv <port> <password>\n"
						  << std::endl,
				1);
	try
	{
		Server server(atoi(av[1]), av[2]);
		server.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
	}
	return 0;
}
