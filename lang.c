
u32[42] fred = 32;

u8 bar = '\'';
u8 *bar = "a\\  \"bcd";
u8 bar = 0xDEAD;

// Variables can only be initialized to literals.
//u8 bar = bar;

struct Foo {
	u8 foo;
	u32 bar;
}

Foo bob;

/*
 * Add things.
 * Return something.
 */
fn add(u8 a, u8 *b) u8 {
	b[2] = 42;
	return a + *b;
}

// Bob
fn main() u32 {
	Foo foo;

	foo.foo = 0x42; // The answer.
	foo.bar = 0xDEADBEEF00;

	a = 0x10 | foo.foo;

	u8 a = 3;
	//u8 sum = add(foo.foo, a);
	u8 sum;
	sum = add(foo.foo, a);

	if (a || foo.bar) {
		return 32;
	}

	return 0;
}

