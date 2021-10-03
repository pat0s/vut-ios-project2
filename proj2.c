#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/ipc.h>   
#include <sys/shm.h>
#include <semaphore.h>
#include <time.h>
#include <sys/wait.h>

enum process {MAIN, SANTA, ELF, REINDEER};

typedef struct{
	int type;
	int id;
}Proc;

typedef struct{
	int reindeers_away;
	int reindeers_hitched;
	bool closed;
	int elves_wait;
	int elves_get_help;
	int order;
	int shmid;
	sem_t sem_write;
	sem_t sem_vacation;
	sem_t sem_help;
	sem_t sem_close_workshop;
	sem_t sem_christmas;
	sem_t sem_get_hitched;
	sem_t sem_santa;
	sem_t sem_elves;
	sem_t sem_all_get_help;
}SharedMem;

#define shared_mem_key 4242

// Validate arguments
int check_arguments(int argc, char* argv[], int* NE, int* NR, int* TE, int *TR)
{
	for (int i = 1; i < argc; i++)
	{
		int tmp = atoi(argv[i]);
		switch (i)
		{
			case 1:
				if (tmp <= 0 || tmp >= 1000)
				{
					fprintf(stderr, "%s %d\n", "Incorrect argument", i); 
					return 1;
				}
				*NE = tmp;
				break;
			case 2:	
				if (tmp <= 0 || tmp >= 20)
				{
					fprintf(stderr, "%s %d\n", "Incorrect argument", i); 
					return 1;
				}
				*NR = tmp;
				break;
			case 3:
				if (tmp < 0 || tmp > 1000)
				{
					fprintf(stderr, "%s %d\n", "Incorrect argument", i); 
					return 1;
				}
				*TE = tmp;
				break;
			case 4:	
				if (tmp < 0 || tmp > 1000)
				{
					fprintf(stderr, "%s %d\n", "Incorrect argument", i); 
					return 1;
				}
				*TR = tmp;
				break;
		}
	}
	return 0;
}

// Semaphore inicialization
int sem_inicialization(sem_t *sem, int init_value)
{
	if (sem_init(sem, 1, init_value) == -1)
	{
		fprintf(stderr, "%s\n", "Error: Sem_init failed.");
		return 1;
	}
	return 0;
}

// Create process using fork
int create_process(int set_type, int *type)
{
	pid_t pid;
	// Child process
	if ((pid = fork()) == 0)
	{
		*type = set_type;
	}
	// Fork error
	else if (pid < 0)
	{
		fprintf(stderr, "Error: Fork failed to create child process\n");
		return 1;	
	}

	return 0;
}

// Create processes
int create_processes(int set_type, int *type, int *id, int n)
{
	for (int i = 0; i < n; i++)
	{
		if (create_process(set_type, type) == 1) return 1;	 
		
		*id = i + 1;

		if (*type != MAIN) return 0;
	}
	return 0;
}

// Print output to file
int print_message(FILE *file, Proc process, SharedMem *shm, const char *msg)
{
	sem_wait(&shm->sem_write);

	char *name;
	switch (process.type)
	{
		case SANTA: name = "Santa"; 
			break;
		case ELF: name = "Elf";
			break;
		case REINDEER: name = "RD";
			break;
		default: 
			break;
	}

	if (process.type != SANTA)
		fprintf(file," %d: %s %d: %s\n", (shm->order)++, name, process.id, msg);
	else
		fprintf(file," %d: %s: %s\n", (shm->order)++, name, msg);
	
	fflush(file);
	setbuf(file, NULL);
	sem_post(&shm->sem_write);
	
	return 0;
}

// Elves processes
void* elves_lives(FILE *file, Proc process, SharedMem *shm, int TE)
{
	while (true)
	{
		int time = random() % (TE + 1);
		usleep(time);
		print_message(file, process, shm, "need help");

		sem_wait(&shm->sem_help);
		shm->elves_wait++;
		sem_post(&shm->sem_help);

		if (shm->elves_wait == 3) sem_post(&shm->sem_santa);

		sem_wait(&shm->sem_elves);

		// Workshop is closed
		if (shm->closed)
			break;
		
		print_message(file, process, shm, "get help");

		sem_wait(&shm->sem_help);
		shm->elves_get_help++;
		sem_post(&shm->sem_help);
		if (shm->elves_get_help == 3) sem_post(&shm->sem_all_get_help);
	}

	print_message(file, process, shm, "taking holidays");	
	exit(0);
}

// Reindeers processes
void* reindeers_lives(FILE *file, Proc process, SharedMem *shm, int TR)
{
	int time = random() % (TR + 1 - TR/2) + (TR / 2);
	usleep(time);
	print_message(file, process, shm, "return home");

	// Return home from vacation
	sem_wait(&shm->sem_vacation);
	shm->reindeers_away--;
	sem_post(&shm->sem_vacation);

	// All reindeers are at home
	if (shm->reindeers_away == 0) sem_post(&shm->sem_santa);

	// Hitching reindeers
	sem_wait(&shm->sem_get_hitched);
	print_message(file, process, shm, "get hitched");
	shm->reindeers_hitched--;
	sem_post(&shm->sem_get_hitched);

	// All hitched
	if (shm->reindeers_hitched == 0)
		sem_post(&shm->sem_christmas);

	exit(0);
}

// Santa's life
void* santa_life(FILE *file, Proc process, SharedMem *shm, int NE, int NR)
{
	while (true)
	{
		sem_wait(&shm->sem_santa);

		if(shm->reindeers_away == 0)
		{
			print_message(file, process, shm, "closing workshop");
			shm->closed = true;
			
			int waiting = NE;
			while (waiting--) sem_post(&shm->sem_elves);
			waiting = NR;
			while (waiting--) sem_post(&shm->sem_get_hitched);
			sem_post(&shm->sem_get_hitched);
			break;
		}
		else
		{
			sem_wait(&shm->sem_help);
			shm->elves_get_help = 0;
			sem_post(&shm->sem_help);
			print_message(file, process, shm, "helping elves");
			
			int count = 3;
			while (count--) sem_post(&shm->sem_elves);
			
			sem_wait(&shm->sem_all_get_help);
			print_message(file, process, shm, "going to sleep");
		}
	}

	// Semaphore will open when the last reindeer is hitched
	sem_wait(&shm->sem_christmas);
	print_message(file, process, shm, "Christmas started");

	exit(0);
}

// Main
int main(int argc, char *argv[])
{
	// ------------------- Check arguments --------------------------
	if (argc != 5)
	{
		fprintf(stderr, "Incorrect number of arguments\n");
		return 1;
	}

	int NE, NR, TE, TR = 0;
	check_arguments(argc, argv, &NE, &NR, &TE, &TR);
	
	// ------------------- Shared memory ----------------------------
	int shmid = shmget(shared_mem_key, sizeof(SharedMem), IPC_CREAT | 0666);
	if (shmid < 0)
	{
		fprintf(stderr, "%s\n", "Error: Shmget failed.");
		return 1;
	}

	SharedMem *shm = shmat(shmid, NULL, 0);
	if (shm == (SharedMem*) -1)
	{
		fprintf(stderr, "%s\n", "Error: Shmat failed.");
		return 1;
	}
	
	// Share memory inicialization
	shm->shmid = shmid;
	shm->order = 1;
	shm->reindeers_away = NR;
	shm->reindeers_hitched = NR;
	shm->closed = false;
	shm->elves_wait = 0;
	shm->elves_get_help = 0;

	// ------------------- Inicialize semaphores --------------------
	if (sem_inicialization(&shm->sem_write, 1) == 1) return 1;
	if (sem_inicialization(&shm->sem_vacation, 1) == 1) return 1;
	if (sem_inicialization(&shm->sem_help, 1) == 1) return 1;
	if (sem_inicialization(&shm->sem_christmas, 0) == 1) return 1;
	if (sem_inicialization(&shm->sem_get_hitched, 0) == 1) return 1;
	if (sem_inicialization(&shm->sem_santa, 0) == 1) return 1;
	if (sem_inicialization(&shm->sem_elves, 0) == 1) return 1;
	if (sem_inicialization(&shm->sem_all_get_help, 0) == 1) return 1;

	// ------------------- Open output file -------------------------
	FILE *output;
	if ((output = fopen("proj2.out", "w")) == NULL)
	{
		fprintf(stderr, "%s\n", "Error: File does not exist."); 
		return 1;
	}
	
	// -------------------------- Messages --------------------------
	const char *start[] = {"nic", "going to sleep", "started", "rstarted"};
	
	srand(time(NULL));

	// ------------------- Create processes -------------------------
	// Process info
	Proc process_info;
	process_info.type = MAIN;
	
	// Create SANTA process
	if (process_info.type == MAIN)
		if (create_process(SANTA, &process_info.type) == 1) return 1;

	// Create ELVES processes
	if (process_info.type == MAIN) 
		create_processes(ELF, &process_info.type, &process_info.id, NE);

	// Create REINDEERS processes
	if (process_info.type == MAIN) 
		create_processes(REINDEER, &process_info.type, &process_info.id, NR);
	
	// Start message from each process
	if (process_info.type != MAIN)
		print_message(output, process_info, shm, start[process_info.type]);

	// Process life cycles
	switch(process_info.type)
	{
		case SANTA: santa_life(output, process_info, shm, NE, NR);
			break;
		case ELF: elves_lives(output, process_info, shm, TE);
			break;
		case REINDEER: reindeers_lives(output, process_info, shm, TR);
			break;
		default:
			break;
	}
	
	// Wait for all child processes
	while(wait(NULL) >= 0);

	// DELETE semaphores
	sem_destroy(&shm->sem_write);
	sem_destroy(&shm->sem_vacation);
	sem_destroy(&shm->sem_help);
	sem_destroy(&shm->sem_get_hitched);
	sem_destroy(&shm->sem_christmas);
	sem_destroy(&shm->sem_santa);
	sem_destroy(&shm->sem_elves);
	sem_destroy(&shm->sem_all_get_help);

	// DELETE shared memory
	shmid = shm->shmid;
	if (shmdt(shm))
	{
		fprintf(stderr, "%s\n", "Error: Shmdt failed");
		return 1;
	}
	if (shmctl(shmid, IPC_RMID, NULL) == -1)
	{
		fprintf(stderr, "%s\n", "Error: Shmctl failed");
	}

	// Close file
	if (fclose(output) == EOF)
	{
		fprintf(stderr, "%s\n", "Error: File could not be closed.");
		return 1;
	}

	return 0;	
}
