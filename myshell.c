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

char username[BUF_SZ];
char hostname[BUF_SZ];
char outputFile[BUF_SZ];
char curPath[BUF_SZ];
char commands[BUF_SZ][BUF_SZ];

int isCommandExist(const char* command);
void getUsername();
void getHostname();
int getCurWorkDir();
int splitCommands(char command[BUF_SZ]);
int callExit();
int callHelp();
int callCommand(int commandNum);
int callCd(int commandNum);

int main() {
	char argv[BUF_SZ];

	getUsername();
	getHostname();

	int result = getCurWorkDir();
	if (ERROR_SYSTEM == result) {
		fprintf(stderr, "Error: System error while getting current work directory.\n");
		exit(ERROR_SYSTEM);
	}
	strcpy(outputFile, curPath);
	strcat(outputFile, "/command.res");

	while (1) {
		printf("%s@%s:%s$ ", username, hostname,curPath);
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
					fprintf(stderr, "Error: Fork error in line %d.\n", __LINE__);
					exit(ERROR_FORK);
				}
			} else if (strcmp(commands[0], COMMAND_CD) == 0) {
				result = callCd(commandNum);
				switch (result) {
					case ERROR_MISS_PARAMETER:
						fprintf(stderr, "Error: Miss parameter while using command \"%s\".\n",COMMAND_CD);
						break;
					case ERROR_WRONG_PARAMETER:
						fprintf(stderr, "Error: No such path \"%s\".\n", commands[1]);
						break;
					case ERROR_TOO_MANY_PARAMETER:
						fprintf(stderr, "Error: Too many parameters while using command \"%s\".\n", COMMAND_CD);
						break;
					case RESULT_NORMAL:
						result = getCurWorkDir();
						if (ERROR_SYSTEM == result) {
							fprintf(stderr, "Error: System error while getting current work directory.\n");
							exit(ERROR_SYSTEM);
						} else {
							break;
						}
				}
			} else {
				result = callCommand(commandNum);
				switch (result) {
					case ERROR_FORK:
						fprintf(stderr, "Error: Fork error in line %d.\n", __LINE__);
						exit(ERROR_FORK);
					case ERROR_COMMAND:
						fprintf(stderr, "Error: Command not exist in myshell.\n");
						break;
				}
			}
		}
	}
}

int isCommandExist(const char* command) {
	if (command == NULL || strlen(command) == 0) return FALSE;

	int result = TRUE;
	
	pid_t pid = vfork();
	if (pid == -1) {
		result = FALSE;
	} else if (pid == 0) {
		freopen(outputFile, "w", stdout);
		execlp("which", "which", command, NULL);
		exit(1);
	} else {
		waitpid(pid, NULL, 0);

		FILE* fp = fopen(outputFile, "r");
		if (fp == NULL || fgetc(fp) == EOF) {
			result = FALSE;
		}
		fclose(fp);
	}

	return result;
}

void getUsername() {
	struct passwd* pwd = getpwuid(getuid());
	strcpy(username, pwd->pw_name);
}

void getHostname() {
	gethostname(hostname, BUF_SZ);
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
	if (!isCommandExist(commands[0])) {
		return ERROR_COMMAND;
	}	

	int result = RESULT_NORMAL;
	pid_t pid = vfork();

	if (pid == -1) {
		result = ERROR_FORK;
	} else if (pid == 0) {
		char* comm[BUF_SZ];
		for (int i=0; i<commandNum; ++i)
			comm[i] = commands[i];
		comm[commandNum] = NULL;
		execvp(comm[0], comm);
		exit(errno);
	} else {
		int status;
		waitpid(pid, &status, 0);
		int err = WEXITSTATUS(status);

		if (err) {
			printf("Error: %s\n", strerror(err));
		}
	}

	return result;
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
