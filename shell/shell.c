#define CMD_BUFFER_SIZE		1024
#define CMD_ARG_LEN		512
#define CMD_ARG_NUM		128
#define CMD_PIPE_NUM		128
#define CMD_DELIMITER		" "
#define HISTORY_SIZE		101
#define MALLOC_ALLOWED_TIMES	3

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

char *buildin_cmds[] = {
	"cd",
	"history",
	"exit",
	NULL
};


/*
 * Cmd_s.type:
 * 0: buildin cmds
 * 1: executables
 */
struct Cmd_s {
	char **args;
	int    type;
};

struct Cmd_s *cmds;
int cmd_num;

/* history cmd related global variables starts with his_ */
char *his_stack[HISTORY_SIZE] = {0};
int his_oldest = -1;
int his_newest = -1;
int his_size;
char *his_cmd;
int his_reload;
int his_reload_size;

void store_cmd_line(char *cmd_line, int cmd_size);
void empty_history(void);
void remove_newest_record(void);

char *cmd_reader(int *cmd_size);
void cmd_parser(char *cmd_line);
int cmd_launcher(void);
int executable_launcher(void);

int execute_cd(char **args);
int execute_history(char **args);

int check_errno(int status);
void check_errno_and_exit(int status);
int check_buildin(char *words);
void *malloc_ck(size_t size);
void *realloc_ck(void *ptr, size_t size);
void store_cmd_para(char **args, char *arg_w, int *args_i, int *word_i);
struct Cmd_s create_cmd(char **args);
void store_cmd(struct Cmd_s cmd, int *a_i);
int is_decimal(char *s);

void free_args(char **args);
void free_cmds(void);
void free_global(void);

int main(int argc, char **argv)
{
	char *cmd_line;
	int cmd_size;

	while (1) {
		if (!his_reload) {
			printf("$");
			cmd_line = cmd_reader(&cmd_size);
			store_cmd_line(cmd_line, cmd_size);
		} else {
			cmd_line = his_cmd;
			cmd_size = his_reload_size;
		}
		his_reload = 0;
		if (cmd_size == 0)
			continue;
		cmd_parser(cmd_line);
		cmd_launcher();
	}
}

/* checker if words is a buildin cmd */
int check_buildin(char *words)
{
	int i = 0;

	while (buildin_cmds[i] != NULL) {
		if (strcmp(words, buildin_cmds[i]) == 0)
			return 1;
		i++;
	}
	return 0;
}

int check_errno(int status)
{
	if (status == -1)
		fprintf(stderr, "error: %s\n", strerror(errno));
	return status;
}


int check_errno_and_report(int status, char *str)
{
	if (status == -1)
		fprintf(stderr, "error: %s: %s\n", str, strerror(errno));
	return status;
}

void check_errno_and_exit(int status)
{
	if (status == -1) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		free_global();
		exit(1);
	}
}

void *malloc_ck(size_t size)
{
	int timer = MALLOC_ALLOWED_TIMES;
	void *b = NULL;

	while (b == NULL && timer > 0) {
		b = malloc(size);
		timer--;
	}
	if (b == NULL) {
		check_errno(-1);
		free_global();
	}
	return b;
}

void *realloc_ck(void *ptr, size_t size)
{
	int timer = MALLOC_ALLOWED_TIMES;
	void *b = NULL;

	while (b == NULL && timer > 0) {
		b = realloc(ptr, size);
		timer--;
	}
	if (b == NULL) {
		check_errno(-1);
		free_global();
	}
	return b;

}

/* store cmd into his_stack */
void store_cmd_line(char *cmd_line, int cmd_size)
{
	if (cmd_size == 0)
		return;

	char *new_cmd = malloc_ck(sizeof(char) * cmd_size);

	if (new_cmd == NULL) {
		free_global();
		free(cmd_line);
		cmd_line = NULL;
		exit(1);
	}
	strcpy(new_cmd, cmd_line);
	if (his_size < HISTORY_SIZE) {
		his_newest++;
		if (his_newest == HISTORY_SIZE)
			his_newest = 0;
		his_stack[his_newest] = new_cmd;
		his_size++;
		if (-1 == his_oldest)
			his_oldest = 0;
	} else {
		free(his_stack[his_oldest]);
		his_stack[his_oldest] = new_cmd;
		his_newest = his_oldest;
		his_oldest++;
		if (his_oldest == HISTORY_SIZE)
			his_oldest = 0;
	}
}

void empty_history(void)
{
	int i = his_oldest;

	if (his_size == 0)
		return;
	while (i != his_newest) {
		free(his_stack[i]);
		his_stack[i] = NULL;
		i++;
		if (i == HISTORY_SIZE)
			i = 0;
	}
	free(his_stack[i]);
	his_stack[i] = NULL;
	his_size = 0;
	his_oldest = -1;
	his_newest = -1;
}

void remove_newest_record(void)
{
	free(his_stack[his_newest]);
	his_stack[his_newest] = NULL;
	his_newest--;
	his_size--;
	if (his_size == 0)
		his_oldest = -1;
	else if (-1 == his_newest)
		his_newest = HISTORY_SIZE - 1;
}


char *cmd_reader(int *size)
{
	int c;
	int i = 0;
	int b_sz = CMD_BUFFER_SIZE;
	char *buffer = malloc_ck(sizeof(char) * CMD_BUFFER_SIZE);

	if (buffer == NULL) {
		free_global();
		exit(1);
	}

	while (1) {
		if (i == b_sz - 1) {
			b_sz += CMD_BUFFER_SIZE;
			buffer = realloc_ck(buffer, sizeof(char) * b_sz);
			if (buffer == NULL) {
				free_global();
				free(buffer);
				buffer = NULL;
				exit(1);
			}
		}
		c = getchar();
		if (c == EOF || c == '\n') {
			buffer[i] = '\0';
			break;
		}
		buffer[i] = c;
		i++;
	}
	*size = i+1;
	return buffer;

}

void store_cmd_para(char **args, char *arg_w, int *args_i, int *word_i)
{
	arg_w[*word_i] = '\0';
	*word_i = 0;
	args[*args_i] = arg_w;
	if (*args_i < CMD_ARG_NUM - 1)
		*args_i += 1;
}

struct Cmd_s create_cmd(char **args)
{
	struct Cmd_s ret;

	ret.args = args;
	if (check_buildin(args[0]))
		ret.type = 0;
	else
		ret.type = 1;
	return ret;
}

void store_cmd(struct Cmd_s cmd, int *a_i)
{
	*a_i = 0;
	if (cmd_num < CMD_PIPE_NUM) {
		cmds[cmd_num] = cmd;
		cmd_num++;
	}
}


void cmd_parser(char *cmd_line)
{
	struct Cmd_s one_cmd;
	char **args;
	char *arg_w;
	int arg_w_i = 0;
	int args_i = 0;
	int i = 0;
	char c = cmd_line[0];
	int invalid = 1;

	cmd_num = 0;

	cmds = malloc_ck(CMD_PIPE_NUM * sizeof(struct Cmd_s));
	args = malloc_ck(CMD_ARG_NUM * sizeof(char *));
	arg_w = malloc_ck(CMD_ARG_LEN * sizeof(char));
	if (cmds == NULL || args == NULL || arg_w == NULL)
		goto parser_free;


	while ('\0' != c && EOF != c) {
		if ('|' == c) {
			if (!invalid) {
				invalid = 1;
				store_cmd_para(args, arg_w, &args_i, &arg_w_i);
				arg_w = malloc_ck(CMD_ARG_LEN * sizeof(char));
				if (arg_w == NULL) {
					args[args_i] = NULL;
					goto parser_free;
				}
			}
			if (args_i == 0) {
				args[args_i] = NULL;
				free(cmd_line);
				cmd_line = NULL;
				free(arg_w);
				arg_w = NULL;
				free_args(args);
				free_cmds();
				fprintf(stderr, "error: syntax error\n");
				return;
			}
			args[args_i] = NULL;
			one_cmd = create_cmd(args);
			store_cmd(one_cmd, &args_i);
			args = malloc_ck(CMD_ARG_NUM * sizeof(char *));
			if (args == NULL) {
				args[args_i] = NULL;
				goto parser_free;
			}
		} else if (' ' == c && !invalid) {
			invalid = 1;
			store_cmd_para(args, arg_w, &args_i, &arg_w_i);
			arg_w = malloc_ck(CMD_ARG_LEN * sizeof(char));
			if (arg_w == NULL) {
				args[args_i] = NULL;
				goto parser_free;
			}
		} else if (' ' != c) {
			invalid = 0;
			arg_w[arg_w_i] = c;
			if (arg_w_i < CMD_ARG_LEN - 1)
				arg_w_i++;
		}
		i++;
		c = cmd_line[i];
	}

	if (arg_w_i != 0) {
		store_cmd_para(args, arg_w, &args_i, &arg_w_i);
	} else {
		free(arg_w);
		arg_w = NULL;
	}

	if (args_i != 0) {
		args[args_i] = NULL;
		one_cmd = create_cmd(args);
		store_cmd(one_cmd, &args_i);
	} else {
		args[args_i] = NULL;
		free_args(args);
	}

	free(cmd_line);
	cmd_line = NULL;
	return;

parser_free:
	free(cmd_line);
	cmd_line = NULL;
	free(arg_w);
	arg_w = NULL;
	free_args(args);
	free_global();
	exit(1);
}

void free_args(char **args)
{
	int i = 0;

	if (args == NULL)
		return;
	while (args[i] != NULL) {
		free(args[i]);
		args[i] = NULL;
		i++;
	}
	free(args);
	args = NULL;
}

void free_cmds(void)
{
	int i;

	if (cmds == NULL) {
		cmd_num = 0;
		return;
	}
	for (i = 0; i < cmd_num; i++)
		free_args(cmds[i].args);
	free(cmds);
	cmds = NULL;
	cmd_num = 0;
}

void free_global(void)
{
	empty_history();
	free_cmds();
}

int cmd_launcher(void)
{
	int status = 0;

	if (cmd_num == 0) {
		remove_newest_record();
		free_cmds();
		return status;
	}
	if (cmd_num > 1 && check_buildin(cmds[0].args[0])) {
		fprintf(stderr, "error: %s", "buildin in pipe cmd\n");
		free_cmds();
		return status;
	}
	if (cmds[0].type != 0) {
		status = executable_launcher();
	} else if (strcmp(cmds[0].args[0], "cd") == 0) {
		status = execute_cd(cmds[0].args);
	} else if (strcmp(cmds[0].args[0], "history") == 0) {
		status = execute_history(cmds[0].args);
	} else if (strcmp(cmds[0].args[0], "exit") == 0) {
		free_global();
		exit(0);
	} else {
		fprintf(stderr, "error: %s", "unknown cmd\n");
		status = 1;
	}
	free_cmds();
	return status;
}

int executable_launcher(void)
{
	int i;
	pid_t pid;
	pid_t end_id;
	int fd[2];
	int pre = 0;
	int status;
	int err = 0;
	int r = 0;
	char *cm;

	for (i = 0; i < cmd_num; i++) {
		check_errno_and_exit(pipe(fd));
		check_errno_and_exit(pid = fork());
		/* child process */
		if (pid == 0) {
			/*
			 * if this process is not for the 1st cmd,
			 * redirect the input to pipe-read port
			 */
			if (i != 0) {
				check_errno_and_exit(dup2(pre, 0));
				check_errno_and_exit(close(pre));
			}
			/*
			 * if this procee is not for the last cmd,
			 * redirect the output to pipe-write port
			 */
			if (i != cmd_num - 1) {
				check_errno_and_exit(close(fd[0]));
				check_errno_and_exit(dup2(fd[1], 1));
			}
			/*
			 * close the pipe for the last cmd since this
			 * child process does not redirect its output
			 */
			if (i == cmd_num - 1) {
				check_errno_and_exit(close(fd[0]));
				check_errno_and_exit(close(fd[1]));
			}
			cm = cmds[i].args[0];
			r = check_errno_and_report(execv(cm, cmds[i].args), cm);
			free_global();
			exit(r);
		}
		/* close the parent write port since it is useless */
		check_errno_and_exit(close(fd[1]));
		/* close pre parent read port if it is NOT the 1st cmd */
		if (pre != 0)
			check_errno_and_exit(close(pre));
		/* store the read port for the next process */
		pre = fd[0];
		if (i == cmd_num - 1) {
			/* close the pipe for last cmd in parent */
			check_errno_and_exit(close(fd[0]));
			/* wait for all child processes end */
			while (1) {
				end_id = wait(&status);
				r |= status;
				err = errno;
				/* all children is done */
				if (end_id == -1 && err == ECHILD)
					break;
			}
		}
	}
	return r;
}

//TODO cd ~
int execute_cd(char **args)
{
	int r = 0;

	if (args[1] == NULL)
		fprintf(stderr, "error: %s", "Pls provide a directory path\n");
	else
		r = check_errno(chdir(args[1]));
	return r;

}

int execute_history(char **args)
{
	int i = his_oldest;
	int counter = 0;
	int offset;
	int position;
	/* no arguments */
	if (args[1] == NULL) {
		remove_newest_record();
		while (his_size > 0 && i != his_newest) {
			printf("%d %s\n", counter, his_stack[i]);
			i++;
			counter++;
			if (i == HISTORY_SIZE)
				i = 0;
		}
		if (his_size > 0)
			printf("%d %s\n", counter, his_stack[i]);
		return 0;
	}
	/* [-c] */
	else if (strcmp(args[1], "-c") == 0)
		empty_history();
	/* [offset] */
	else if (args[2] == NULL) {
		remove_newest_record();
		if (!is_decimal(args[1])) {
			fprintf(stderr, "error: invalid cmd\n");
			return 0;
		}
		/* position the offset command */
		offset = atoi(args[1]);
		if (offset >= his_size || offset < 0) {
			fprintf(stderr, "error: %s\n", "offset wrong");
			return 1;
		}
		position = his_oldest + offset;
		if (position >= HISTORY_SIZE)
			position -= HISTORY_SIZE;
		his_reload = 1;
		his_reload_size = strlen(his_stack[position]) + 1;
		his_cmd = malloc_ck(sizeof(char) * his_reload_size);
		if (his_cmd == NULL) {
			free_global();
			exit(1);
		}
		strcpy(his_cmd, his_stack[position]);
		return 0;
	} else {
		remove_newest_record();
		fprintf(stderr, "error: invalid cmd\n");
	}
	return 0;
}

int is_decimal(char *s)
{
	int i = 0;
	char c = s[0];
	int first = 1;

	if (c == '0' && s[1] == '\0')
		return 1;
	while (c != '\0') {
		if (c < '0' || c > '9')
			return 0;
		else if (c == '0' && first)
			return 0;
		else if (c != '0' && first)
			first = 0;
		i++;
		c = s[i];
	}
	return 1;
}







