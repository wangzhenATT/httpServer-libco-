#include <iostream>
using namespace std;
#include <co_routine_inner.h>
#include <dlfcn.h>
#include <stack>
#include <poll.h>
#include <co_routine.h>

#ifdef __cplusplus
extern "C"
{
#endif
	#include "httpd.h"
#ifdef __cplusplus
}
#endif

int co_accept(int fd, struct sockaddr *addr, socklen_t *len );
struct task_t
{
	int _fd;
	stCoRoutine_t *_co;
	task_t(): _fd(-1), _co(NULL){}
};

int g_listen_fd = -1;
int g_error_code = 1;
stack<task_t *> g_worker;

void usage(string s)
{
	cout << "usage : " << s << "[IP] [PORT] [PROC] [CO]" << endl;
}
//void *A(void *arg)
//{
//	cout << "co_test" << endl;
//}
void *worker_func(void *arg)
{
	co_enable_hook_sys();
	
	task_t *ts = (task_t*)arg;
	for(;;)
	{
		if(ts->_fd == -1)
		{
			g_worker.push(ts);
			co_yield_ct();
			continue;
		}
		int fd = ts->_fd;
		ts->_fd = -1;
		struct pollfd pf = { 0 };
		pf.fd = fd;
		pf.events = (POLLIN | POLLRDHUP | POLLHUP);
		co_poll(co_get_epoll_ct(), &pf, 1, -1);
		accept_handler(fd);
		close(fd);
	}
	return 0;
}
void *listen_func(void *arg)
{
	co_enable_hook_sys();
	//fflush(stdout);
	
	for(;;)
	{
		if(g_worker.empty())
		{
			struct pollfd pf = {0};
			pf.fd = -1;
			pf.events = (POLLIN | POLLRDHUP | POLLHUP);
			co_poll(co_get_epoll_ct(), &pf, 1, 1000);
			continue;
		}
		struct sockaddr_in romate;
		memset(&romate, 0, sizeof(romate));
		socklen_t len = sizeof(romate);
		int connfd = co_accept(g_listen_fd, (struct sockaddr*)&romate, &len);
		if(connfd < 0)
		{
			continue;
		}
		if(g_worker.empty())
		{
			close(connfd);
			continue;
		}
		task_t *ts = g_worker.top();
		g_worker.pop();
		ts->_fd = connfd;
		co_resume(ts->_co);
	}
	return 0;
}
int main(int argc, char *argv[])
{
	if(argc != 5)
	{
		usage(argv[0]);
		return g_error_code++;
	}
	char *ip = argv[1];
	int port = atoi(argv[2]);
	int proc = atoi(argv[3]);
	int co_cnt = atoi(argv[4]);
	g_listen_fd = startup(ip, port);
	for(int i = 0; i < proc; i++)
	{
		pid_t pid = fork();
		if(pid > 0)
		{
			continue;
		}
		if(pid < 0)
		{
			break;
		}
		for(int j = 0; j < co_cnt; j++)
		{
			task_t *ts = (task_t*)calloc(1, sizeof(task_t));
			ts->_fd = -1;
			co_create(&ts->_co, NULL, worker_func, ts);
			co_resume(ts->_co);
		}
		stCoRoutine_t *listen_co = NULL;
		co_create(&listen_co, NULL, listen_func, NULL);
		co_resume(listen_co);

		co_eventloop(co_get_epoll_ct(), 0, 0);
		exit(0);
	}
	return 0;
}
