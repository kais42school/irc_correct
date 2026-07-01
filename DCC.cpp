/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   DCC.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bhamoum <bhamoum@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/19 17:50:02 by bhamoum           #+#    #+#             */
/*   Updated: 2026/01/19 18:50:07 by bhamoum          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "DCC.hpp"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <unistd.h>
#include <cstdio>

DCCManager::DCCManager() : _next_id(1) {}

DCCManager::~DCCManager()
{
	for (std::map<int, DCCTransfer>::iterator it = _transfers.begin();
		 it != _transfers.end(); ++it)
	{
		cleanupTransfer(it->first);
	}
}

int DCCManager::createTransfer(const std::string &sender, const std::string &receiver,
							   const std::string &filename, size_t filesize)
{
	DCCTransfer transfer;
	transfer.id = _next_id++;
	transfer.sender_nick = sender;
	transfer.receiver_nick = receiver;
	transfer.filename = filename;
	transfer.filesize = filesize;
	transfer.state = DCC_WAITING_ACCEPT;
	transfer.created_time = time(NULL);
	transfer.last_activity = time(NULL);

	_transfers[transfer.id] = transfer;
	return transfer.id;
}

DCCTransfer *DCCManager::getTransfer(int id)
{
	std::map<int, DCCTransfer>::iterator it = _transfers.find(id);
	if (it == _transfers.end())
		return NULL;
	return &(it->second);
}

void DCCManager::removeTransfer(int id)
{
	std::map<int, DCCTransfer>::iterator it = _transfers.find(id);
	if (it != _transfers.end())
	{
		cleanupTransfer(id);
		_transfers.erase(it);
	}
}

int DCCManager::getAvailablePort()
{
	for (int port = DCC_MIN_PORT; port <= DCC_MAX_PORT; ++port)
	{
		if (isPortAvailable(port))
			return port;
	}
	return -1;
}

bool DCCManager::isPortAvailable(int port)
{

	for (std::map<int, DCCTransfer>::iterator it = _transfers.begin();
		 it != _transfers.end(); ++it)
	{
		if (it->second.listening_port == port && it->second.state != DCC_COMPLETED && it->second.state != DCC_FAILED)
			return false;
	}
	return true;
}

void DCCManager::checkTimeouts()
{
	time_t now = time(NULL);
	std::map<int, DCCTransfer>::iterator it = _transfers.begin();

	while (it != _transfers.end())
	{
		if (it->second.state != DCC_COMPLETED && it->second.state != DCC_FAILED)
		{
			if (now - it->second.last_activity > DCC_TIMEOUT)
			{
				std::cout << "[DCC] Transfer " << it->first << " timed out" << std::endl;
				it->second.state = DCC_FAILED;
				cleanupTransfer(it->first);
			}
		}
		++it;
	}
}

void DCCManager::cleanupTransfer(int id)
{
	DCCTransfer *transfer = getTransfer(id);
	if (!transfer)
		return;

	if (transfer->listening_socket >= 0)
	{
		close(transfer->listening_socket);
		transfer->listening_socket = -1;
	}

	if (transfer->data_socket >= 0)
	{
		close(transfer->data_socket);
		transfer->data_socket = -1;
	}

	if (transfer->file_handle)
	{
		fclose(transfer->file_handle);
		transfer->file_handle = NULL;
	}
}
