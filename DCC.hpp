/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   DCC.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bhamoum <bhamoum@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/19 17:50:02 by bhamoum           #+#    #+#             */
/*   Updated: 2026/01/19 18:45:41 by bhamoum          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef DCC_HPP
#define DCC_HPP

#include <string>
#include <map>
#include <ctime>

/**
 * @enum DCCState
 * @brief Enumerates the lifecycle states of a DCC file transfer.
 *
 * - DCC_LISTENING: Sender is listening for the receiver to connect.
 * - DCC_CONNECTED: A data connection is established and transfer is in progress.
 * - DCC_COMPLETED: Transfer finished successfully.
 * - DCC_FAILED: Transfer encountered an error and was aborted.
 * - DCC_WAITING_ACCEPT: Transfer has been requested and is awaiting acceptance.
 */
enum DCCState
{
	DCC_LISTENING,
	DCC_CONNECTED,
	DCC_COMPLETED,
	DCC_FAILED,
	DCC_WAITING_ACCEPT
};

/**
 * @struct DCCTransfer
 * @brief Holds metadata and runtime state for a single DCC file transfer.
 *
 * Contains identification fields (id, sender/receiver nicknames), file
 * information (filename, filepath, filesize), socket descriptors for
 * listening and data connections, progress tracking, timestamps for
 * timeouts, and a FILE handle used when reading/writing data.
 */
struct DCCTransfer
{
	int id;                    /**< Unique transfer identifier */
	std::string sender_nick;   /**< Nickname of the sender */
	std::string receiver_nick; /**< Nickname of the receiver */
	std::string filename;      /**< File base name */
	std::string filepath;      /**< Absolute or relative path on sender side */
	size_t filesize;           /**< Total size in bytes */
	size_t bytes_sent;         /**< Bytes already transferred */
	size_t resumed_from;       /**< Resume offset if resuming a transfer */

	int listening_socket;      /**< Listening socket FD used by sender */
	int data_socket;           /**< Active data socket FD for the transfer */
	int listening_port;        /**< Port bound on listening_socket */

	DCCState state;            /**< Current transfer state */
	time_t created_time;       /**< Creation timestamp */
	time_t last_activity;      /**< Last activity timestamp for timeout detection */

	FILE *file_handle;         /**< FILE* used for reading (sender) or writing (receiver) */
	bool is_sender;            /**< True when this struct represents sender-side state */

	DCCTransfer() : id(0), filesize(0), bytes_sent(0), resumed_from(0),
					listening_socket(-1), data_socket(-1), listening_port(0),
					state(DCC_WAITING_ACCEPT), created_time(time(NULL)),
					last_activity(time(NULL)), file_handle(NULL), is_sender(false) {}
};

/**
 * @class DCCManager
 * @brief Manages multiple concurrent DCC file transfers.
 *
 * Responsible for creating transfer records, allocating ephemeral ports,
 * monitoring for timeouts, and cleaning up resources when transfers
 * complete or fail.
 */
class DCCManager
{
private:
	std::map<int, DCCTransfer> _transfers; /**< Map of transfer ID to transfer state */
	int _next_id;                           /**< Incremental ID generator for transfers */
	static const int DCC_TIMEOUT = 300;    /**< Timeout in seconds before aborting a stalled transfer */
	static const int DCC_MIN_PORT = 1024;  /**< Minimum ephemeral port to try */
	static const int DCC_MAX_PORT = 65535; /**< Maximum ephemeral port to try */

public:
	DCCManager();
	~DCCManager();

	/**
	 * @brief Create a new DCC transfer record and return its ID.
	 *
	 * The transfer is created in a WAITING_ACCEPT state. The caller is
	 * expected to create a listening socket and notify the receiver.
	 *
	 * @param sender Sender nickname
	 * @param receiver Receiver nickname
	 * @param filename File name to transfer
	 * @param filesize File size in bytes
	 * @return Newly allocated transfer ID, or -1 on failure
	 */
	int createTransfer(const std::string &sender, const std::string &receiver,
					   const std::string &filename, size_t filesize);

	/**
	 * @brief Retrieve a pointer to a transfer record by ID.
	 *
	 * @param id Transfer ID
	 * @return Pointer to DCCTransfer or NULL if not found
	 */
	DCCTransfer *getTransfer(int id);

	/**
	 * @brief Remove and cleanup a transfer by ID.
	 *
	 * Closes sockets, file handles, and erases the transfer record.
	 *
	 * @param id Transfer ID
	 */
	void removeTransfer(int id);

	/**
	 * @brief Access all transfers map for external iteration.
	 *
	 * @return Reference to underlying transfer map
	 */
	std::map<int, DCCTransfer> &getTransfers() { return _transfers; }

	/**
	 * @brief Find an available ephemeral port in the configured range.
	 *
	 * @return Available port number or -1 if none available
	 */
	int getAvailablePort();

	/**
	 * @brief Check whether a given port is available for binding.
	 *
	 * @param port Port number to check
	 * @return true if available, false otherwise
	 */
	bool isPortAvailable(int port);

	/**
	 * @brief Scan transfers and abort those that exceeded the timeout.
	 *
	 * Transfers that have been idle longer than DCC_TIMEOUT will be cleaned up.
	 */
	void checkTimeouts();

	/**
	 * @brief Force cleanup of resources associated with a transfer.
	 *
	 * @param id Transfer ID
	 */
	void cleanupTransfer(int id);
};

#endif
