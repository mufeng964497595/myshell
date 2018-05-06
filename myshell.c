#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <errno.h>
#include <pwd.h>

#define BUF_SZ 256
#define TRUE 1
#define FALSE 0

const char* COMMAND_EXIT = "exit";
const char* COMMAND_HELP = "help";
const char* COMMAND_CD = "cd";
const char* COMMAND_IN = "<";
const char* COMMAND_OUT = ">";
const char* COMMAND_PIPE = "|";

// 内置的状态码
enum {
	RESULT_NORMAL,
	ERROR_FORK,
	ERROR_COMMAND,
	ERROR_WRONG_PARAMETER,
	ERROR_MISS_PARAMETER,
	ERROR_TOO_MANY_PARAMETER,
	ERROR_CD,
	ERROR_SYSTEM,
	ERROR_EXIT,

	/* 重定向的错误信息 */
	ERROR_MANY_IN,
	ERROR_MANY_OUT,
	ERROR_FILE_NOT_EXIST,
	
	/* 管道的错误信息 */
	ERROR_PIPE,
	ERROR_PIPE_MISS_PARAMETER
};

char username[BUF_SZ];
char hostname[BUF_SZ];
char curPath[BUF_SZ];
char commands[BUF_SZ][BUF_SZ];

int isCommandExist(const char* command);
void getUsername();
void getHostname();
int getCurWorkDir();
int splitCommands(char command[BUF_SZ]);
int callExit();
int callCommand(int commandNum);
int callCommandWithPipe(int left, int right);
int callCommandWithRedi(int left, int right);
int callCd(int commandNum);

int main() {
	/* 获取当前工作目录、用户名、主机名 */
	int result = getCurWorkDir();
	if (ERROR_SYSTEM == result) {
		fprintf(stderr, "\e[31;1mError: System error while getting current work directory.\n\e[0m");
		exit(ERROR_SYSTEM);
	}
	getUsername();
	getHostname();

	/* 启动myshell */
	char argv[BUF_SZ];
	while (TRUE) {
		printf("\e[32;1m%s@%s:%s\e[0m$ ", username, hostname,curPath); // 显示为绿色
		/* 获取用户输入的命令 */
		fgets(argv, BUF_SZ, stdin);
		int len = strlen(argv);
		if (len != BUF_SZ) {
			argv[len-1] = '\0';
		}

		int commandNum = splitCommands(argv);
		
		if (commandNum != 0) { // 用户有输入指令
			if (strcmp(commands[0], COMMAND_EXIT) == 0) { // exit命令
				result = callExit();
				if (ERROR_EXIT == result) {
					exit(-1);
				}
			} else if (strcmp(commands[0], COMMAND_CD) == 0) { // cd命令
				result = callCd(commandNum);
				switch (result) {
					case ERROR_MISS_PARAMETER:
						fprintf(stderr, "\e[31;1mError: Miss parameter while using command \"%s\".\n\e[0m"
							, COMMAND_CD);
						break;
					case ERROR_WRONG_PARAMETER:
						fprintf(stderr, "\e[31;1mError: No such path \"%s\".\n\e[0m", commands[1]);
						break;
					case ERROR_TOO_MANY_PARAMETER:
						fprintf(stderr, "\e[31;1mError: Too many parameters while using command \"%s\".\n\e[0m"
							, COMMAND_CD);
						break;
					case RESULT_NORMAL: // cd命令正常执行，更新当前工作l目录
						result = getCurWorkDir();
						if (ERROR_SYSTEM == result) {
							fprintf(stderr
								, "\e[31;1mError: System error while getting current work directory.\n\e[0m");
							exit(ERROR_SYSTEM);
						} else {
							break;
						}
				}
			} else { // 其它命令
				result = callCommand(commandNum);
				switch (result) {
					case ERROR_FORK:
						fprintf(stderr, "\e[31;1mError: Fork error.\n\e[0m");
						exit(ERROR_FORK);
					case ERROR_COMMAND:
						fprintf(stderr, "\e[31;1mError: Command not exist in myshell.\n\e[0m");
						break;
					case ERROR_MANY_IN:
						fprintf(stderr, "\e[31;1mError: Too many redirection symbol \"%s\".\n\e[0m", COMMAND_IN);
						break;
					case ERROR_MANY_OUT:
						fprintf(stderr, "\e[31;1mError: Too many redirection symbol \"%s\".\n\e[0m", COMMAND_OUT);
						break;
					case ERROR_FILE_NOT_EXIST:
						fprintf(stderr, "\e[31;1mError: Input redirection file not exist.\n\e[0m");
						break;
					case ERROR_MISS_PARAMETER:
						fprintf(stderr, "\e[31;1mError: Miss redirect file parameters.\n\e[0m");
						break;
					case ERROR_PIPE:
						fprintf(stderr, "\e[31;1mError: Open pipe error.\n\e[0m");
						break;
					case ERROR_PIPE_MISS_PARAMETER:
						fprintf(stderr, "\e[31;1mError: Miss pipe parameters.\n\e[0m");
						break;
				}
			}
		}
	}
}

int isCommandExist(const char* command) { // 判断指令是否存在
	if (command == NULL || strlen(command) == 0) return FALSE;

	int result = TRUE;
	
	int fds[2];
	if (pipe(fds) == -1) {
		result = FALSE;
	} else {
		/* 暂存输入输出重定向标志 */
		int inFd = dup(STDIN_FILENO);
		int outFd = dup(STDOUT_FILENO);

		pid_t pid = vfork();
		if (pid == -1) {
			result = FALSE;
		} else if (pid == 0) {
			/* 将结果输出重定向到文件标识符 */
			close(fds[0]);
			dup2(fds[1], STDOUT_FILENO);
			close(fds[1]);

			char tmp[BUF_SZ];
			sprintf(tmp, "command -v %s", command);
			system(tmp);
			exit(1);
		} else {
			waitpid(pid, NULL, 0);
			/* 输入重定向 */
			close(fds[1]);
			dup2(fds[0], STDIN_FILENO);
			close(fds[0]);

			if (getchar() == EOF) { // 没有数据，意味着命令不存在
				result = FALSE;
			}
			
			/* 恢复输入、输出重定向 */
			dup2(inFd, STDIN_FILENO);
			dup2(outFd, STDOUT_FILENO);
		}
	}

	return result;
}

void getUsername() { // 获取当前登录的用户名
	struct passwd* pwd = getpwuid(getuid());
	strcpy(username, pwd->pw_name);
}

void getHostname() { // 获取主机名
	gethostname(hostname, BUF_SZ);
}

int getCurWorkDir() { // 获取当前的工作目录
	char* result = getcwd(curPath, BUF_SZ);
	if (result == NULL)
		return ERROR_SYSTEM;
	else return RESULT_NORMAL;
}

int splitCommands(char command[BUF_SZ]) { // 以空格分割命令， 返回分割得到的字符串个数
	int num = 0;
	int i, j;
	int len = strlen(command);

	for (i=0, j=0; i<len; ++i) {
		if (command[i] != ' ') {
			commands[num][j++] = command[i];
		} else {
			if (j != 0) {
				commands[num][j] = '\0';
				++num;
				j = 0;
			}
		}
	}
	if (j != 0) {
		commands[num][j] = '\0';
		++num;
	}

	return num;
}

int callExit() { // 发送terminal信号退出进程
	pid_t pid = getpid();
	if (kill(pid, SIGTERM) == -1) 
		return ERROR_EXIT;
	else return RESULT_NORMAL;
}

int callCommand(int commandNum) { // 给用户使用的函数，用以执行用户输入的命令
	pid_t pid = fork();
	if (pid == -1) {
		return ERROR_FORK;
	} else if (pid == 0) {
		/* 获取标准输入、输出的文件标识符 */
		int inFds = dup(STDIN_FILENO);
		int outFds = dup(STDOUT_FILENO);

		int result = callCommandWithPipe(0, commandNum);
		
		/* 还原标准输入、输出重定向 */
		dup2(inFds, STDIN_FILENO);
		dup2(outFds, STDOUT_FILENO);
		exit(result);
	} else {
		int status;
		waitpid(pid, &status, 0);
		return WEXITSTATUS(status);
	}
}

int callCommandWithPipe(int left, int right) { // 所要执行的指令区间[left, right)，可能含有管道
	if (left >= right) return RESULT_NORMAL;
	/* 判断是否有管道命令 */
	int pipeIdx = -1;
	for (int i=left; i<right; ++i) {
		if (strcmp(commands[i], COMMAND_PIPE) == 0) {
			pipeIdx = i;
			break;
		}
	}
	if (pipeIdx == -1) { // 不含有管道命令
		return callCommandWithRedi(left, right);
	} else if (pipeIdx+1 == right) { // 管道命令'|'后续没有指令，参数缺失
		return ERROR_PIPE_MISS_PARAMETER;
	}

	/* 执行命令 */
	int fds[2];
	if (pipe(fds) == -1) {
		return ERROR_PIPE;
	}
	int result = RESULT_NORMAL;
	pid_t pid = vfork();
	if (pid == -1) {
		result = ERROR_FORK;
	} else if (pid == 0) { // 子进程执行单个命令
		close(fds[0]);
		dup2(fds[1], STDOUT_FILENO); // 将标准输出重定向到fds[1]
		close(fds[1]);
		
		result = callCommandWithRedi(left, pipeIdx);
		exit(result);
	} else { // 父进程递归执行后续命令
		int status;
		waitpid(pid, &status, 0);
		int exitCode = WEXITSTATUS(status);
		
		if (exitCode != RESULT_NORMAL) { // 子进程的指令没有正常退出，打印错误信息
			char info[4096] = {0};
			char line[BUF_SZ];
			close(fds[1]);
			dup2(fds[0], STDIN_FILENO); // 将标准输入重定向到fds[0]
			close(fds[0]);
			while(fgets(line, BUF_SZ, stdin) != NULL) { // 读取子进程的错误信息
				strcat(info, line);
			}
			printf("%s", info); // 打印错误信息
			
			result = exitCode;
		} else if (pipeIdx+1 < right){
			close(fds[1]);
			dup2(fds[0], STDIN_FILENO); // 将标准输入重定向到fds[0]
			close(fds[0]);
			result = callCommandWithPipe(pipeIdx+1, right); // 递归执行后续指令
		}
	}

	return result;
}

int callCommandWithRedi(int left, int right) { // 所要执行的指令区间[left, right)，不含管道，可能含有重定向
	if (!isCommandExist(commands[left])) { // 指令不存在
		return ERROR_COMMAND;
	}	

	/* 判断是否有重定向 */
	int inNum = 0, outNum = 0;
	char *inFile = NULL, *outFile = NULL;
	int endIdx = right; // 指令在重定向前的终止下标

	for (int i=left; i<right; ++i) {
		if (strcmp(commands[i], COMMAND_IN) == 0) { // 输入重定向
			++inNum;
			if (i+1 < right)
				inFile = commands[i+1];
			else return ERROR_MISS_PARAMETER; // 重定向符号后缺少文件名

			if (endIdx == right) endIdx = i;
		} else if (strcmp(commands[i], COMMAND_OUT) == 0) { // 输出重定向
			++outNum;
			if (i+1 < right)
				outFile = commands[i+1];
			else return ERROR_MISS_PARAMETER; // 重定向符号后缺少文件名
				
			if (endIdx == right) endIdx = i;
		}
	}
	/* 处理重定向 */
	if (inNum == 1) {
		FILE* fp = fopen(inFile, "r");
		if (fp == NULL) // 输入重定向文件不存在
			return ERROR_FILE_NOT_EXIST;
		
		fclose(fp);
	}
	
	if (inNum > 1) { // 输入重定向符超过一个
		return ERROR_MANY_IN;
	} else if (outNum > 1) { // 输出重定向符超过一个
		return ERROR_MANY_OUT;
	}

	int result = RESULT_NORMAL;
	pid_t pid = vfork();
	if (pid == -1) {
		result = ERROR_FORK;
	} else if (pid == 0) {
		/* 输入输出重定向 */
		if (inNum == 1)
			freopen(inFile, "r", stdin);
		if (outNum == 1)
			freopen(outFile, "w", stdout);

		/* 执行命令 */
		char* comm[BUF_SZ];
		for (int i=left; i<endIdx; ++i)
			comm[i] = commands[i];
		comm[endIdx] = NULL;
		execvp(comm[left], comm+left);
		exit(errno); // 执行出错，返回errno
	} else {
		int status;
		waitpid(pid, &status, 0);
		int err = WEXITSTATUS(status); // 读取子进程的返回码

		if (err) { // 返回码不为0，意味着子进程执行出错，用红色字体打印出错信息
			printf("\e[31;1mError: %s\n\e[0m", strerror(err));
		}
	}


	return result;
}

int callCd(int commandNum) { // 执行cd命令
	int result = RESULT_NORMAL;

	if (commandNum < 2) {
		result = ERROR_MISS_PARAMETER;
	} else if (commandNum > 2) {
		result = ERROR_TOO_MANY_PARAMETER;
	} else {
		int ret = chdir(commands[1]);
		if (ret) result = ERROR_WRONG_PARAMETER;
	}

	return result;
}
