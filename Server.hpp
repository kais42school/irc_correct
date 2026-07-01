/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: marvin <marvin@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/18 18:04:02 by bhamoum           #+#    #+#             */
/*   Updated: 2026/07/02 01:21:05 by marvin           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include "Channel.hpp"
#include "DCC.hpp"

#define PORT 6667
#define MAX_CLIENTS 100
#define BUFFER_SIZE 512

/**
 * @struct ClientInfo
 * @brief Stores per-client connection state and authentication information.
 * 
 * Maintains all necessary state for each connected IRC client including
 * socket descriptor, authentication status (password, nickname, username),
 * registration state, and a receive buffer for message assembly.
 */
struct ClientInfo
{
	int socket;              /**< Socket file descriptor for the client connection */
	bool passwordAccepted;   /**< Whether the client successfully authenticated with PASS */
	bool hasNick;            /**< Whether the client has sent a valid NICK command */
	bool hasUser;            /**< Whether the client has sent a valid USER command */
	bool isRegistered;       /**< Whether the client is fully registered (password, NICK, USER received) */
	std::string nickname;    /**< The client's current IRC nickname */
	std::string username;    /**< The client's IRC username/login */
	std::string buffer;      /**< Incomplete incoming message buffer (until CRLF received) */
};

/**
 * @class Server
 * @brief Multi-client IRC server using non-blocking sockets and poll-based multiplexing.
 * 
 * Implements an RFC 2812 compliant IRC server supporting:
 * - Password authentication
 * - Channel management (JOIN, PART, LIST, TOPIC, MODE, KICK, INVITE)
 * - User-to-user messaging (PRIVMSG)
 * - Channel operator privileges
 * - DCC file transfer between users
 * - Comprehensive IRC numeric replies
 * 
 * Architecture:
 * - Non-blocking TCP sockets for all connections
 * - poll() for I/O multiplexing (supports up to 100 clients)
 * - Per-client state management with ClientInfo structs
 * - Per-channel state with Channel objects
 * - DCC transfer management with DCCManager
 */
class Server
{
private:
	int _port;                                   /**< Server listening port */
	std::string _password;                       /**< Server password for authentication */
	int _server_fd;                              /**< Server socket file descriptor */
	std::vector<pollfd> _poll_fd;                /**< Array of poll file descriptors for I/O multiplexing */
	std::map<int, ClientInfo> _clients;          /**< Map of socket FD to client information */
	std::map<std::string, Channel> _channels;    /**< Map of channel name to Channel object */
	DCCManager _dcc_manager;                     /**< Manager for DCC file transfers */
	std::map<int, int> _dcc_listening_sockets;   /**< Map of DCC listening socket FD to transfer ID */

	/**
	 * @brief Initialize and bind the server listening socket.
	 * 
	 * Sets up the main server socket, configures it as non-blocking,
	 * binds to the specified port, and starts listening for connections.
	 * 
	 * @throws std::runtime_error if socket creation or binding fails
	 */
	void setupSocket();

	/**
	 * @brief Accept a new client connection and register it with poll().
	 * 
	 * Called when the server socket has an incoming connection. Creates
	 * a new socket for the client, sets it non-blocking, adds it to the
	 * poll array, and initializes ClientInfo structure.
	 */
	void acceptNewClient();

	/**
	 * @brief Process incoming data from a client and dispatch commands.
	 * 
	 * Reads available data into the client buffer, splits by CRLF line endings,
	 * and dispatches each complete line to the appropriate command handler.
	 * Handles client disconnection (POLLHUP) and removes the client if needed.
	 * 
	 * @param index The index of the client in the _poll_fd array
	 * @return true if the client/pollfd was removed during processing, false otherwise
	 */
	bool handleClientMessage(size_t index);

	/**
	 * @brief Check if a nickname is already in use by another client.
	 * 
	 * @param nick The nickname to check
	 * @return true if the nickname is already in use, false otherwise
	 */
	bool isNickInUse(const std::string &nick) const;

	/**
	 * @brief Send all bytes of a message, handling partial writes.
	 * 
	 * Ensures complete message delivery even if the socket write buffer
	 * is full. Uses non-blocking send with retries.
	 * 
	 * @param sock The socket to send on
	 * @param data The data to send
	 * @return true if all data was sent successfully, false on error
	 */
	bool sendAll(int sock, const std::string &data);

	/**
	 * @brief Set a socket to non-blocking mode using fcntl().
	 * 
	 * @param fd The file descriptor to set non-blocking
	 * @return 0 on success, -1 on error
	 */
	int setNonBlocking(int fd);

	/**
	 * @brief Handle PASS command (password authentication).
	 * 
	 * Verifies the server password. Client must send PASS before
	 * other commands. Returns error reply on incorrect password.
	 * 
	 * @param sock The client socket
	 * @param client The client information
	 * @param command The PASS command string
	 * @param index The client index in _poll_fd
	 * @return true if client was disconnected, false otherwise
	 */
	bool handlePASS(int sock, ClientInfo &client, const std::string &command, size_t index);

	/**
	 * @brief Handle NICK command (set nickname).
	 * 
	 * Allows client to set or change their nickname. Checks for
	 * duplicates and updates nickname in all channels. Sends
	 * appropriate error messages if nickname is invalid or in use.
	 * 
	 * @param sock The client socket
	 * @param client The client information
	 * @param command The NICK command string
	 * @return true if client was disconnected, false otherwise
	 */
	bool handleNICK(int sock, ClientInfo &client, const std::string &command);

	/**
	 * @brief Handle USER command (set username and real name).
	 * 
	 * Sets the username and marks the client as having sent the USER command.
	 * When both NICK and USER are received (and PASS accepted), the client
	 * becomes registered and receives the 001 welcome message.
	 * 
	 * @param sock The client socket
	 * @param client The client information
	 * @param command The USER command string
	 * @return true if client was disconnected, false otherwise
	 */
	bool handleUSER(int sock, ClientInfo &client, const std::string &command);

	/**
	 * @brief Handle JOIN command (join or create channel).
	 * 
	 * Allows client to join an existing channel or create a new one.
	 * The first user to join a channel becomes an operator. Sends
	 * appropriate join confirmations and member list to the joining client.
	 * 
	 * @param sock The client socket
	 * @param client The client information
	 * @param command The JOIN command string
	 * @return true if client was disconnected, false otherwise
	 */
	bool handleJOIN(int sock, ClientInfo &client, const std::string &command);

	/**
	 * @brief Handle PART command (leave channel).
	 * 
	 * Removes client from a channel and broadcasts a PART message to
	 * all remaining members. If the channel becomes empty, it is destroyed.
	 * 
	 * @param sock The client socket
	 * @param client The client information
	 * @param command The PART command string
	 * @return true if client was disconnected, false otherwise
	 */
	bool handlePART(int sock, ClientInfo &client, const std::string &command);

	/**
	 * @brief Handle PRIVMSG command (direct message or channel message).
	 * 
	 * Sends a message to either a specific user (direct message) or to all
	 * members of a channel. Routes the message appropriately and handles
	 * error cases (target not found, not in channel, etc.).
	 * 
	 * @param sock The client socket
	 * @param client The client information
	 * @param command The PRIVMSG command string
	 * @return true if client was disconnected, false otherwise
	 */
	bool handlePRIVMSG(int sock, ClientInfo &client, const std::string &command);
	bool handleNOTICE(int sock, ClientInfo &client, const std::string &command);

	/**
	 * @brief Handle QUIT command (disconnect client).
	 * 
	 * Cleanly disconnects the client from all channels and closes
	 * the socket. Broadcasts QUIT message to all channels and removes
	 * the client from all data structures.
	 * 
	 * @param sock The client socket
	 * @param client The client information
	 * @param command The QUIT command string
	 * @param index The client index in _poll_fd
	 * @return true if client was disconnected (always true), false otherwise
	 */
	bool handleQUIT(int sock, ClientInfo &client, const std::string &command, size_t index);

	/**
	 * @brief Handle TOPIC command (get/set channel topic).
	 * 
	 * Allows getting the current channel topic or setting a new one.
	 * If +t mode is set, only operators can change the topic.
	 * Returns topic information and confirmation of topic change.
	 * 
	 * @param sock The client socket
	 * @param client The client information
	 * @param command The TOPIC command string
	 * @return true if client was disconnected, false otherwise
	 */
	bool handleTOPIC(int sock, ClientInfo &client, const std::string &command);

	/**
	 * @brief Handle MODE command (manage channel modes and user privileges).
	 * 
	 * Manages channel modes including:
	 * - +i (invite-only): Requires invitation to join
	 * - +t (topic-restricted): Only operators can change topic
	 * - +k (key): Requires password to join
	 * - +o (operator): Promote/demote users to operators
	 * - +l (limit): Set maximum member limit
	 * 
	 * Only channel operators can change modes. Broadcasts mode changes
	 * to all members.
	 * 
	 * @param sock The client socket
	 * @param client The client information
	 * @param command The MODE command string
	 * @return true if client was disconnected, false otherwise
	 */
	bool handleMODE(int sock, ClientInfo &client, const std::string &command);

	/**
	 * @brief Handle KICK command (remove user from channel).
	 * 
	 * Allows channel operators to remove a user from a channel.
	 * Broadcasts kick message to all members and removes the target
	 * user from the channel.
	 * 
	 * @param sock The client socket
	 * @param client The client information
	 * @param command The KICK command string
	 * @return true if client was disconnected, false otherwise
	 */
	bool handleKICK(int sock, ClientInfo &client, const std::string &command);

	/**
	 * @brief Handle INVITE command (invite user to channel).
	 * 
	 * Allows channel operators (or any member if +i is not set) to
	 * invite another user to a channel. Sends invite notification
	 * to the target user.
	 * 
	 * @param sock The client socket
	 * @param client The client information
	 * @param command The INVITE command string
	 * @return true if client was disconnected, false otherwise
	 */
	bool handleINVITE(int sock, ClientInfo &client, const std::string &command);

	/**
	 * @brief Handle LIST command (list all channels).
	 * 
	 * Lists all channels on the server with their member counts and topics.
	 * Optionally filters to specific channels if provided.
	 * 
	 * @param sock The client socket
	 * @param client The client information
	 * @param command The LIST command string
	 * @return true if client was disconnected, false otherwise
	 */
	bool handleLIST(int sock, ClientInfo &client, const std::string &command);

	/**
	 * @brief Handle DCC command (initiate file transfer).
	 * 
	 * Parses DCC SEND command to initiate file transfer between users.
	 * Creates a listening socket, allocates a port, sends notification
	 * to receiver with sender IP and port.
	 * 
	 * @param sock The client socket
	 * @param client The client information
	 * @param command The DCC command string
	 * @return true if client was disconnected, false otherwise
	 */
	bool handleDCC(int sock, ClientInfo &client, const std::string &command);

	/**
	 * @brief Process incoming DCC listening socket connections.
	 * 
	 * Called when a DCC listening socket has an incoming connection
	 * from a receiver. Accepts the connection, opens the file for reading,
	 * and updates the transfer state to CONNECTED.
	 */
	void processDCCListening();

	/**
	 * @brief Process active DCC file transfer.
	 * 
	 * Sends file data in 4KB chunks to the connected receiver. Updates
	 * progress tracking and marks transfer as completed when finished.
	 * 
	 * @param transfer_id The ID of the transfer to process
	 */
	void processDCCTransfer(int transfer_id);

	/**
	 * @brief Extract the IP address of a client from its socket.
	 * 
	 * Uses getpeername() to determine the client's IP address in
	 * dotted-decimal notation.
	 * 
	 * @param sock The client socket
	 * @return The IP address as a string, or "0.0.0.0" on error
	 */
	std::string getClientIPAddress(int sock);

	/**
	 * @brief Create and configure a DCC listening socket.
	 * 
	 * Creates a non-blocking listening socket for DCC file transfer,
	 * allocates an available port, and adds the socket to the poll array
	 * for monitoring incoming receiver connections.
	 * 
	 * @param transfer_id The ID of the DCC transfer this socket is for
	 * @return true if socket was created successfully, false otherwise
	 */
	bool createDCCListeningSocket(int transfer_id);

public:
	/**
	 * @brief Constructor initializing server with port and password.
	 * 
	 * @param port The port number to listen on
	 * @param password The server password for client authentication
	 * @throws std::runtime_error if socket setup fails
	 */
	Server(int port, const std::string &password);

	/**
	 * @brief Destructor cleaning up server resources.
	 * 
	 * Closes all client sockets, DCC sockets, and the server listening socket.
	 */
	~Server();

	/**
	 * @brief Main server event loop.
	 * 
	 * Runs the server indefinitely, using poll() to monitor all sockets
	 * for incoming data and new connections. Processes client messages,
	 * DCC transfers, and handles timeouts. Continues until the server
	 * is manually stopped or a fatal error occurs.
	 */
	void run();
};

#endif
