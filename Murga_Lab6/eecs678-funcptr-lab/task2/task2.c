#include <stdio.h>
#include <stdlib.h>

/* IMPLEMENT ME: Declare your functions here */
int add (int a, int b);
int subtract (int a, int b);
int multiply (int a, int b);
int divide (int a, int b);

typedef int (*function) (int a, int b);

int main (void)
{
	/* IMPLEMENT ME: Insert your algorithm here */
	printf("Operand 'a' : 6 | Operand 'b' : 3\n");
	printf("Specify the operation to perform (0 : add | 1 : subtract | 2 : Multiply | 3 : divide): ");

	int input;
	scanf("%d", &input);

	function f_pointer[] = {add, subtract, multiply, divide}; // function pointer array
	
	int output = f_pointer[input] (6, 3);
	printf("x = %d\n", output);

	fflush(stdout);

	return 0;
}

/* IMPLEMENT ME: Define your functions here */
int add (int a, int b) { printf ("Adding 'a' and 'b'\n"); return a + b; }
int subtract (int a, int b) { printf ("Subtracting 'b' from 'a'\n"); return a - b; }
int multiply (int a, int b) { printf ("Multiplying 'a' and 'b'\n"); return a * b; }
int divide (int a, int b) { printf ("Dividing 'a' by 'b'\n"); return a / b; }
