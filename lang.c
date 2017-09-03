
u32[42] fred = 32;

u8 bar = '\'';
u8 *bar = "a\\  \"bcd";
u8 bar = 0xDEAD;

struct Foo {
	u8 foo;
	u32 bar;
}

Foo bob;

/*
 * Add things.
 * Return something.
 */
func add(u8 a, u8 b) u8 {
	return a + b;
}

// Bob
func main() u32 {
	Foo foo;

	foo.foo = 0x42; // The answer.
	foo.bar = 0xDEADBEEF00;

	u8 a = 3;
	u8 sum = add(foo.foo, a);

	return 0;
}

