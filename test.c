// Declare struct
struct bob {
	u32 a;
	u32 c;
}

// Define function
u32 inc(int *a)
{
	return (*a + 1);
}

u32 main()
{
	int *a;      // Define pointer
	int b = (1); // Define and initialize
	int c[2];    // Define array
	struct bob o; // Define struct

	o.a = 2;
	if (o.a == 2) {
		o.c = o.a * 3;
	}

	*a = inc(&b); // Expression with assignment and function call
	*(c + 1) = b;
	c[2 - 2] = (b + 1) * 2;

	return 0;
}
