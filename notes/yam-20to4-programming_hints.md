[Original source](http://yam.20to4.net/dreamcast/hints/index.html)
## A few programming hints I thought they are worth sharing.

Vector Math Instructions
Cache
Store Queues
Optimizing TnL Loops


## Vector Math Instructions
The SH4 CPU featrues some nice builtin math operations. I saw some people trying to speed up their code by using approximations with integer codes and other tricks. That may work but on the SH4 good code that makes use of the builtin instructions will outperform in the most situations. There is not much sense in trying to replace that functions at all since they are already pretty fast.

### `FIPR`
Computes the dot product of two 4D vectors `x1*x2 + y1*y2 + z1*z2 + w1*w2`
Often used in lighting formulas.

### `FTRV`
Muliplies a 4x4 matrix with a 4D vector

Usually used to transform vertices, but can also be used to compute "a * b + c" for four floats in parallel. However, the FMAC may perform better in particular situations because the matrix needs to be setup in a special way.

### `FSRRA`
Approximates `1.0f / sqrt (x)`
Usually used to normalize a vector

### `FSCA`
Approximates the sine and cosine.

Comes in handy when constructing rotation matrices. This one takes the angle as an integer in the range 0...32767 so it's best to redefine angle representations throughout the whole code so that an angle of PI = 16384 or something. This would save the required conversion (range scale and truncation to integer).

### `FSQRT`
Computes `sqrt (x)`

This is actually not a special instruction but it's there and comes in handy sometimes although it's slower than FSRRA so rewrite your math formulas (e.g. range falloff lighting) and try to use FSRRA where applicable instead.

### `FMAC`
Computes `a * b + c`

This is as fast as normal float addition, subtraction or multiplication, so basically you pay one and get another operation for free. Comes in handy when doing linear interpolation (polygon clipping, table lookups etc).

### `FMOV` (64 bit)

This instruction is very important since it can access the second banked set of the FPU registers that are normally used to hold the vector transformation matrix. It's best to avoid switching the fmov mode very often, so rearrange your data layout. It also requires the data to be 64 bit aligned (8 bytes).


### Cache
The cache is very very important. It is so important that it may either save your day or screw up everything.

First of all it is important to understand how the cache works. It is a direct mapped cache, i.e. the cache lines are mapped directly onto the addresses of the memory and since it's only 16 KB small the memory at address (n) is mapped onto the same cache line as the location at address (n + 1024*16) which will result in a cache miss. So it's best to avoid such situations which is not always possible especially when working with large amounts of data, e.g. vertex data.
That's where the powerful cache control instructions come in...

### `PREF`
Prefetches a cache line for the specified address

Use this to prefetch the next element in processing loops for large data buffers. The prefetch delay can be hidden by prefetching the next element at the beginning of the loop.

### `OCBWB`
Writes back a cache line

This one is often used when cache content and memory content need to be synchronized explicitely for DMA transfers since the DMA controllers operate directly on memory.

### `OCBI`
Invalidates a cache line

This is also used often with DMA functions before reading a memory area that had just been rewritten by a DMA controller. Another way would be to access the memory area as uncached but this might slow down everything.

### `OCBP`
Purges a cache line (write back & invalidate)

Use this one after your processing loop definitely has finished its operations for one element. The effect is very similar to the store queues except that you cannot read anything from the store queue lines and can write only 32 or 64 bits at once.

### `MOVCA.L`
Allocates a cache line for the specified address

This one is very important when processing large buffers. While the OCBP instruction can be left out wihtout any performance drops this is does not apply for MOVCA.L.

When processing large buffers you often have an input buffer and an output buffer. The input buffer is read, something is done with the elements and they are stored into the output buffer. Without MOVCA.L a store to the output buffer would cause the cache to read a cache line from the output buffer and redirect the store into the cache. This is a total waste, because most of time every byte of the output buffer is only rewritten with new content. That's what the MOVCA.L is for. It tells the cache to reserve a cache line without fetching memory. But be careful this may cause data corruption.

There is another cache feature which makes it possible to use half of the operand cache (i.e. 8 KB) as scratchpad RAM. This may be used to hold transformation loop parameters like matrices or something but it also halves the operand cache size which may decrease overall performance of the code.


### Store Queues
These come in handy when flushing vertices to the graphics hardware or to memory. Basically all they are for is batching up 32 bytes of data and flushing it to some location. The store queues can be used to write to main memory but they bypass the cache thus cache synchronization may become an issue.
There are some benefits when using the store queues, among them there are two which I think are worth mentioning.

First there are two of them each 32 bytes in size and both operate independently, i.e. you can instruct one store queue to flush its contents and write to the other at the same time without stalling the program code.

Then there is another nice feature which I call "data cloning". Sometimes you need to clone data for whatever reason. For one normal store and one cloning operation this implies two reads and two writes per element. With appropriate pointer mapping setup you can reduce this to one read and two writes which will boost the performance of your code. The trick is easy to do. All you have to do is telling a store queue to flush with two different addresses. The store queue number is indicated by bit 6 of the flushing address and the other upper bits are the raw address where the store queue content is flushed to. However, each flush instruction will stall the code until the concerned store queue has finished a previous flush operation. So it's best to do something else between two flush instructions.

Notice also that the store queues can be used in conjunction with the MMU for mapping the store queue address space to different pieces of hardware which eliminates the need to set up the QARC0 / QARC1 registers.


### Optimizing TnL Loops
This is probably one of the most frequent optimization topics.
Always remember the 20-80 rule: 80 percent of the time is spent in 20 percent of the code.
Thus first of all identify your bottle necks. For the most cases this will be the graphics routines, or more precicely the transform and lighting part of the graphics routines, since on Dreamcast this stuff is done by the CPU.

When doing "complex" transformation and lighting you may get very large loops that do a lot of operations per vertex. Of course it's best to avoid any operations in the first place (culling) but eventually you will have to compute the lighting intensities for 3 light sources, compute the bumpmapping parameters, apply 8 skinning matrices or whatever you are going to do with one vertex.

Let's take a simple example which is supposed to apply diffuse and specular lighting to a vertex, transforming the vertex and storing it in a buffer.
This may look as follows:
 ```
do
{
      fetch vertex

      apply diffuse lighting ((N * L) * material diffuse color * light color)

      apply specular lighting (pow ((N * H), material shininess) * material specular color * light color)

      clamp and pack the lighting result to packed 32 bit ARGB

      apply transformation (matrix) * (x,y,z,1)
      apply perspective divide (1/w)

      write back vertex

} while (-- count);
```
Now add a few more things and you will get quite a big loop and you will run out of FPU registers for sure. Additionally this code is subject to bad pipelining since most of the operatations depend on the results of previous operations.

So there are a few things we better do in a different way.

Normally each vertex does not depend on adjacent vertices in the buffer. The operations applied are the same for each vertex. If we had a lot of CPU cores as the Cell Processor will we could do some parallel processing, but we don't. Still there is a way to speed things up by processing two vertices at a time, but this will work only if there are enough FPU registers left to hold at least some parts of a second vertex. What we do is simply breaking up the loop in independent and simple steps:

      - apply diffuse lighting
      - apply specular lighting
      - clamp and pack lighting result
      - apply transformation and perspective divide

Thus we'll get something like...
 ```
do
{
    fetch vertex parts
    apply diffuse lighting
    store temporary results
} while (-- count);

do
{
    fetch vertex parts
    apply specular lighting
    store temporary results
} while (-- count);

do
{
    fetch temporary results
    clamp and pack lighting result
    store packed values
} while (-- count);

do
{
    fetch vertex parts
    apply transformation
    apply perspective divide
    store results
} while (-- count);
```

Now we have small and simple loops that can be pipelined much better since there are more spare registers left. Also the loops can be applied in any order and in any combination making the code more flexible. For instance, if you don't need specular lighting then just don't call the specular lighting loop, that's it.

The trick now is to run the small loops not over the whole buffer at once but over smaller blocks of the buffer. This way the currently processed vertices will be kept in the cache for each of the many loop iterations. As to my experience a block size of 32 vertices gives the best performance. Decreasing the value may introduce loop startup or function call overhead. Increasing the value may also increase cache misses. It's the same as with network packet sizes: you have to try a little bit to find the best value.
