# White noise / random numbers

Synthesizing white noise is easy enough: just output a sequence of random numbers. The problem is that making random numbers on the computer can be tricky.

A random number generator isn't only used for white noise. It can also be used for other purposes, such as:

- making colored noise, by applying a filter to white noise
- sample-and-hold LFO
- starting voices with a random phase
- playing random notes in an arpeggiator
- positioning a note randomly in the stereo field
- randomizing the plug-in parameters
- as a modulation source

## Pseudorandom number generators

For realtime audio processing, we want a pseudorandom number generator (PRNG) with the following properties:

- produces a uniform distribution: all numbers should appear equally often
- has a long sequence length before it repeats
- each value in the sequence should be independent of the others
- the sequence should not be too predictable
- is fast
- runs in constant time
- does not use global variables or shared state
- does not block the thread
- can be given an initial seed, for deterministic results
- is cross-platform, works the same way on each platform
- outputs numbers in the range [-1, 1] or [0, 1] or [0, 1)

There is a trade-off between speed and quality. For our purposes, we don't need the random numbers to be of the highest possible quality — they just need to be good enough to add some noise into an audio signal. Just be aware that the random number generators shown here are not suitable for tasks where quality does matter, such as cryptography or running Monte Carlo simulations.

A note on sequence length: Let's say the PRNG outputs 24-bit integers and has a sequence length of $2^{24}$ steps before it repeats. With a sample rate of 48 kHz, that is about 6 minutes worth of audio, certainly long enough that you won't hear the period of this signal. A 15-bit PRNG, however, has a sequence length of less than one second and this period will become noticeable to the listener.

## Random number generators from the standard library

The standard library PRNGs are useful in a pinch but are best kept out of the audio thread.

Problems with these random generators:

- They do not always provide very good numbers. This is especially the case for `std::rand`, avoid it!

- POSIX systems such as macOS and Linux have `rand48` and `random` that give reasonable results for audio purposes. Unfortunately, these functions do not exist on Windows.

- The PRNG may be susceptible to data races because it uses global variables, although the `_r` versions of such functions are safe to use.

- Depending on the implementation, the PRNG may use a lock. `arc4random` uses a spinlock to sync between threads, which by itself would probably be fine. However, every so often it will get re-seeded with truly random data by reading from `/dev/random`, which involves potentially blocking system calls.

- Certain PRNGs such as `arc4random` are designed for cryptographic purposes and are overkill for audio work. You cannot seed these functions manually.

- The standard library functions may not have exactly what we're looking for. Maybe we want numbers in a closed range `[a, b]` and the library function gives us a half-open range `[a, b)` or vice versa.

- The C++ `<random>` library only guarantees amortized constant time complexity for its PRNGs, meaning they run fast most of the time but occasionally take a lot longer. In particular, this is true for the Mersenne twister. That's not good enough for realtime audio code where each block is on a strict deadline.

The standard library often only defines the interface of the function, not how it must be implemented. Therefore, you don't know exactly what happens inside the PRNG. This may also vary by platform. Since we want to be 100% in control over what happens on the audio thread, it's better to roll your own random number generator.

If you're using JUCE, the `juce::Random` class is perfectly acceptable. It's based on the same algorithm as `rand48`, so it's fast and produces good enough numbers for most audio tasks. You might want to avoid `nextFloat`, which has a slightly distorted distribution (more about this issue below), but `nextDouble` is fine.

References:

- [Using the C++ Standard Library for Real-time Audio - Timur Doumler - ADC21](https://youtu.be/vn7563IAQ_E?t=2237)
- [arc4random implementation as used by Apple](https://opensource.apple.com/source/Libc/Libc-1044.1.2/gen/FreeBSD/arc4random.c)

## Linear congruential generator (LCG)

This is one of the oldest methods for making random numbers. It's what I'd recommend for real-time audio use on a modern 64-bit CPU where multiplication is fast.

The formula for the linear congruential algorithm is:

```c++
random = (A * seed + C) mod M
```

Here, A is known as the multiplier term, C is the increment term, and M is the modulus term. These are fixed constants.

The values of A, C, and M are carefully chosen to produce a good sequence of numbers that doesn't repeat for a long time.

By using unsigned arithmetic and setting M to be a power of two, typically $2^{32}$ or $2^{64}$, we get the "modulo M" part of the formula for free using numeric overflow. The formula then simplifies to:

```c++
random = A * seed + C
```

For any word size (number of bits), it's possible to choose values for A and C that produce a sequence that only repeats after M steps, regardless of the initial seed.

Since the math is so simple, this makes for a pseudorandom number generator that is fast and also easy to implement. The LCG is obsolete as a random generator for "serious" purposes, but for our goal of making white noise this algorithm still works fine.

References: [Wikipedia](https://en.wikipedia.org/wiki/Linear_congruential_generator)

### 32-bit version

The following function calculates a pseudorandom unsigned 32-bit integer using the linear congruential method:

```c++
uint32_t randomGenerator(uint32_t seed)
{
    seed = seed * 196314165 + 907633515;
    return seed;
}
```

The values for A and C used here were taken from [Musical Applications of Microprocessors by Hal Chamberlin](http://sites.music.columbia.edu/cmc/courses/g6610/fall2016/week8/Musical_Applications_of_Microprocessors-Charmberlin.pdf), page 533.

The modulus M is $2^{32}$, so that numeric overflow automatically handles the modulo operation.

The length of the generated sequence is the same as the modulus, $2^{32}$ = 4294967296 elements. In other words, the random generator steps through all possible 32-bit integers before it repeats.

The `seed` is the previously generated random number. For the very first number, you manually set the seed; different seeds make different random sequences.

```c++
uint32_t randomValue = 22222;  // pick a seed
```

To calculate a new random value, call `randomGenerator` and pass in the previous output or the initial seed:

```c++
randomValue = randomGenerator(randomValue);
```

The output of this particular random generator is a number between 0 and 4294967295 (inclusive).

### Using a struct

Because the LCG needs to manage state (the current seed), it might be useful to implement the random number generator as a struct rather than a free function:

```c++
struct RandomGenerator
{
    RandomGenerator(uint32_t seed = 22222): seed(seed) { }

    uint32_t operator()()
    {
        seed = seed * 196314165 + 907633515;
        return seed;
    }

private:
    uint32_t seed;
};
```

Use it as follows:

```c++
RandomGenerator rng { 12345 };

auto randomValue = rng();
```

### 48-bit version

The POSIX `rand48` family of algorithms are also linear congruential generators. They work on unsigned integers that are 48 bits long (i.e. 6 bytes).

A = 25214903917 and C = 11. The modulus M is $2^{48}$, which is equivalent to masking by `0xFFFFFFFFFFFF`. The default initial seed is 20017429951246, but of course you can pick any seed.

```c++
struct RandomGenerator
{
    RandomGenerator() { }
    RandomGenerator(uint64_t seed): seed(seed) { }

    uint32_t operator()()
    {
        seed = (seed * 25214903917 + 11) & 0xFFFFFFFFFFFF;
        return (seed >> 17) & 0x7FFFFFFF;
    }

private:
    uint64_t seed = 20017429951246;
};
```

Even though this `RandomGenerator` uses 48 bits internally, it returns a `uint32_t` containing only the highest 31 bits. The generated random numbers are therefore in the range $[0, 2^{31}-1]$, so between 0 and 2147483647 (inclusive). You can safely convert this to a signed int.

The reason for returning fewer bits is that the higher bits give statistically better output values, since they have longer cycles than the lower bits. This especially matters when doing `randomValue % upperBound` to limit the range of the generated numbers. (The low bit of the LCG alternates between 0 and 1 on consecutive calls, hardly random. Calls to the random generator therefore are not completely independent: if the current result is even, you know the next one will be odd.)

`juce::Random` uses this exact same LCG, except they return the highest 32 bits (shift by 16 instead of 17) and cast the result to a signed `int`. This produces numbers in the range $[-2^{31}, 2^{31}-1]$ or [-2147483648, 2147483647].

### 64-bit version

64-bit version with parameters found by Donald Knuth:

```c++
uint64_t randomGenerator(uint64_t seed)
{
    return seed = seed * 6364136223846793005 + 1442695040888963407;
}
```

The generated random numbers are in the range $[0, 2^{64} - 1]$.

You can also combine two 32-bit random numbers to make a 64-bit random number. Assuming that `rng()` is a PRNG that returns a 32-bit unsigned integer:

```c++
uint64_t randomValue = (uint64_t(rng()) << 32) | uint64_t(rng());
```

This method is simple but not entirely without problems. For example, since an LCG always alternates between even and odd numbers, if you generate a bunch of these 64-bit numbers in succession without re-seeding the PRNG, they will always be even or always be odd.

### Using the C++ standard library

C++ has a `std::linear_congruential_engine` class. Unlike the other standard library PRNGs, this is safe in realtime audio code.

Use it as follows:

```c++
#include <random>

std::linear_congruential_engine<std::uint_fast32_t, 196314165, 907633515, 0> rng { 22222 };

auto randomValue = rng();
```

The template parameters are:

- the datatype, here `std::uint_fast32_t`
- the multiplier A
- the increment C
- the modulus M

In the above example, the M term is 0 because the chosen A and C are for a sequence of $2^{32}$ elements. This random generator is exactly the same as the 32-bit LCG implemented above. It returns values from 0 – 4294967295 (inclusive).

The value passed into the `linear_congruential_engine` constructor is the initial seed.

The C++ standard library provides `minstd_rand` as a predefined linear congruential generator you can use out-of-the-box:

```c++
std::minstd_rand rng { 12345 };

auto randomValue = rng();
```

This returns `std::uint_fast32_t` numbers in the range 1 – 2147483646 (inclusive). You use `rng.min()` and `rng.max()` to determine what the valid output range is.

## Linear feedback shift register (LFSR)

The LFSR is an old school random generator. It's fast because it only uses bit shifts and XOR operations.

However, LFSRs don't actually create pure white noise but something that sounds slightly darker. It sits somewhere between white noise and pink noise. Read the article about [LFSR noise](lfsr-noise.markdown).

To make white noise using an LFSR, you'd need to run the LFSR several times per output sample, for example 5×. This makes sure successive outputs are no longer correlated. Of course, these extra computations make it less appealing to use the LFSR as a fast white noise generator.

The Xorshift algorithm described below is actually a type of LFSR.

## Xorshift

This random generator gives very good numbers and is extremely simple. The algorithm consists of several steps where the seed value is XOR'ed with a shifted version of itself. The seed can be any number except zero.

```c++
struct RandomGenerator
{
    RandomGenerator() { }
    RandomGenerator(uint64_t seed): seed(seed) { }

    uint64_t operator()()
    {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        return seed;
    }

private:
    uint64_t seed = 161803398;
};
```

Xorshift is a kind of LFSR but it creates better quality random numbers than a regular LFSR or the LCG method. Howvever, I found that it also runs slower. Not sure why as you'd think some shifts and XORs would be faster than a multiplication... At least it produces pure white noise, which wasn't the case for the LFSR.

A variation is xorshift* where the output is "scrambled" by multiplying it by some big number. This gives improved results that pass most (but not all) statistical tests for the quality of random number generators.

```c++
struct RandomGenerator
{
    RandomGenerator() { }
    RandomGenerator(uint64_t seed): seed(seed) { }

    uint64_t operator()()
    {
        seed ^= seed >> 12;
        seed ^= seed << 25;
        seed ^= seed >> 27;
        return seed * 0x2545F4914F6CDD1D;
    }

private:
    uint64_t seed = 161803398;
};
```

There are several other variants of xorshift too, such as xorshift+, that aim to give even better quality numbers. Many programming languages have a random generator based on xorshift — just not C or C++.

I wouldn't use xorshift to create white noise, but you might use one of the algorithms from the xorshift family for other random number generation tasks in your audio code. If you do, make sure to keep only the highest bits of the result, since the lowest bits can have weaknesses.

References: [Wikipedia](https://en.wikipedia.org/wiki/Xorshift)

## PCG (permuted congruential generator)

PCG is a relatively new but high-quality random generator. It combines ideas from LCG and xorshift to generate a very strong random sequence.

This is a good choice for a default "go to" PRNG. In fact, I found it was significantly faster than xorshift and only a tiny bit slower than an LCG, so it's perfectly suitable for making white noise too.

References: [pcg-random.org](https://www.pcg-random.org) (has source code)

## Generating floating-point numbers

Most pseudorandom number generators will output integer values, typically unsigned integers. However, for sound synthesis we prefer to have a floating-point value in a range such as [0, 1] or [-1, 1].

If the PRNG outputs 32-bit unsigned integers, they will go from 0 up to and including $2^{32} - 1$ = 4294967295. It's easy to turn this into a number between 0 and 1 by dividing by the maximum value:

```c++
double randomFloat = double(randomInt) / double(4294967296);
```

You can divide either by 4294967295, the largest value a 32-bit integer can have, or by 4294967296, one beyond the maximum.

A good reason for dividing by 4294967296 is that this is a power of two, which has the following benefits:

1. The compiler can automatically turn this division into a multiplication instead, which is faster.

2. It gives nicer numbers because floating-point numbers are based on powers of two. The random number can now be exactly 0.5, for example. The distance between consecutive floating-point numbers is always the same.

When dividing by 4294967296, the resulting numbers lie within in the half-open range [0, 1). No random number will never be exactly 1.0.

To use the closed range [0, 1] instead of [0, 1), divide by 4294967295. However, when dividing by a number that is not a power-of-two, the resulting floating-point values will not be evenly spread out across the full [0, 1] range. It's only a tiny difference, but some of the numbers will be closer to each other than others. You can still consider the distribution to be uniform.

To get a floating-point value between [-1, 1) the division becomes:

```c++
double randomFloat = double(randomInt) * (2.0 / 4294967296) - 1.0;
```

Notice that here too the upper bound is exclusive. While in theory a closed range [-1, 1] would be better, for making noise this is not important at all. Technically, it creates a DC offset but it's too small to be perceptible.

Again, use 4294967295 if you need the range to be [-1, 1] but realize the numbers won't be as nicely spaced out — you can't get exactly 0.0 this way, for example.

Naturally, the divisor depends on the maximum possible value produced by the PRNG. For a 64-bit PRNG it will be 18446744073709551616. However, we also have to consider the limitations of floating point, especially what happens with precision...

### Precision issues

A `double` has 53 bits of precision. A `float` has 24 bits of precision. The PRNG will typically output 32-bit or 64-bit numbers. Clearly, these numbers do not match.

The following two situations are possible:

#### 1) The floating-point data type has more bits of precision than the integers produced by the PRNG.

Example: `double` and a 32-bit PRNG, as shown in the previous section. There are only 4294967296 unsigned 32-bit integers but the [0, 1] range contains many more double-precision values than that!

The distribution is still uniform but there is a larger "gap" between each floating-point random number than is necessary. The low bits of the mantissa are always set to zero. In fact, only one out of every 2097152 possible double values will be used by this random number generator.

It would be better to use a PRNG that returns more bits (at least 53), so that the maximum number of possible `double` values between 0.0 and 1.0 can be generated.

#### 2) The floating-point data type has fewer bits of precision than the PRNG.

Example: `float` and a 32-bit PRNG, or `double` and a 64-bit PRNG.

Let's say the PRNG returns 32-bit unsigned integers, then to get a floating-point number in the interval [0, 1] we might do:

```c++
float randomFloat = float(randomInt) / float(4294967296);
```

Because a single-precision `float` only has 24 bits of precision, `float(randomInt)` will round off the 32-bit unsigned integer before making it a float, effectively causing the lowest bits of the integer to be set to 0. For example, the integer 794127851 becomes 794127872 as a float. Hexadecimal this was 0x2F556DEB but it rounds up to 0x2F556E00.

In other words, the PRNG can return 4294967296 unique numbers, but the [0, 1] range cannot describe all of these as single-precision floating-point numbers.

<!--
Dividing by 4294967295 or by 4294967296 are literally the same thing now because 4294967295 doesn't exist as a `float`. The float can accurately represent 4294967296 because it's a power of two, but all other neighboring numbers will round up or down to 4294967296 as well. (When using a double, dividing by 4294967296 gave numbers in the half-open range [0, 1) but here it always gives the closed range [0, 1].)
-->

The numbers 0 through 16777215 will fit into the `float` data type without problem, but anything larger will get rounded off because there is not enough precision to represent the exact value — and this rounding becomes more extreme the higher you go. That means the distribution is no longer technically uniform!

The closer you get to 1.0, the fewer unique random numbers there are, and so these numbers have a higher chance of being drawn. Between 0.5 and 1.0, the numbers are 256× more sparse than near 0.0, but each of these also has a 256× higher probability. This may sound like a lot but keep in mind there are over 16.7 million possible random numbers and so this isn't something you'd ever notice. Still, this isn't the most optimal way to distribute random numbers on the float-point number line.

One way to make this behave a bit better is to use only the top 24 bits of the result, as these are the most random anyway (especially in the case of an LCG):

```c++
float randomFloat = float(randomInt >> 8) / float(16777216);
```

This now divides by $2^{24} - 1$, the largest integer value that fits inside 24 bits. Every random integer is placed into a bin that is 256 numbers wide, and there will be exactly one floating-point value for each bin, meaning the distribution remains uniform.

### Recommended method

This is what I use:

```c++
struct RandomGenerator
{
    RandomGenerator() { }
    RandomGenerator(uint64_t seed): seed(seed) { }

    double operator()()
    {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        return double((seed >> 11) & 0x1FFFFFFFFFFFFFULL) / 9007199254740992.0;
    }

private:
    uint64_t seed = 161803398ULL;
};
```

It's a 64-bit LCG that keeps only the 53 most-significant bits and converts to double-precision in the half-open range [0, 1). This is fast and behaves well.

A shortcoming of the linear congruential method (and many other PRNGs) is that the low bits tend to not be very random. By using the low bits, the resulting distribution may not be 100% uniform. This is why we use the most-significant bits by shifting off the lowest bits.

To make numbers in the range [-1, 1) you can simply do `rng() * 2.0 - 1.0`. However, to get the best possible precision do the following:

```c++
double operator()()
{
    seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
    int64_t temp = int64_t((seed >> 10) & 0x3FFFFFFFFFFFFFULL) - 9007199254740992LL;
    return double(temp) / double(9007199254740992);
}
```

This turns the integer into a 54-bit number, which is too large to fit into a `double`. However, by making it a signed integer it will fit again, as the double can use the sign bit to hold the extra bit. This gives us one more bit of precision even though the range has twice as many elements in it now.

### Using the C++ standard library

When using a random generator from the C++ standard library, such as `std::linear_congruential_engine`, you can use `std::uniform_real_distribution` to convert the integer result into a floating-point value in the interval [0, 1) or any other interval `[a, b)`.

```c++
#include <random>

// create the random number generator
std::minstd_rand rng { std::random_device{}() };

// create a uniform distribution between [0, 1)
std::uniform_real_distribution<double> distr { 0.0, 1.0 };

// generate a random number from the distribution
auto randomValue = distr(rng);
```

This returns a `double` between 0.0 and 1.0 (exclusive).

However, `uniform_real_distribution` has two drawbacks for realtime audio work:

- The algorithm for producing the distribution is implementation-defined, meaning you may get different results on different platforms.

- It is not guaranteed to run in constant time. In order to ensure the distribution is truly uniform, the algorithm may sometimes need to throw away the generated random number and generate a new one, and repeat this an arbitrary number of times. (See also modulo bias below.)

In other words, a distribution can be constant time or it can be uniform, but it can't be both at the same time. Usually this won't be an issue, but under unfortunate circumstances, it may take a relatively long time for the algorithm to find a suitable number. In audio code, we'd rather be constant time.

Instead of using `uniform_real_distribution`, you can convert the output from a C++ standard library PRNG to a floating-point number yourself:

```c++
auto x = double(rng() - rng.min()) / double(rng.max() - rng.min());
```

This outputs a `double` in the range [0, 1]. Note that 1.0 is included in the range now. The reason for subtracting `rng.min()` is that not all PRNGs will have zero as the lowest possible output. For example, the smallest value that `minstd_rand` outputs is 1.

### Using type punning

These techniques avoid the division and integer-to-double conversion. Instead, they construct the bits that make up the floating-point number using an integer, and then reinterpret these bits as floating-point.

This typically uses type punning and is considered undefined behavior in C++, although type punning through a union is legal in C. Even though it's considered "bad" in C++, type punning with a union in C++ will typically work fine too — but there are no guarantees that the compiler emits the correct code here. (To avoid type punning you could use `memcpy` or `std::bit_cast` but that's more expensive.)

How this works: Start from the unsigned integer `0x3FF0000000000000`, which is what 1.0 looks like as a `double`. The zeros are the 52 bits of the mantissa. We can fill these in with the top-most 52 bits of the 64-bit integer returned by the PRNG. When converted to a `double`, the number is now in the interval [1, 2). Subtract 1.0 to get the interval [0, 1).

```c++
union
{
    uint64_t i;
    double f;
}
pun = { 0x3FF0000000000000ULL | (rng() >> 12) };
double randomFloat = pun.f - 1.0;
```

This method is simple and fast but wastes one bit of precision, since it only uses 52 bits instead of the 53 bits of precision that are available in a `double`, although that's a good-enough for our purposes.

The same idea also works for `float`, where you start with `0x3F800000` and use the most-signficant 23 bits of the PRNG output to fill in the mantissa (e.g. shift by `>> 9` when using a 32-bit PRNG).

The above code was based on [Andy Gainey's excellent blog post](https://experilous.com/1/blog/post/perfect-fast-random-floating-point-numbers) where he also investigates solutions for eking out that extra bit of precision.

A similar hack I came across in code by Paul Kellett looks like this:

```c++
unsigned int r = (rng() & 0x7FFFFF) + 0x40000000;
float f = *(float *)&r;
float randomFloat = f - 3.0f;
```

This converts the integer into a floating-point number between 2 and 4, which for `float` have the hexadecimal values 0x40000000 – 0x407FFFFF, and then subtracts 3 to get the float into the range [-1, 1).

It's clever but I wouldn't use this method. Because of the `& 0x7FFFFF`, the random number's most significant bits are discarded. It would be better to use the most significant bits by shifting right before masking. In addition, the kind of type punning used here definitely is undefined behavior.

The POSIX `drand48` and `erand48` functions use [a bit twiddling solution](https://opensource.apple.com/source/Libc/Libc-1044.1.2/gen/FreeBSD/rand48.h.auto.html) where the full 48-bit value is loaded into the mantissa of a `double` and the exponent is set such that the values lie in the interval [0, 1).

Tricks like this might not be significantly faster on modern hardware than doing the int-to-double conversion and the division, but if you need the best possible performance it's worth trying.

## Integer random numbers with a smaller range

You may want to use random numbers for other things than synthesizing white noise. For example, to randomize the plug-in parameters or to pick notes for a random arpreggio.

When using a PRNG that produces unsigned integers up to some large number, it's common to use the modulo operator to make the range more manageable.

To generate a number between 0 and `upperBound` (exclusive):

```c++
auto randomValue = randomGenerator();
auto value = randomValue % upperBound;
```

To generate a number in the interval `[lowerBound, upperBound)`:

```c++
auto randomValue = randomGenerator();
auto value = randomValue % (upperBound - lowerBound) + lowerBound;
```

Note that the upper bound is not included in the range, so if you want to generate a random number from 1 to 10, use `lowerBound = 1` and `upperBound = 11`.

This simple approach works OK most of the time, but you should be aware that it suffers from so-called *modulo bias*. When the full range of the PRNG is not evenly divisible by your modulus, it does not create a uniform distribution of random numbers.

For example, if `randomGenerator` returns numbers from 0 – 4,294,967,295 and you do `% 100`, then all the random numbers that fall between 0 and 4,294,967,199 get mapped to the output range 0 – 99 an equal number of times. So far, so good. But there is also a "left over" bit, from 4,294,967,200 to 4,294,967,295, that is mapped to 0 – 95 only. This means the outcomes 96 - 99 occur one fewer time than the outcomes 0 – 95.

Is this a big deal? It depends on how large the modulus is. For `% 100` the difference is negligible, but with larger ranges it might skew the probabilities. For our purposes it's probably not worth worrying about if the range is smaller than 1 million or so. (If the modulus is a power of two, there is no modulo bias, at least for this particular PRNG.)

One solution to modulo bias is to call the random generator in a loop until it finds a suitable number. See [Mike Ash's blog post](https://www.mikeash.com/pyblog/friday-qa-2011-03-18-random-numbers.html) for example code. This is also what [arc4random_uniform](https://opensource.apple.com/source/Libc/Libc-1044.1.2/gen/FreeBSD/arc4random.c) does. The downside of this solution is that the PRNG is no longer constant time. (Same reason why using C++'s `uniform_*_distribution` is not recommended.)

Tip: For most PRNGs, the lowest bits of the output value are of less quality than the highest bits. It's a good idea to shift the value down to discard the low bits before you do `% upperBound`. Example:

```c++
auto randomValue = randomGenerator();
auto value = (randomValue >> 8) % upperBound;
```

### Using fixed-point math

It would be easy to get a number between 0 and `upperBound` if the random number was a floating-point value between 0 and 1. Just multiply this with `upperBound` and round off the result. It's possible to do this without converting the random number to a float first.

Not sure if this method is well-known but I first saw it in the JUCE source code:

```c++
uint32_t upperBound = /* some number greater than zero */
uint32_t randomValue = randomGenerator();
uint32_t limited = uint32_t((randomValue * uint64_t(upperBound)) >> 32);
```

Actually, `juce::Random` returns an `int` and so their logic was slightly different:

```c++
int upperBound = /* some number greater than zero */
int randomValue = randomGenerator();
int limited = int((uint32_t(randomValue) * uint64_t(upperBound)) >> 32);
```

How this works is similar to doing fixed-point math: Here we assume the 32 bits making up the random value are actually behind the decimal point, so `randomValue` represents a value in [0, 1) but expressed in fixed-point. Multiplying this by `upperBound` gives the result, also in fixed-point. Since multiplying two 32-bit values gives a 64-bit result, we shift right to make it a 32-bit number again, effectively throwing away the portion behind the decimal point. Since this uses the half-open interval [0, 1), the upper bound is exclusive.

This solution doesn't suffer from modulo bias and is presumably faster (no division needed).

### Random boolean

For the special case of getting a random yes/no answer, look at the most significant bit of the random number. For a PRNG that returns unsigned 32-bit integers:

```c++
bool randomBool = (randomGenerator() & 0x80000000);
```

When the PRNG returns signed 32-bit integers, `0x80000000` is the sign bit and so this is equivalent to checking if the random value `>= 0`.

## Choosing a random seed

For creating white noise, it's OK to set a hardcoded seed.

For stuff that needs to appear more random, you want to start from a different seed each time your program launches. The typical way to do this is:

```c++
uint64_t seed = uint64_t(time(0));
```

Since the unit of `time()` is seconds, you may want something with a finer resolution or that is more unpredictable. To make a more interesting seed, you can combine different random-ish values through XOR. Something like this:

```c++
uint64_t seed = uint64_t(time(0)) ^ uint64_t(this) ^ currentTimeMillis() ^ (intptr_t)&printf ^ /* other stuff */;
```

You can also use the system's source of randomness as the initial seed, using C++'s `std::random_device`. This uses hardware entropy and will therefore be as random as it gets.

```c++
unsigned int seed = std::random_device{}();
```

Neither are a good idea to do in the realtime audio callback, but should be fine during initialization.

## Other methods

There are many other methods for making pseudorandom numbers that are suitable for realtime audio.

For example:

- Subtractive generator (lagged Fibonacci sequence). This uses a small lookup table (typically 55 elements) and returns `table[index1] - table[index2]`, and also updates the table with this value. The table is filled in during seeding using a deterministic algorithm. *Numerical Recipes* has a version that directly outputs doubles without having to do an int-to-double conversion. See also [Rosetta Code](https://rosettacode.org/wiki/Subtractive_generator#C) for implementations.

- Multiply-with-carry.

- Subtract-with-borrow.
