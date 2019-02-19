#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NB_TASKS (31)

#define EXT4 (0x45585434)
/* #define TMDA (0x544D4441)
#define CPDH (0x43504448)
#define RTMA (0x52544D41)
#define TCHA (0x54434841)
#define SYCY (0x53594359)
#define RTCH (0x52544348)
#define TMRO (0x544D524F)
#define SGMH (0x53474D48)
#define TMPR (0x544D5052)
#define BU1H (0x42553148)
#define BU2H (0x42553248)
#define PFMY (0x50464D59)
#define AOSC (0x414F5343)
#define SSRA (0x53535241)
#define RADY (0x52414459)
#define INIT (0x494E4954)
#define FMSA (0x464D5341)
#define FTMA (0x46544D41)
#define TTRA (0x54545241)
#define FILA (0x46494C41)
#define MEMD (0x4D454D44)
#define CELA (0x43454C41)
#define PLMY (0x504C4D59) */

typedef enum TaskEvent
{
  TASK_SWITCH =1,
  IT_BEGIN = 2,
  IT_END = 3,
  USER_TRACE = 4
} TaskEvent;

typedef struct TaskStat TaskStat;

struct TaskStat
{
  char * name;
  unsigned int id;
  unsigned int nbCyclesExec;
  unsigned int isActive;
  double maxDuration;
  double minDuration;
  double avgDuration;
  double taskSwitchTime;
  double duration;
};

TaskStat taskStats[NB_TASKS];

const char * taskNames[NB_TASKS] = {"RTCH","BU1H","BU2H","RADY","CELA","SYCY","AOSC","PLMY",
                                    "PFMY","OBCP","CPDH","TMRO","TMDA","TMPR","MTLA","FTMA",
				                    "SGMH","MEMD","CTXC","FMSA","IDLE","INIT","RTMA","TCHA",
				                    "SIFM","FILA","DEVT","SSTX","SSRA","SSRB","SPRA"};
unsigned int end_of_file = 0;
unsigned int timetag = 0;
unsigned int isMeasurementStarted = 0;

unsigned int readInt();

unsigned int name2id(const char * name)
{
  unsigned int result = 0;
  unsigned int i = 0;
  
  for (i=0; i<NB_TASKS; i++)
  {
    if (memcmp(name, taskNames[i], strlen(name))==0)
    {
      result = ((name[0])<<24) + ((name[1])<<16) + ((name[2])<<8) + (name[3]);
      break;
    }
  }

  return result;
}

void init()
{
  unsigned int i = 0;

  for (i=0; i<NB_TASKS; i++)
  {
    taskStats[i].name = taskNames[i];
    taskStats[i].id = name2id(taskStats[i].name);
    taskStats[i].nbCyclesExec = 0;
    taskStats[i].maxDuration = 0.0;
    taskStats[i].minDuration = 125.0;
    taskStats[i].avgDuration = 0.0;
    taskStats[i].taskSwitchTime = 0.0;
    taskStats[i].duration = 0.0;
    taskStats[i].isActive = 0;
  }
}

void parse_args(int argc, char * argv[])
{
   if ((argc!=2) && (argc!=3)) 
   {
     printf("Usage: taskname");
     exit(1);
   }
   
   if (memcmp(argv[1],"-t",2)==0)
   {
     timetag = atoi(argv[2]);
     printf("Timetag : %d\n", timetag);
   }
}

void update_stats(unsigned int task, double time)
{
  unsigned int i = 0;

  if (isMeasurementStarted)
  {
    for (i=0; i< NB_TASKS; i++)
    {
        if (taskStats[i].isActive)
        {
          taskStats[i].isActive = 0;
          taskStats[i].duration = taskStats[i].duration + time - taskStats[i].taskSwitchTime;
		  if (taskStats[i].id == 0x50464d59)
		  {
		    printf("PFMY END time %f duration %f\n", time, taskStats[i].duration);
		  }
        } 
		else if (taskStats[i].id == task)
		{
		  taskStats[i].isActive = 1;
	      taskStats[i].taskSwitchTime = time;
		  if (task == 0x50464d59)
		  {
		    printf("PFMY START time %f duration %f\n", time, taskStats[i].duration);
		  }
		}
	    //if (taskStats[i].id == name2id("AOSC")) printf("++++ %f\n", time);
	}

  }
}

void end_of_frame(double time)
{
  unsigned int i = 0;

  if (1)
  {
    for (i=0; i< NB_TASKS; i++)
    {
      if (taskStats[i].isActive)
      {
        taskStats[i].isActive = 0;
        taskStats[i].duration = taskStats[i].duration + time - taskStats[i].taskSwitchTime;
      }
      if (taskStats[i].duration > taskStats[i].maxDuration) taskStats[i].maxDuration = taskStats[i].duration;
      if ((taskStats[i].duration < taskStats[i].minDuration) && (taskStats[i].duration > 1e-6)) taskStats[i].minDuration = taskStats[i].duration;
	  taskStats[i].isActive = 0;
      taskStats[i].duration = 0;
	  taskStats[i].taskSwitchTime = 0;
    }
  }
}

void correct_for_interrupt(double time, double interruptTime)
{
   unsigned int i = 0;
   
   if (isMeasurementStarted)
   {
     for (i=0; i< NB_TASKS; i++)
     {
       if (taskStats[i].isActive)
       {
         taskStats[i].duration = taskStats[i].duration - time + interruptTime;
       }
     }
   }
}

void report()
{
   unsigned int i = 0;
   printf("\nName Max. Dur. Min. Dur.\n");
   for (i=0; i<NB_TASKS; i++)
   {
      printf("%s %f %f\n", taskStats[i].name, taskStats[i].maxDuration, taskStats[i].minDuration);
   }
}
void main(int argc, char * argv[])
{
  unsigned int coarse = 0;
  unsigned int fine = 0;
  unsigned int type = 0;
  unsigned int value = 0;
  unsigned int task = 0;
  int i = 0;
  unsigned int isTaskActive = 0;
  double lastTaskSwitchTime = 0.0 , taskSwitchTime = 0.0, measurementTime = 0.0;
  double duration = 0.0;
  double maxTaskDuration = 0.0;
  double averageTaskDuration = 0.0;
  double interruptTime = 0.0;
  double interruptDUration = 0.0;
  unsigned int nbCycles = 0;
  unsigned int nbCyclesExec = 0;
  char taskName[5];
  unsigned int task_id = 0;
  unsigned long long int length = 0;
  unsigned long long int nbRecords = 0;

  parse_args(argc, argv);
  init();

  task_id = ((*argv[1])<<24) + (*(argv[1]+1)<<16) + (*(argv[1]+2)<<8) + *(argv[1]+3);
  printf("Task id = %x\n", task_id);
  //printf("C1= %x\n",*argv[1]);
  //printf("C2= %x\n",*(argv[1]+1));
  //printf("C3= %x\n",*(argv[1]+2));
  //printf("C4= %x\n",*(argv[1]+3));
  FILE* file = fopen("./ioprintf.txt_2.tsk","rb");
  if (file != NULL)
  {
    fseek(file, 0, SEEK_END); 
    length=ftell(file); 
    fseek(file, 0 , SEEK_SET); 
    nbRecords = length / 12;

    while (!end_of_file)
    {
      coarse = readInt(file);
      fine = readInt(file);
      type = readInt(file);
	    
      task = readInt(file);
	  taskName[0] = ((task & 0xFF000000) >> 24);
	  taskName[1] = ((task & 0x00FF0000) >> 16);
	  taskName[2] = ((task & 0x0000FF00) >> 8);
	  taskName[3] = (task & 0x000000FF);
	  taskName[4] = 0;

	  measurementTime = coarse + fine*0.000000001;

      if (type == USER_TRACE) 
	  {
	    fread(&value, sizeof(int), 1, file);
      }
      if (type == TASK_SWITCH) 
	  {
	    update_stats(task, measurementTime);
	  }
	  if (type == IT_BEGIN)
	  {
	    interruptTime = coarse + fine*0.000000001;
        if (task == EXT4)
        {
		  if (interruptTime>3.0) isMeasurementStarted = 1;
		  end_of_frame(interruptTime);
		}
	  }
	  if (type == IT_END)
	  {
	    if (task == EXT4)
	    {
	       nbCycles++;
	    }
	    else
	    {
		  correct_for_interrupt(measurementTime, interruptTime);
	    }

	  }
	     //printf("Task %s %d %f %d\n", &taskName[0] , task, coarse + fine*0.000000001, type);
    }
          
    fclose(file);
    printf("Total nbCycles: %d\n", nbCycles);
    printf("Total nbCycles executed: %d\n", nbCyclesExec);
    printf("Length of file in bytes: %lld\n", length);
    printf("Nb records: %lld\n", length/12);
    printf("%s Max task duration: %f\n", argv[1], maxTaskDuration);
    printf("%s Average task duration per cycle: %f\n", argv[1], averageTaskDuration/nbCyclesExec);
  }

   report();
}

unsigned int readInt(FILE* my_file)
{
    unsigned int int_read = 0;
    unsigned int i = 0;
    unsigned int int_returned = 0;

    if (fread(&int_read, sizeof(unsigned int), 1, my_file)==0)
    {
       end_of_file = 1;
    }
    
    int_returned = ((int_read & 0xFF000000) >> 24) + ((int_read & 0x00FF0000) >> 8) +
	           ((int_read & 0x0000FF00) << 8) + ((int_read & 0x000000FF) << 24);
//	printf("%08x %08x\n", int_read, int_returned);
    return int_returned;
}

