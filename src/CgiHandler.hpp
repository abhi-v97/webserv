#ifndef CGIHANDLER_HPP
# define CGIHANDLER_HPP

# include <iostream>
# include <map>
# include <sched.h>
# include <string>

class CgiHandler
{
public:
	CgiHandler();
	CgiHandler(const CgiHandler &src);
	~CgiHandler();

	CgiHandler &operator=(const CgiHandler &rhs);

	void execute(std::string cgiName);
	int getOutFd();

private:
	std::map<std::string, std::string> m_header;
	pid_t m_PID;
	int m_fd[2];
};

std::ostream &operator<<(std::ostream &outf, const CgiHandler &obj);

#endif

/* ****************************************************** CGIHANDLER_H  */
