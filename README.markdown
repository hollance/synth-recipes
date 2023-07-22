# Sound Synthesis Recipes

>  
> **NOTE:** I've decided to turn this content into a blog instead. Read it here: [audiodev.blog](https://audiodev.blog)  
>  
> (This repo won't be maintained any longer.)  
>  

I've been collecting code snippets of sound synthesis algorithms for a while now. Recently I decided to clean up my notes and organize them a bit better into a handy reference.

The inspiration for this is *The Pocket Handbook of Image Processing Algorithms In C* by Myler & Weeks, a concise guide of algorithms for image processing. I was looking for something similar for audio synthesis but came up empty-handed, so I've started to write my own.

This is not meant to be an exhaustive treatise of the entire field of digital audio synthesis, only a quick  reference — with working source code! Where possible I have included links to material for further research.

The goal of this work is to educate, not necessarily to provide a DSP library that has super optimized ready-to-use routines. Feel free to take this code and tweak it to your needs.

## Table of contents

Introduction:

- Quick introduction to digital audio
- How to read this guide
- How to run the source code

Basic waveforms:

- Sine waves

Noise:

- [White noise / random numbers](recipes/white-noise.markdown)
- [LFSR noise](recipes/lfsr-noise.markdown)
- [Filtered noise & explosions](recipes/explosions.markdown)
- Pink noise

👷 This is a work in progress! I'll be adding new content every so often.

## Is this enough to build a synthesizer?

Yes and no. The purpose of this guide is purely to describe different algorithms for producing sound. That is an important part of building a musical instrument, but not the only part. To implement a complete software synthesizer, you will need to tackle a lot of additional functionality such as:

- handling MIDI
- using envelopes to shape the sound over time
- polyphony and voice support
- filters
- modulation
- user-configurable parameters
- talking to a host through the VST or Audio Unit protocol
- a user interface
- and many other features...

Those topics are outside the scope of this guide. As it happens, I also wrote a book about how to do all this other stuff: [Code Your Own Synth Plug-Ins With C++ and JUCE](https://leanpub.com/synth-plugin).

![Cover of my synth book](illustrations/book-cover.jpg)

Learn the fundamentals of audio programming by building a fully-featured software synthesizer plug-in, with every step explained along the way. The plug-in can be used in all the popular DAWs and is made using the industry standard tools for audio development: JUCE framework and the C++ programming language. Not too much math, lots of explanations!

[Buy the book here](https://leanpub.com/synth-plugin) (free sample chapters available)

## Contributors

Written by Matthijs Hollemans in 2022.

## License

The source code snippets from this work can be freely used without restrictions, unless noted otherwise. The use of these programs is at your own risk.

It is possible that a demonstrated algorithm is under some form of patent protection. The author of this work cannot vouch for the suitability of using these algorithms for any purpose other than education.

```text
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

The text is licensed under a [Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License](http://creativecommons.org/licenses/by-nc-sa/4.0/).
