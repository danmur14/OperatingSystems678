/**
 * @file execute.c
 *
 * @brief Implements interface functions between Quash and the environment and
 * functions that interpret an execute commands.
 *
 * @note As you add things to this file you may want to change the method signature
 */

#define _GNU_SOURCE

#include "execute.h"

#include <stdio.h>
#include <sys/wait.h>

#include "quash.h"
#include "deque.h"

#define READ 0
#define WRITE 1

IMPLEMENT_DEQUE_STRUCT(PID_QUEUE, pid_t);
IMPLEMENT_DEQUE(PID_QUEUE, pid_t);

/* Job structure to hold jobs in queue */
typedef struct Job
{
	int job_id;
	PID_QUEUE pid_queue;
	char *cmd;
} Job;
IMPLEMENT_DEQUE_STRUCT(JOB_QUEUE, struct Job);
IMPLEMENT_DEQUE(JOB_QUEUE, struct Job);
JOB_QUEUE job_queue_pointer;

/* Pipe tracker */
static int environment_pipes[2][2];
static int prev_pipe = -1;
static int next_pipe = 0;

static bool init_flag = true;

/***************************************************************************
 * Interface Functions
 ***************************************************************************/

// Return a string containing the current working directory.
char *get_current_directory(bool *should_free)
{
	// Change this to true if necessary
	*should_free = true;

	char *cwd = (char *)get_current_dir_name();

	return cwd;
}

// Returns the value of an environment variable env_var
const char *lookup_env(const char *env_var)
{
	// Lookup environment variables. This is required for parser to be able
	// to interpret variables from the command line and display the prompt
	// correctly
	return getenv(env_var);
}

// Check the status of background jobs
void check_jobs_bg_status()
{
	// Check on the statuses of all processes belonging to all background
	// jobs. This function should remove jobs from the jobs queue once all
	// processes belonging to a job have completed.
	int length_q = length_JOB_QUEUE (&job_queue_pointer);

	for(int i = 0; i < length_q; i++) {
		Job temp_job = pop_front_JOB_QUEUE(&job_queue_pointer);
		int wait;
		int length_p = length_PID_QUEUE (&temp_job.pid_queue);
		for(int j = 0; j < length_p; j++) {
			pid_t temp_pid = pop_front_PID_QUEUE(&temp_job.pid_queue);
			// still going
			if(waitpid(temp_pid, &wait, WNOHANG) == 0) {
				push_back_PID_QUEUE(&temp_job.pid_queue, temp_pid);
			}
		}

		if (is_empty_PID_QUEUE (&temp_job.pid_queue)) {
			print_job_bg_complete(temp_job.job_id, 1234, temp_job.cmd);
			free (temp_job.cmd);
			destroy_PID_QUEUE (&temp_job.pid_queue);
		} else {
			push_back_JOB_QUEUE(&job_queue_pointer, temp_job);
		}
	}
}

// Prints the job id number, the process id of the first process belonging to
// the Job, and the command string associated with this job
void print_job(int job_id, pid_t pid, const char *cmd)
{
	printf("[%d]\t%8d\t%s\n", job_id, pid, cmd);
	fflush(stdout);
}

// Prints a start up message for background processes
void print_job_bg_start(int job_id, pid_t pid, const char *cmd)
{
	printf("Background job started: ");
	print_job(job_id, pid, cmd);
}

// Prints a completion message followed by the print job
void print_job_bg_complete(int job_id, pid_t pid, const char *cmd)
{
	printf("Completed: \t");
	print_job(job_id, pid, cmd);
}

/***************************************************************************
 * Functions to process commands
 ***************************************************************************/
// Run a program reachable by the path environment variable, relative path, or
// absolute path
void run_generic(GenericCommand cmd)
{
	// Execute a program with a list of arguments. The `args` array is a NULL
	// terminated (last string is always NULL) list of strings. The first element
	// in the array is the executable
	char *exec = cmd.args[0];
	char **args = cmd.args;

	execvp(exec, args);

	perror("ERROR: Failed to execute program");
}

// Print strings
void run_echo(EchoCommand cmd)
{
	// Print an array of strings. The args array is a NULL terminated (last
	// string is always NULL) list of strings.
	char **str = cmd.args;

	int i = 0;
	while (str[i] != NULL)
	{
		printf(str[i]);
		i++;
	}
	printf("\n");

	// Flush the buffer before returning
	fflush(stdout);
}

// Sets an environment variable
void run_export(ExportCommand cmd)
{
	// Write an environment variable
	const char *env_var = cmd.env_var;
	const char *val = cmd.val;

	setenv(env_var, val, 1);
}

// Changes the current working directory
void run_cd(CDCommand cmd)
{
	// Get the directory name
	const char *dir = cmd.dir;
	char *cwd = (char *)get_current_dir_name();

	// Check if the directory is valid
	if (dir == NULL)
	{
		perror("ERROR: Failed to resolve path");
		return;
	}

	// Change directory
	chdir(dir);

	// Update the PWD environment variable to be the new current working
	// directory and optionally update OLD_PWD environment variable to be the old
	// working directory.
	setenv("PWD", dir, 1);
	setenv("OLD_PWD", cwd, 1);

	free(cwd);
}

// Sends a signal to all processes contained in a job
void run_kill(KillCommand cmd)
{
	int signal = cmd.sig;
	int job_id = cmd.job;

	Job temp_job;
	int length_q = length_JOB_QUEUE (&job_queue_pointer);

	for(int i = 0; i < length_q; i++) {
		temp_job = pop_front_JOB_QUEUE(&job_queue_pointer);
		if(temp_job.job_id == job_id) {
			int length_p = length_PID_QUEUE (&temp_job.pid_queue);
			for(int j = 0; j < length_p; j++) {
				pid_t temp_id = pop_front_PID_QUEUE(&temp_job.pid_queue);
				kill(temp_id, signal);
			}
		}
		push_back_JOB_QUEUE(&job_queue_pointer, temp_job);
	}

	// Kill all processes associated with a background job
}

// Prints the current working directory to stdout
void run_pwd()
{
	// Print the current working directory
	char *cwd = (char *)get_current_dir_name();
	printf(cwd);
	printf("\n");

	free(cwd);

	// Flush the buffer before returning
	fflush(stdout);
}

// Prints all background jobs currently in the job list to stdout
void run_jobs()
{
	// Print background jobs
	if(is_empty_JOB_QUEUE(&job_queue_pointer)) {
		return;
	}

	Job temp_job;
	for(int i = 0; i < length_JOB_QUEUE(&job_queue_pointer); i++) {
		temp_job = pop_front_JOB_QUEUE(&job_queue_pointer);
		print_job(temp_job.job_id, 1234, temp_job.cmd);
		push_back_JOB_QUEUE(&job_queue_pointer, temp_job);
	}

	// Flush the buffer before returning
	fflush(stdout);
}

/***************************************************************************
 * Functions for command resolution and process setup
 ***************************************************************************/

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for child processes.
 *
 * This version of the function is tailored to commands that should be run in
 * the child process of a fork.
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void child_run_command(Command cmd)
{
	CommandType type = get_command_type(cmd);

	switch (type)
	{
		case GENERIC:
			run_generic(cmd.generic);
			break;

		case ECHO:
			run_echo(cmd.echo);
			break;

		case PWD:
			run_pwd();
			break;

		case JOBS:
			run_jobs();
			break;
		case EXPORT:
			break;
		case CD:
			break;
		case KILL:
			break;
		case EXIT:
			break;
		case EOC:
			break;

		default:
			fprintf(stderr, "Unknown command type: %d\n", type);
	}
}

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for the quash process.
 *
 * This version of the function is tailored to commands that should be run in
 * the parent process (quash).
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void parent_run_command(Command cmd)
{
	CommandType type = get_command_type(cmd);

	switch (type)
	{
		case EXPORT:
			run_export(cmd.export);
			break;

		case CD:
			run_cd(cmd.cd);
			break;

		case KILL:
			run_kill(cmd.kill);
			break;
		case GENERIC:
			break;
		case ECHO:
			break;
		case PWD:
			break;
		case JOBS:
			break;
		case EXIT:
		case EOC:
			break;

		default:
			fprintf(stderr, "Unknown command type: %d\n", type);
	}
}

/**
 * @brief Creates one new process centered around the @a Command in the @a
 * CommandHolder setting up redirects and pipes where needed
 *
 * @note Processes are not the same as jobs. A single job can have multiple
 * processes running under it. This function creates a process that is part of a
 * larger job.
 *
 * @note Not all commands should be run in the child process. A few need to
 * change the quash process in some way
 *
 * @param holder The CommandHolder to try to run
 *
 * @sa Command CommandHolder
 */
void create_process(CommandHolder holder, Job *cJob)
{

	// Read the flags field from the parser
	bool p_in = holder.flags & PIPE_IN;
	bool p_out = holder.flags & PIPE_OUT;
	bool r_in = holder.flags & REDIRECT_IN;
	bool r_out = holder.flags & REDIRECT_OUT;
	bool r_app = holder.flags & REDIRECT_APPEND; // This can only be true if r_out is true

	if (p_out)
	{
		if (pipe(environment_pipes[next_pipe]) == -1)
		{
			perror("Pipe Error");
			exit(EXIT_FAILURE);
		}
	}

	pid_t pid_0;

	pid_0 = fork();
	/*Child*/
	if (pid_0 == 0)
	{
		if (r_in)
		{
			FILE *r_input = fopen(holder.redirect_in, "r");
			dup2(fileno(r_input), STDIN_FILENO);
		}
		if (r_out)
		{
			if (r_app)
			{
				FILE *r_output = fopen(holder.redirect_out, "a");
				dup2(fileno(r_output), STDOUT_FILENO);
			}
			else
			{
				FILE *r_output = fopen(holder.redirect_out, "w");
				dup2(fileno(r_output), STDOUT_FILENO);
			}
		}

		if (p_in)
		{
			dup2(environment_pipes[prev_pipe][READ], STDIN_FILENO);
			close(environment_pipes[prev_pipe][WRITE]);
		}

		if (p_out)
		{
			dup2(environment_pipes[next_pipe][WRITE], STDOUT_FILENO);
			close(environment_pipes[next_pipe][READ]);
		}

		/* Run child command */
		child_run_command(holder.cmd); // This should be done in the child branch of a fork
		exit(0);
	}
	else
	{
		if (p_in)
		{
			close(environment_pipes[prev_pipe][READ]);
		}
		if (p_out)
		{
			close(environment_pipes[next_pipe][WRITE]);
		}

		next_pipe = (next_pipe + 1) % 2;
		prev_pipe = (prev_pipe + 1) % 2;

		push_front_PID_QUEUE(&cJob->pid_queue, pid_0);

		/* Run parent command */
		parent_run_command(holder.cmd); // This should be done in the parent branch of a fork
	}
}

// Run a list of commands
void run_script(CommandHolder *holders)
{
	if (holders == NULL)
		return;

	if (init_flag) {
		job_queue_pointer = new_JOB_QUEUE(1);
		init_flag = false;
	}

	check_jobs_bg_status();

	if (get_command_holder_type(holders[0]) == EXIT &&
			get_command_holder_type(holders[1]) == EOC)
	{
		end_main_loop();
		return;
	}

	Job new_job;
	new_job.pid_queue = new_PID_QUEUE (1);

	CommandType type;

	// Run all commands in the `holder` array
	for (int i = 0; (type = get_command_holder_type(holders[i])) != EOC; ++i)
		create_process(holders[i], &new_job);

	if (!(holders[0].flags & BACKGROUND))
	{
		// Not a background Job
		// Wait for all processes under the job to complete
		while(!(is_empty_PID_QUEUE(&new_job.pid_queue))) {
			pid_t temp_pid = pop_front_PID_QUEUE(&new_job.pid_queue);
			int wait;
			waitpid(temp_pid, &wait, 0);
		}

		destroy_PID_QUEUE (&new_job.pid_queue);
	}
	else
	{
		new_job.cmd = get_command_string ();
		new_job.job_id = length_JOB_QUEUE (&job_queue_pointer) + 1;

		// Once jobs are implemented, uncomment and fill the following line
		print_job_bg_start(new_job.job_id, peek_front_PID_QUEUE(&new_job.pid_queue), new_job.cmd);

		// A background job.
		// Push the new job to the job queue
		push_back_JOB_QUEUE(&job_queue_pointer, new_job);
	}
}
