#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/types.h>

#define BUF_SZ 256

const char* COMMAND_EXIT = "exit";
const char* COMMAND_HELP = "help";
const char* COMMAND_CD = "cd";

enum {
	RESULT_NORMAL,
	ERROR_FORK,
	ERROR_COMMAND,
	ERROR_WRONG_PARAMETER,
	ERROR_MISS_PARAMETER,
	ERROR_TOO_MANY_PARAMETER,
	ERROR_CD,
	ERROR_SYSTEM,
	ERROR_EXIT
};

char curPath[BUF_SZ];
char commands[BUF_SZ][BUF_SZ];

int getCurWorkDir();
int splitCommands(char command[BUF_SZ]);
int callExit();
int callHelp();
int callCommand(int commandNum);
int callCd(int commandNum);

int main() {
	char argv[BUF_SZ];

	int result = getCurWorkDir();
	if (ERROR_SYSTEM == result) {
		fprintf(stderr, "System error while getting current work directory.\n");
		exit(ERROR_SYSTEM);
	}

	while (1) {
		printf("%s$ ", curPath);
		fgets(argv, BUF_SZ, stdin);
		int len = strlen(argv);
		if (len != BUF_SZ) {
			argv[len-1] = '\0';
		}

		int commandNum = splitCommands(argv);
		
		if (commandNum != 0) {
			if (strcmp(commands[0], COMMAND_EXIT) == 0) {
				result = callExit();
				if (ERROR_EXIT == result) {
					exit(-1);
				}
			} else if (strcmp(commands[0], COMMAND_HELP) == 0) {
				result = callHelp();
				if (ERROR_FORK == result) {
					fprintf(stderr, "Fork error in line %d.\n", __LINE__);
					exit(ERROR_FORK);
				}
			} else if (strcmp(commands[0], COMMAND_CD) == 0) {
				result = callCd(commandNum);
				switch (result) {
					case ERROR_MISS_PARAMETER:
						fprintf(stderr, "Miss parameter while using command \"%s\".\n",COMMAND_CD);
						break;
					case ERROR_WRONG_PARAMETER:
						fprintf(stderr, "No such path \"%s\".\n", commands[1]);
						break;
					case ERROR_TOO_MANY_PARAMETER:
						fprintf(stderr, "Too many parameters while using command \"%s\".\n", COMMAND_CD);
						break;
					case RESULT_NORMAL:
						result = getCurWorkDir();
						if (ERROR_SYSTEM == result) {
							fprintf(stderr, "System error while getting current work directory.\n");
							exit(ERROR_SYSTEM);
						} else {
							break;
						}
				}
			} else {
				result = callCommand(commandNum);
				switch (result) {
					case ERROR_FORK:
						fprintf(stderr, "Fork error in line %d.\n", __LINE__);
						exit(ERROR_FORK);
					case ERROR_COMMAND:
						fprintf(stderr, "No such command in myshell.\n");
						break;
				}
			}
		}
	}
}

int getCurWorkDir() {
	char* result = getcwd(curPath, BUF_SZ);
	if (result == NULL)
		return ERROR_SYSTEM;
	else return RESULT_NORMAL;
}

int splitCommands(char command[BUF_SZ]) {
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

int callExit() {
	pid_t pid = getpid();
	if (kill(pid, SIGTERM) == -1) 
		return ERROR_EXIT;
	else return RESULT_NORMAL;
}

int callHelp() {
	return RESULT_NORMAL;
}

int callCommand(int commandNum) {
	return RESULT_NORMAL;
}

int callCd(int commandNum) {
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
