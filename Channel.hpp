/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: bhamoum <bhamoum@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/01/18 22:26:38 by bhamoum           #+#    #+#             */
/*   Updated: 2026/01/19 18:50:07 by bhamoum          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <iostream>
#include <string>
#include <set>
#include <map>
#include <ctime>

/**
 * @class Channel
 * @brief Represents an IRC channel with members, operators, modes, and topic.
 * 
 * Manages all channel state including:
 * - Member list with socket file descriptors
 * - Operator privileges
 * - Channel topic and modification timestamp
 * - Channel modes: +i (invite-only), +t (topic-restricted), +k (key/password), +o (operator), +l (member limit)
 * - Ban list
 * - Member nickname tracking for quick lookups
 * 
 * Provides methods to manage membership, operators, modes, and topic information.
 */
class Channel
{
private:
	std::string _name;
	std::set<int> _members;           /**< Socket file descriptors of all members */
	std::set<int> _operators;         /**< Socket file descriptors of channel operators (@) */
	std::string _topic;               /**< Current channel topic */
	time_t _topic_time;               /**< Timestamp when topic was last set */
	std::string _topic_setter;        /**< Nickname that set the topic */
	int _member_limit;                /**< Maximum members allowed (0 = no limit) */
	bool _invite_only;                /**< +i mode: invite-only channel */
	bool _topic_restricted;           /**< +t mode: only operators can set topic */
	std::string _key;                 /**< +k mode: channel key/password */
	std::set<int> _banned;            /**< Set of banned socket FDs */
	std::map<int, std::string> _nicknames; /**< Map socket FD to nickname for quick lookup */

public:
	/**
	 * @brief Construct a Channel with the given name.
	 *
	 * @param name Channel name (including leading '#')
	 */
	Channel(const std::string &name);

	/**
	 * @brief Destroy the Channel and release resources.
	 */
	~Channel();

	/**
	 * @name Accessors
	 * Methods that provide read-only access to channel properties.
	 */
	//@{
	const std::string &getName() const;
	const std::set<int> &getMembers() const;
	const std::set<int> &getOperators() const;
	const std::string &getTopic() const;
	time_t getTopicTime() const;
	const std::string &getTopicSetter() const;
	int getMemberLimit() const;
	bool isInviteOnly() const;
	bool isTopicRestricted() const;
	size_t getMemberCount() const;
	//@}

	/**
	 * @name Membership management
	 * Methods to add/remove members and manage operator status.
	 */
	//@{
	/** Add a member to the channel and register their nickname. */
	void addMember(int sock, const std::string &nickname);

	/** Remove a member from the channel. */
	void removeMember(int sock);

	/** Check whether a socket FD is a member of the channel. */
	bool hasMember(int sock) const;

	/** Promote a member to channel operator. */
	void addOperator(int sock);

	/** Demote a member from operator status. */
	void removeOperator(int sock);

	/** Check whether a socket FD is an operator. */
	bool isOperator(int sock) const;
	//@}

	/**
	 * @name Topic management
	 */
	//@{
	/** Set the channel topic and record the setter. */
	void setTopic(const std::string &topic, const std::string &setter);

	/** Clear the channel topic. */
	void clearTopic();
	//@}

	/**
	 * @name Mode management
	 */
	//@{
	void setInviteOnly(bool invite_only);
	void setTopicRestricted(bool restricted);
	void setMemberLimit(int limit);
	void setKey(const std::string &key);
	void clearKey();
	const std::string &getKey() const;
	bool hasKey() const;
	//@}

	/**
	 * @name Ban management
	 */
	//@{
	void addBan(int sock);
	void removeBan(int sock);
	bool isBanned(int sock) const;
	//@}

	/**
	 * @name Utility
	 */
	//@{
	std::string getNickname(int sock) const;
	void updateNickname(int sock, const std::string &new_nick);
	std::string getMemberListWithModes() const;
	//@}
};

#endif