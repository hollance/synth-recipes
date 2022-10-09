# Linear feedback shift register (LFSR)

The LFSR is an old school random generator. It's fast because it only uses bit shifts and XOR operations. This is how 8-bit computers such as the Commodore 64 and the NES were able to create noise for sound effects.

LFSRs create pseudorandom noise with a spectrum that is not flat but gently slopes off at the high end. It sits somewhere in between white noise and pink noise. Because it has fewer high frequencies than pure white noise, it's more pleasing to listen to.

There are many possible implementations of LFSRs. Here is an interesting 32-bit version I recently came across. It's based on [Maxim Integrated Application Note 1743](https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/1743.html), which describes how to implement this PRNG using a microcontroller. On such device, it makes sense to restrict the design of a random number generator to shift registers and logic gates, as they may not even have a way to do multiplication in hardware. The implementation below is loosely based on code by [Andre Majorel](https://web.archive.org/web/20180704074216/http://www.teaser.fr/~amajorel/noise/).

```c++
struct LFSR
{
    LFSR(uint32_t seed = 0x55555555) : seed(seed) { }

    uint32_t operator()()
    {
        if (seed & 1) {
            seed = (seed >> 1) ^ 0x80000062;
        } else {
            seed >>= 1;
        }
        return seed;
    }

    uint32_t seed;
};
```

This is a maximal-length 32-bit LFSR, meaning that it has a sequence length of 4294967295. All possible unsigned 32-bit values are generated, except for 0.

Like any other PRNG, you start it with a seed. One thing to keep in mind: the seed cannot be 0. This is because doing an XOR between zeros results in 0 and nothing will ever happen. At least one bit in the seed must be 1 for the LFSR to work.

Starting from the seed `0x55555555`, this particular LFSR generates the following sequence of bits:

```text
              bits                   uint32
*------------------------**---*-   ----------
10101010101010101010101011001000   2863311560
01010101010101010101010101100100   1431655780
00101010101010101010101010110010   715827890
00010101010101010101010101011001   357913945
10001010101010101010101011001110   2326440654
01000101010101010101010101100111   1163220327
10100010101010101010101011010001   2729093841
11010001010101010101010100001010   3512030474
01101000101010101010101010000101   1756015237
...
```

The stars indicate the "tap" positions, which from left-to-right are bits 31, 6, 5, and 1. The LFSR will read from these bit positions and uses that to toggle bits on or off with the XOR. This is the "feedback" part of the LFSR.

This works as follows. In this example the seed is initially 0x55555555, which is an alternating pattern of ones and zeros (but it could be anything, as long as it's not 0):

```text
01010101010101010101010101010101   seed
```

If we shift all of these bits one position to the right, the least-significant bit will "fall off". This is considered to be the output bit. To make the feedback path, the output bit will shift back in on the left.

After shifting our seed looks like the following. The most-significant bit is always 0 now.

```text
00101010101010101010101010101010   shifted
```

Now the bits in the tap positions will be XOR'ed with the output bit. When the output bit is a zero, the XOR has no effect, so we only need to perform the XOR when the least-significant bit was 1. That happens to be the case for the current seed value, so do an XOR between the taps and the shifted seed:

```text
10000000000000000000000001100010   taps
00101010101010101010101010101010   shifted
--------------------------------
10101010101010101010101011001000   xor
*                        **   *
```

Notice how most of the bits don't change, but the ones in the tap positions are toggled from 0 to 1 or from 1 to 0. Also, the bit that was shifted out on the right has made a reappearance on the left. We now have the result from the first row.

With carefully chosen tap positions, this simple series of operations will generate a maximum length sequence for any seed value.

There are several ways to build LFSRs. The above uses the so-called Galois configuration, the other variation is the Fibonacci LFSR. Both methods use this same idea of reading from the previous seed and using feedback to shift new values into the bit pattern. The main difference between the different configurations is where the XOR is placed and whether the values are shifted to the left or right. Instead of reading the entire bit pattern as the random number, you can also use only the output bit (the one that was shifted out) or grab bits from a few different locations and combine them.

Here is an implementation that outputs floating-point numbers between -1.0 and 1.0:

```c++
struct LFSRNoise
{
    LFSRNoise(uint32_t seed = 161803398UL) : seed(seed) { }

    float operator()()
    {
        if (seed & 1UL) {
            seed = (seed >> 1) ^ 0x80000062UL;
        } else {
            seed >>= 1;
        }
        return (float((seed >> 7) & 0x1FFFFFFUL) - 16777216.0f) / 16777216.0f;
    }

    uint32_t seed;
};
```

Because of the decreased high end, LFSR noise appears to be louder than white noise. I found that multiplying by 0.63 gives roughly the same loudness as white noise but with the high frequencies filtered out.

The reason the sequence of random numbers generated by the LFSR does not create pure white noise, is that consecutive LFSR outputs are somewhat correlated. To get pure white noise, the sampes must be independent. You can create white noise with an LFSR by repeating it several times per output sample, but there are [better methods](white-noise.markdown).

According to Pirkle, using the LFSR rather than white noise as the source for a sample-and-hold LFO, will give the LFO a somewhat periodic sound: "When using the [pseudorandom noise] sequence as the source, the random fluctuations have inner patterns that repeat quasi-randomly and quasi-periodically." So that might also be an interesting use of LFSR noise.

References:

- Jon Dattorro, [Effect Design, Part 3 Oscillators: Sinusoidal and Pseudonoise](https://ccrma.stanford.edu/~dattorro/EffectDesignPart3.pdf), §8 Sonically pleasant noise generation
- Will Pirkle, *Designing Software Synthesizer Plugins in C++*, first edition, §5.17
- [Wikipedia](https://en.wikipedia.org/wiki/Linear-feedback_shift_register)
