NAME = ircserv
BOT_NAME = ircbot

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -fsanitize=address -g

SRCS = main.cpp \
	   Server.cpp \
	   Channel.cpp \
	   DCC.cpp

BOT_SRCS = bot_main.cpp \
		   Bot.cpp

OBJS = $(SRCS:.cpp=.o)
BOT_OBJS = $(BOT_SRCS:.cpp=.o)

all: $(NAME) $(BOT_NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(BOT_NAME): $(BOT_OBJS)
	$(CXX) $(CXXFLAGS) $(BOT_OBJS) -o $(BOT_NAME)

clean:
	$(RM) $(OBJS) $(BOT_OBJS)

fclean: clean
	$(RM) $(NAME) $(BOT_NAME)


re: fclean all

.PHONY: all clean fclean re