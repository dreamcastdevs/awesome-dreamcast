# Dreamcast optimisation
These notes is mostly taken from the good advice of all the Dreamcast scholars (Ian Micheal, Moop, Mrneo, and more). The original documents are in the [notes](https://github.com/dreamcastdevs/awesome-dreamcast/tree/master/notes) folder.

## Dan Potter's GCC-SH4 tips
### Use local variables.

   Global variables are slow - to retrieve the value, the SH4 typically
   must execute:

   ```
   mov.l L2,r1
   mov.l @r1,r1
   ```

   Local variables are faster - it's stack-relative, and **function parameters
   are even faster because the first four integers parameters are passed
   in r4-r7 and first eight floating-point parameters in fr4-fr11**.

### Write small functions.

   We've noticed GCC generates very pessimal code when it starts to
   spill registers, so try to avoid doing too much in one function.

   A function which exceeds more than about a hundred lines should
   be broken into smaller functions.

### Use struct copies (instead of copying individual elements of a struct).

    GCC and G++ generate code with weak scheduling when copying a struct
    by individual elements.

    GCC and G++ generate code with better instruction scheduling when
    copying a struct via struct assignment.
