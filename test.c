// Declare struct
struct bob {
	u32 a;
	u32 c;
}

// Define function
func inc(u32 *a) u32
{
	return (*a + 1);
}

func main() u32
{
	u32 *a;      // Define pointer
	u32 b = (1); // Define and initialize
	u32 c[2];    // Define array
	struct bob o; // Define struct

	o.a = 0x42;
	if (o.a == 2) {
		o.c = o.a * 3;
	}

	*a = inc(&b); // Expression with assignment and function call
	*(c + 1) = b;
	c[2 - 2] = (b + 1) * 2;

	return 0;
}
