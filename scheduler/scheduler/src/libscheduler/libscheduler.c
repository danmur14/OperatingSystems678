/** @file libscheduler.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libscheduler.h"
#include "../libpriqueue/libpriqueue.h"

/**
  Stores information making up a job to be scheduled including any statistics.

  You may need to define some global variables or a struct to store your job queue elements. 
*/
typedef struct _job_t
{
  int t_arrival;   //arrival time
  int t_total;     //total job time
  int t_remaining; //time remaining

  int t_waiting; //time spent waiting
  int t_updated;  //last timestep where something happened
  int t_started; //time started
  int t_end; //time ended

  int job_id; //job number
  int priority;
  int j_core; //core job is assigned to 

} job_t;

typedef struct _cores_t
{
  job_t **c_job; //array for cores, each index is core_id
  int n_cores;   //total number of cores
} cores_t;

priqueue_t *jobs;                       //queue for jobs
cores_t *c_cores;                       //struct for cores
scheme_t t_scheme;                      //current scheme type
int c_time;                             //current time
int n_jobs;                             //total number of jobs
float t_response, t_wait, t_turnaround; //scheduler performance variables

/**
  Initalizes the scheduler.
 
  Assumptions:
    - You may assume this will be the first scheduler function called.
    - You may assume this function will be called only once.
    - You may assume that cores is a positive, non-zero number.
    - You may assume that scheme is a valid scheduling scheme.

  @param cores the number of cores that is available by the scheduler. These cores will be known as core(id=0), core(id=1), ..., core(id=cores-1).
  @param scheme  the scheduling scheme that should be used. This value will be one of the six enum values of scheme_t
*/
void scheduler_start_up(int cores, scheme_t scheme)
{
  //create structs
  c_cores = (cores_t *)malloc(sizeof(cores_t));
  jobs = (priqueue_t *)malloc(sizeof(priqueue_t));
  priqueue_init(jobs, &comparer);

  //set amount of cores
  c_cores->n_cores = cores;
  c_cores->c_job = (job_t **)malloc(sizeof(job_t *) * cores);
  for (int i = 0; i < cores; i++)
  {
    c_cores->c_job[i] = NULL;
  }

  t_scheme = scheme;
  c_time = 0;
  t_response = 0.0;
  t_wait = 0.0;
  t_turnaround = 0.0;
}

/**
  Called when a new job arrives.
 
  If multiple cores are idle, the job should be assigned to the core with the
  lowest id.
  If the job arriving should be scheduled to run during the next
  time cycle, return the zero-based index of the core the job should be
  scheduled on. If another job is already running on the core specified,
  this will preempt the currently running job.
  Assumptions:
    - You may assume that every job wil have a unique arrival time.

  @param job_number a globally unique identification number of the job arriving.
  @param time the current time of the simulator.
  @param running_time the total number of time units this job will run before it will be finished.
  @param priority the priority of the job. (The lower the value, the higher the priority.)
  @return index of core job should be scheduled on
  @return -1 if no scheduling changes should be made. 
 
 */
int scheduler_new_job(int job_number, int time, int running_time, int priority)
{
  //set new job properties
  job_t *job = (job_t *)malloc(sizeof(job_t));
  job->t_total = running_time;
  job->t_remaining = running_time;
  job->t_arrival = time;
  job->job_id = job_number;
  job->priority = priority;
  job->t_started = -1;
  job->t_updated = -1;
  job->j_core = -1;

  c_time = time;
  n_jobs++;

  //increment time
  increment_timestep();

  //check for free core
  int i = 0;
  while (i < c_cores->n_cores && c_cores->c_job[i] != NULL)
  {
    i++;
  }

  //used for preemption
  int coretochange = -1;
  int corejoblength = -1;
  int corepriority = -1;
  if (i < c_cores->n_cores) // free core
  {
    job->t_waiting = 0;      //arrived and started
    job->t_started = c_time; //started now
    job->t_updated = c_time; //updated now
    job->j_core = i;           //what core
    c_cores->c_job[i] = job;   //core occupied
    priqueue_offer(jobs, job); //job in queue
    return i;
  }
  else // all cores busy, if preemptive then switch jobs on core PSJF, PPRI
  {
    i = 0;
    if (t_scheme == PSJF)
    {
      while (i < c_cores->n_cores)
      {
        if (c_cores->c_job[i]->t_remaining > job->t_remaining)
        {
          //switch
          if (c_cores->c_job[i]->t_remaining > corejoblength) //max job length to switch
          {
            coretochange = i;
            corejoblength = c_cores->c_job[i]->t_remaining;
          }
        }
        i++;
      }
    }
    else if (t_scheme == PPRI)
    {
      while (i < c_cores->n_cores)
      {
        if (c_cores->c_job[i]->priority > job->priority)
        {
          //switch
          if (c_cores->c_job[i]->priority > corepriority) //max priority to switch
          {
            coretochange = i;
            corepriority = c_cores->c_job[i]->priority;
          }
          // test 3 doesn't follow its own rules to use lowest core, bases it on arrival time if same higher priority
          else if (c_cores->c_job[i]->priority == corepriority) //same max priority, later arrival is switched
          {
            if (c_cores->c_job[i]->t_arrival > c_cores->c_job[coretochange]->t_arrival) //same priority on higher core has later arrival time
            {
              coretochange = i;
            }
          }
        }
        i++;
      }
    }

    //preemptively slots to core over job
    if (coretochange != -1)
    {
      job->t_waiting = 0;
      job->t_started = c_time;
      job->t_updated = c_time;
      job->j_core = coretochange;

      c_cores->c_job[coretochange]->j_core = -1;
      c_cores->c_job[coretochange]->t_updated = c_time;

      //if this job was started this timestep but then overwritten by this new job, it didn't actually start
      if (c_cores->c_job[coretochange]->t_started == c_time)
      {
        c_cores->c_job[coretochange]->t_started = -1;
        c_cores->c_job[coretochange]->t_updated = -1;
      }

      //assign it new job
      c_cores->c_job[coretochange] = job;

      priqueue_offer(jobs, job);
      return coretochange;
    }
  }

  //no free cores and not preemptive
  priqueue_offer(jobs, job);
  return -1;
}

/**
  Called when a job has completed execution.
 
  The core_id, job_number and time parameters are provided for convenience. You may be able to calculate the values with your own data structure.
  If any job should be scheduled to run on the core free'd up by the
  finished job, return the job_number of the job that should be scheduled to
  run on core core_id.
 
  @param core_id the zero-based index of the core where the job was located.
  @param job_number a globally unique identification number of the job.
  @param time the current time of the simulator.
  @return job_number of the job that should be scheduled to run on core core_id
  @return -1 if core should remain idle.
 */
int scheduler_job_finished(int core_id, int job_number, int time)
{
  c_time = time;

  //increment time
  increment_timestep();

  //search through queue for job being finished
  int i = 0;
  job_t *job_finished;
  while (i < priqueue_size(jobs))
  {
    job_finished = priqueue_at(jobs, i);
    if (job_finished->job_id == job_number)
    {
      //if this job ended, remove from queue
      job_finished = priqueue_remove_at(jobs, i);
      break;
    }
    i++;
  }

  //end time
  job_finished->t_end = time;

  // stats
  t_turnaround += (float)job_finished->t_end - job_finished->t_arrival;
  t_wait += (float)job_finished->t_waiting;
  t_response += (float)job_finished->t_started - job_finished->t_arrival;

  free(job_finished);

  // empty core slot
  c_cores->c_job[core_id] = NULL;

  //check if empty job queue
  if (priqueue_size(jobs) == 0)
  {
    return -1;
  }

  //look at next job
  i = 0;
  job_t *next_job = NULL;
  while (i < priqueue_size(jobs))
  {
    next_job = priqueue_at(jobs, i);
    // job exists and isnt already on a core
    if (next_job != NULL && next_job->j_core == -1)
    {
      break;
    }
    i++;
  }

  //there is no job without a core
  if (i >= priqueue_size(jobs))
  {
    return -1;
  }

  // if there is a next job to assign to core
  if (next_job != NULL)
  {
    //never started, been waiting
    if (next_job->t_started == -1)
    {
      //assign start time, add that to wait
      next_job->t_started = c_time;
      next_job->t_updated = c_time;
      next_job->t_waiting = next_job->t_started - next_job->t_arrival;
    }
    //started then waited
    else
    {
      //add time waiting in queue after already started from pause point
      next_job->t_waiting += c_time - next_job->t_updated;
      next_job->t_updated = c_time;
    }

    next_job->j_core = core_id;
    c_cores->c_job[core_id] = next_job;
    return next_job->job_id;
  }
  else
  {
    return -1;
  }
}

/**
  When the scheme is set to RR, called when the quantum timer has expired
  on a core.
 
  If any job should be scheduled to run on the core free'd up by
  the quantum expiration, return the job_number of the job that should be
  scheduled to run on core core_id.

  @param core_id the zero-based index of the core where the quantum has expired.
  @param time the current time of the simulator. 
  @return job_number of the job that should be scheduled on core cord_id
  @return -1 if core should remain idle
 */
int scheduler_quantum_expired(int core_id, int time)
{

  c_time = time;

  increment_timestep();

  int length = priqueue_size(jobs);

  //search for job in queue that is on core
  int i = 0;
  job_t *active_job;
  while (i < length)
  {
    active_job = priqueue_at(jobs, i);
    //if this job is the one on the core, remove from queue
    if (active_job->job_id == c_cores->c_job[core_id]->job_id)
    {
      active_job = priqueue_remove_at(jobs, i);
      //job hasnt ended this quantum jump
      if (active_job->t_remaining != 0)
      {
        //put at back of queue
        active_job->j_core = -1;
        active_job->t_updated = c_time;
        priqueue_offer(jobs, active_job);
      }
      //job ended this jump
      else
      {
        active_job->t_end = c_time;

        //stats
        t_turnaround += (float)active_job->t_end - active_job->t_arrival;
        t_wait += (float)active_job->t_waiting;
        t_response += (float)active_job->t_started - active_job->t_arrival;

        c_cores->c_job[core_id] = NULL;
        free(active_job);
      }
      break;
    }
    i++;
  }

  //search for next job to put in core
  i = 0;
  job_t *next_job;
  while (i < priqueue_size(jobs))
  {
    next_job = priqueue_at(jobs, i);
    //if this job is not on a core, assign to this core
    if (next_job->j_core == -1)
    {
      //assign job to core that ended
      //never started, been waiting
      if (next_job->t_started == -1)
      {
        //assign start time, add that to wait
        next_job->t_started = c_time;
        next_job->t_updated = c_time;
        next_job->t_waiting = next_job->t_started - next_job->t_arrival;
      }
      //started then waited
      else
      {
        //add time waiting in queue after already started from pause point
        next_job->t_waiting += c_time - next_job->t_updated;
        next_job->t_updated = c_time;
      }

      next_job->j_core = core_id;
      c_cores->c_job[core_id] = next_job;
      return next_job->job_id;
    }
    i++;
  }

  return -1;
}

/**
  Returns the average waiting time of all jobs scheduled by your scheduler.

  Assumptions:
    - This function will only be called after all scheduling is complete (all jobs that have arrived will have finished and no new jobs will arrive).
  @return the average waiting time of all jobs scheduled.
 */
float scheduler_average_waiting_time()
{
  if (t_wait != 0.0)
  {
    return ((float)t_wait / n_jobs);
  }
  else
  {
    return 0.0;
  }
}

/**
  Returns the average turnaround time of all jobs scheduled by your scheduler.

  Assumptions:
    - This function will only be called after all scheduling is complete (all jobs that have arrived will have finished and no new jobs will arrive).
  @return the average turnaround time of all jobs scheduled.
 */
float scheduler_average_turnaround_time()
{
  if (t_turnaround != 0.0)
  {
    return ((float)t_turnaround / n_jobs);
  }
  else
  {
    return 0.0;
  }
}

/**
  Returns the average response time of all jobs scheduled by your scheduler.

  Assumptions:
    - This function will only be called after all scheduling is complete (all jobs that have arrived will have finished and no new jobs will arrive).
  @return the average response time of all jobs scheduled.
 */
float scheduler_average_response_time()
{
  if (t_response != 0.0)
  {
    return ((float)t_response / n_jobs);
  }
  else
  {
    return 0.0;
  }
}

/**
  Free any memory associated with your scheduler.
 
  Assumptions:
    - This function will be the last function called in your library.
*/
void scheduler_clean_up()
{
  priqueue_destroy(jobs);
  free(jobs);
  free(c_cores->c_job);
  free(c_cores);
}

/**
  This function may print out any debugging information you choose. This
  function will be called by the simulator after every call the simulator
  makes to your scheduler.
  In our provided output, we have implemented this function to list the jobs in the order they are to be scheduled. Furthermore, we have also listed the current state of the job (either running on a given core or idle). For example, if we have a non-preemptive algorithm and job(id=4) has began running, job(id=2) arrives with a higher priority, and job(id=1) arrives with a lower priority, the output in our sample output will be:

    2(-1) 4(0) 1(-1)  
  
  This function is not required and will not be graded. You may leave it
  blank if you do not find it useful.
 */
void scheduler_show_queue()
{
  int length = priqueue_size(jobs);
  int i = 0;
  job_t *temp;
  while (i < length)
  {
    temp = priqueue_at(jobs, i);
    printf("%d PRIORITY: %d REMAINING: %d CORE: %d |", temp->job_id, temp->priority, temp->t_remaining, temp->j_core); //temp->priority);
    i++;
  }
}

int comparer(const void *p1, const void *p2)
{ //p1 = new node to insert p2 = node in queue
  job_t *job_1 = (job_t *)p1;
  job_t *job_2 = (job_t *)p2;
  if (job_2 == NULL)
  {
    return -1;
  }
  switch (t_scheme)
  {
  case FCFS:
    return 1;
    break;
  case SJF:
    if (job_1->t_total < job_2->t_remaining && job_2->t_started == -1)
    {
      return -1;
    }
    else
    {
      return 1;
    }
    break;
  case PSJF:
    if (job_1->t_remaining < job_2->t_remaining)
    {
      return -1;
    }
    else
    {
      return 1;
    }
    break;
  case PRI:
    if (job_1->priority < job_2->priority && job_2->t_started == -1)
    {
      return -1;
    }
    else
    {
      return 1;
    }
    break;
  case PPRI:
    if (job_1->priority < job_2->priority)
    {
      return -1;
    }
    else
    {
      return 1;
    }
    break;
  case RR:
    return 1;
    break;
  default:
    return 1;
    break;
  }
}

void increment_timestep()
{
  int i = 0;
  for (i = 0; i < c_cores->n_cores; i++)
  {
    // get job on core
    job_t *running_job = c_cores->c_job[i];
    // core has job running
    if (running_job != NULL)
    {
      //if job hasnt started and wasn't updated this timestep FCFS, SJF, PRI
      if ((running_job->t_started == -1) && (running_job->t_updated != c_time))
      {
        running_job->t_started = running_job->t_updated;
        // t_response += (float)(running_job->t_started - running_job->t_arrival);
      }
      else
      { // else job was paused or already started and restarted PSJF, PPRI
        running_job->t_remaining -= c_time - running_job->t_updated;
        running_job->t_updated = c_time;
      }
    }
  }
}