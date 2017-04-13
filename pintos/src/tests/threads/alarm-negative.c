/* Tests timer_sleep(-100).  Only requirement is that it not crash. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

void
test_alarm_negative (void) 
{
  // timer_sleep (-100);
  int* a = (int*)malloc(1000);
  printf("a = %d\n", *a);
  *a = 7482;
  printMemory();
  int* b = (int*)malloc(8000);
  printf("b = %d\n", *b);
  *b=8846;
  printMemory();
  int* c = (int*)malloc(700);
  printf("c = %d\n", *c);
  *c=8846;
  printMemory();
  printf("a = %d\n", *a);
  printf("b = %d\n", *b);
  printf("c = %d\n", *c);
  printf("..................FREE A .......................\n");
  // printf("a = %d\n", *a);
  free(a);
  printMemory();
  printf("..................FREE C .......................\n");
  free(c);
  printMemory();
  printf("..................FREE B .......................\n");
  free(b);
  printMemory();
  int **Y=(int **)malloc(5*sizeof(int *));
  int i,j;
  for(i=0;i<5;++i)
  {
  	Y[i]=(int *)malloc(sizeof(int *)*5);
  }
  for(i=0;i<5;i++)
  {
  	for(j=0;j<5;++j)
  	{
  		Y[i][j]=i+j*6;
  		printf("%d ",Y[i][j]);
  	}
  	printf("\n");
  }
  printMemory();
  for(i=0;i<5;++i)
  {
  	free(Y[i]);
  }
  printMemory();
  free(Y);
  printMemory();

  int *arr;

  arr = (int *)malloc(sizeof(int));

  arr[0] = 5;

  for(int i = 0;i < 1;i++)printf("%d \n", arr[i]);

  arr = (int *)realloc(arr, 4*sizeof(int));

  for(i = 0;i < 4;i++)arr[i] = i*i;

  for(i = 0;i < 4;i++)printf("%d \n",arr[i]);

  arr = (int *)realloc(arr, 2*sizeof(int));
  
  for(i = 0;i < 4;i++)printf("%d \n",arr[i]);

  free(arr);

  pass ();
}