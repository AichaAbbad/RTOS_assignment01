/* In the following code we have an application with three periodic tasks
  J1 with T1 = 300 ms
  J2 with T2 = 500 ms
  J3 with T3 = 800 ms
  And an aperiodic task J4 in the bbackground triggered by task J2
  Every task shall just waste time and write its identifier to the driver using
  the "write" system call

  The output of this code shows the string in the kernel log written by the system.
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>

//code of periodic tasks
void task1_code( );
void task2_code( );
void task3_code( );

//code of aperiodic tasks
void task4_code( );

//characteristic function of the thread, only for timing and synchronization
//periodic tasks
void *task1( void *);
void *task2( void *);
void *task3( void *);

//aperiodic tasks
void *task4( void *);

// Function for writting in the kernel
void call_driver(const char *);

// initialization of mutex and condition (aperiodic scheduling J4)
pthread_mutex_t mutex_task_4 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_task_4 = PTHREAD_COND_INITIALIZER;

#define INNERLOOP 1050
#define OUTERLOOP 1500

#define NPERIODICTASKS 3
#define NAPERIODICTASKS 1
#define NTASKS NPERIODICTASKS + NAPERIODICTASKS

long int periods[NTASKS];
struct timespec next_arrival_time[NTASKS];
double WCET[NTASKS];
pthread_attr_t attributes[NTASKS];
pthread_t thread_id[NTASKS];
struct sched_param parameters[NTASKS];
int missed_deadlines[NTASKS];

int main()
{
  // set task periods in nanoseconds
	//the first task has period 300 millisecond
	//the second task has period 500 millisecond
	//the third task has period 800 millisecond
  periods[0]= 100000000; //in nanoseconds
  periods[1]= 200000000; //in nanoseconds
  periods[2]= 400000000; //in nanoseconds

  //for aperiodic tasks we set the period equals to 0
  periods[3]= 0; 

  struct sched_param priomax;
  priomax.sched_priority=sched_get_priority_max(SCHED_FIFO);
  struct sched_param priomin;
  priomin.sched_priority=sched_get_priority_min(SCHED_FIFO);

	// set the maximum priority to the current thread 

  if (getuid() == 0)
      pthread_setschedparam(pthread_self(),SCHED_FIFO,&priomax);


 	int i;
  for (i =0; i < NTASKS; i++){

		// initializa time_1 and time_2 required to read the clock
		struct timespec time_1, time_2;
		clock_gettime(CLOCK_REALTIME, &time_1);

		//we should execute each task more than one for computing the WCET
		//periodic tasks
 	  if (i==0)
			task1_code();
    if (i==1)
			task2_code();
    if (i==2)
			task3_code();
      		
    //aperiodic tasks
    if (i==3)
      task4_code();

		clock_gettime(CLOCK_REALTIME, &time_2);


		// compute the Worst Case Execution Time (in a real case, we should repeat this many times under
		//different conditions, in order to have reliable values

    WCET[i]= 1000000000*(time_2.tv_sec - time_1.tv_sec)+(time_2.tv_nsec-time_1.tv_nsec);
    printf("\nWorst Case Execution Time %d=%f \n", i, WCET[i]);
  }

  // compute U
	double U = WCET[0]/periods[0]+WCET[1]/periods[1]+WCET[2]/periods[2];

  // compute Ulub by considering the fact that we have harmonic relationships between periods
	double Ulub = 0.75682846;
	
	//check the sufficient conditions: if they are not satisfied, exit  
  if (U > Ulub){
    printf("\n U=%lf Ulub=%lf Non schedulable Task Set", U, Ulub);
    return(-1);
  }
  printf("\n U=%lf Ulub=%lf Scheduable Task Set", U, Ulub);
  fflush(stdout);
  sleep(5);

  	// set the minimum priority to the current thread

  if (getuid() == 0)
      pthread_setschedparam(pthread_self(),SCHED_FIFO,&priomin);

  // set the attributes of each task, including scheduling policy and priority
  for (i =0; i < NPERIODICTASKS; i++){
    //initializa the attribute structure of task i
    pthread_attr_init(&(attributes[i]));

    //set the attributes to tell the kernel that the priorities and policies
    pthread_attr_setinheritsched(&(attributes[i]), PTHREAD_EXPLICIT_SCHED);
    
    // set the attributes to set the SCHED_FIFO policy (pthread_attr_setschedpolicy)
    pthread_attr_setschedpolicy(&(attributes[i]), SCHED_FIFO);

    //properly set the parameters to assign the priority inversely proportional to the period
    parameters[i].sched_priority = sched_get_priority_max(SCHED_FIFO) - i -1;

    //set the attributes and the parameters of the current thread (pthread_attr_setschedparam)
    pthread_attr_setschedparam(&(attributes[i]), &(parameters[i]));
  }

 	// aperiodic tasks
  for (int i =NPERIODICTASKS; i < NTASKS; i++){
    pthread_attr_init(&(attributes[i]));
    pthread_attr_setschedpolicy(&(attributes[i]), SCHED_FIFO);

    //set minimum priority (background scheduling)
    parameters[i].sched_priority = 0;
    pthread_attr_setschedparam(&(attributes[i]), &(parameters[i]));
  }


	//delare the variable to contain the return values of pthread_create	
  int thread[NTASKS];

	//declare variables to read the current time
	struct timespec time_1;
	clock_gettime(CLOCK_REALTIME, &time_1);

  // set the next arrival time for each task
  for (i = 0; i < NPERIODICTASKS; i++){
		long int next_arrival_nanoseconds = time_1.tv_nsec + periods[i];
		//then we compute the end of the first period and beginning of the next one
		next_arrival_time[i].tv_nsec= next_arrival_nanoseconds%1000000000;
		next_arrival_time[i].tv_sec= time_1.tv_sec + next_arrival_nanoseconds/1000000000;
    missed_deadlines[i] = 0;
  }

	// create all threads(pthread_create)
  thread[0] = pthread_create( &(thread_id[0]), &(attributes[0]), task1, NULL);
  thread[1] = pthread_create( &(thread_id[1]), &(attributes[1]), task2, NULL);
  thread[2] = pthread_create( &(thread_id[2]), &(attributes[2]), task3, NULL);
  thread[3] = pthread_create( &(thread_id[3]), &(attributes[3]), task4, NULL);

  // join all threads (pthread_join)
  pthread_join( thread_id[0], NULL);
  pthread_join( thread_id[1], NULL);
  pthread_join( thread_id[2], NULL);

  // set the next arrival time for each task. This is not the beginning of the first
	// period, but the end of the first period and beginning of the next one. 
  for (i = 0; i < NTASKS; i++){
    printf ("\nMissed Deadlines Task %d=%d", i, missed_deadlines[i]);
    fflush(stdout);
  }
  exit(0);
}

// function to write to the driver
void call_driver(const char *my_string){
  int fd, result, len;
  char buf[2];

  if ((fd = open ("/dev/driver", O_RDWR)) == -1) {
    perror("open failed");
  }
  len = strlen(my_string)+1;
  if ((result = write (fd, my_string, len)) != len) 
  {
    perror("write failed");
  }
  printf(" %s ", my_string);
  fflush(stdout);
  close(fd);
}

// code for task 1
void task1_code(){
  //print the id of the current task
  const char *str = "1[";
  call_driver(str);

  // waste time
  int i,j;
  int uno;
  for (i = 0; i < OUTERLOOP; i++){
    for (j = 0; j < INNERLOOP; j++){
      uno = rand()*rand()%10;
    }
  }
  //print the id of the current task
  str = "]1";
  call_driver(str);
  fflush(stdout);
}

void *task1(void *ptr){
  cpu_set_t cset;
	CPU_ZERO (&cset);
	CPU_SET(0, &cset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cset);

    //execute the task one hundred times
  int i=0;
  for(i=0;i<100;i++){
    task1_code();
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_arrival_time[0], NULL);
    long int next_arrival_nanoseconds = next_arrival_time[0].tv_nsec + periods[0];
    next_arrival_time[0].tv_nsec= next_arrival_nanoseconds%1000000000;
    next_arrival_time[0].tv_sec= next_arrival_time[0].tv_sec + next_arrival_nanoseconds/1000000000;
  }
}

// code for task 2
void task2_code(){

	//print the id of the current task
  const char *str = "2[";
  call_driver(str);

	int i,j;
	double uno;
  	for (i = 0; i < OUTERLOOP; i++){
      for (j = 0; j < INNERLOOP; j++){
			  uno = rand()*rand()%10;
		}
	}

    // for scheduling J4
    if(uno == 0){
      str = "(4)";
      call_driver(str);
      pthread_cond_signal(&cond_task_4);
    }

	//print the id of the current task
  str = "]2";
  call_driver(str);
}

void *task2(void *ptr){
  cpu_set_t cset;
	CPU_ZERO (&cset);
	CPU_SET(0, &cset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cset);

  //execute the task one hundred times
  int i=0;
  for(i=0;i<100;i++){
    task2_code();
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_arrival_time[1], NULL);
    long int next_arrival_nanoseconds = next_arrival_time[1].tv_nsec + periods[1];
    next_arrival_time[1].tv_nsec= next_arrival_nanoseconds%1000000000;
    next_arrival_time[1].tv_sec= next_arrival_time[1].tv_sec + next_arrival_nanoseconds/1000000000;
  }
}

// code for task 3
void task3_code(){

	//print the id of the current task
  const char *str = "3[";
  call_driver(str);

	int i,j;
    int uno;
  	for (i = 0; i < OUTERLOOP; i++){
      for (j = 0; j < INNERLOOP; j++){
			  uno = rand()*rand()%10;
		}
	}

	//print the id of the current task
  str = "]3";
  call_driver(str);
}

void *task3(void *ptr){
  cpu_set_t cset;
	CPU_ZERO (&cset);
	CPU_SET(0, &cset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cset);

    //execute the task one hundred times
  int i=0;
  for(i=0;i<100;i++){
    task3_code();
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next_arrival_time[2], NULL);
    long int next_arrival_nanoseconds = next_arrival_time[2].tv_nsec + periods[2];
    next_arrival_time[2].tv_nsec= next_arrival_nanoseconds%1000000000;
    next_arrival_time[2].tv_sec= next_arrival_time[2].tv_sec + next_arrival_nanoseconds/1000000000;
  }
}

// code for task 4
void task4_code(){

	//print the id of the current task
  const char *str = "4[";
  call_driver(str);

	int i,j;
    int uno;
  	for (i = 0; i < OUTERLOOP; i++){
      for (j = 0; j < INNERLOOP; j++){
			  uno = rand()*rand()%10;
		}
	}
	//print the id of the current task
  str = "]4";
  call_driver(str);
}

void *task4(void *ptr){
  cpu_set_t cset;
	CPU_ZERO (&cset);
	CPU_SET(0, &cset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cset);

  while (1){
		pthread_cond_wait(&cond_task_4, &mutex_task_4);
 		task4_code();
	}
}