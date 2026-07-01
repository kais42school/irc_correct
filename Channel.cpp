/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bhamoum <bhamoum@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/18 22:26:37 by bhamoum           #+#    #+#             */
/*   Updated: 2026/01/19 18:50:07 by bhamoum          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"

Channel::Channel(const std::string &name)
	: _name(name), _topic_time(0), _member_limit(0), 
	  _invite_only(false), _topic_restricted(false)
{
}

Channel::~Channel()
{
}

const std::string &Channel::getName() const
{
	return _name;
}

const std::set<int> &Channel::getMembers() const
{
	return _members;
}

const std::set<int> &Channel::getOperators() const
{
	return _operators;
}

const std::string &Channel::getTopic() const
{
	return _topic;
}

time_t Channel::getTopicTime() const
{
	return _topic_time;
}

const std::string &Channel::getTopicSetter() const
{
	return _topic_setter;
}

int Channel::getMemberLimit() const
{
	return _member_limit;
}

bool Channel::isInviteOnly() const
{
	return _invite_only;
}

bool Channel::isTopicRestricted() const
{
	return _topic_restricted;
}

size_t Channel::getMemberCount() const
{
	return _members.size();
}

void Channel::addMember(int sock, const std::string &nickname)
{
	_members.insert(sock);
	_nicknames[sock] = nickname;
}

void Channel::removeMember(int sock)
{
	_members.erase(sock);
	_operators.erase(sock);
	_banned.erase(sock);
	_nicknames.erase(sock);
}

bool Channel::hasMember(int sock) const
{
	return _members.count(sock) > 0;
}

void Channel::addOperator(int sock)
{
	if (hasMember(sock))
		_operators.insert(sock);
}

void Channel::removeOperator(int sock)
{
	_operators.erase(sock);
}

bool Channel::isOperator(int sock) const
{
	return _operators.count(sock) > 0;
}

void Channel::setTopic(const std::string &topic, const std::string &setter)
{
	_topic = topic;
	_topic_setter = setter;
	_topic_time = time(NULL);
}

void Channel::clearTopic()
{
	_topic.clear();
	_topic_setter.clear();
	_topic_time = 0;
}

void Channel::setInviteOnly(bool invite_only)
{
	_invite_only = invite_only;
}

void Channel::setTopicRestricted(bool restricted)
{
	_topic_restricted = restricted;
}

void Channel::setMemberLimit(int limit)
{
	_member_limit = (limit > 0) ? limit : 0;
}

void Channel::setKey(const std::string &key)
{
	_key = key;
}

void Channel::clearKey()
{
	_key.clear();
}

const std::string &Channel::getKey() const
{
	return _key;
}

bool Channel::hasKey() const
{
	return !_key.empty();
}

void Channel::addBan(int sock)
{
	_banned.insert(sock);
}

void Channel::removeBan(int sock)
{
	_banned.erase(sock);
}

bool Channel::isBanned(int sock) const
{
	return _banned.count(sock) > 0;
}

std::string Channel::getNickname(int sock) const
{
	std::map<int, std::string>::const_iterator it = _nicknames.find(sock);
	if (it != _nicknames.end())
		return it->second;
	return "";
}

void Channel::updateNickname(int sock, const std::string &new_nick)
{
	if (hasMember(sock))
		_nicknames[sock] = new_nick;
}

std::string Channel::getMemberListWithModes() const
{
	std::string result;
	for (std::set<int>::const_iterator it = _members.begin(); it != _members.end(); ++it)
	{
		int sock = *it;
		if (!result.empty())
			result += " ";
		
		if (isOperator(sock))
			result += "@";
		
		result += getNickname(sock);
	}
	return result;
}