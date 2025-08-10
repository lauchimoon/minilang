- mov: set values to registers. Can be either of these forms:
```
mov <register1> <value>
mov <register1> <register2>
```

- prnt, prntl: print values inside registers, with or without a newline
```
prnt <register>
prntl <register>
```

- add: add value of a register into another
```
add <register2> <register1>
```
In C, this is equivalent to `register2 += register1;`

- add2: add values of two registers and set it to another
```
add2 <register> <register1> <register2>
```
In C, this is equivalent to `register = register1 + register2;`

- sub: subtract value of a register into another
```
sub <register2> <register1>
```
In C, this is equivalent to `register2 -= register1;`

- mul: multiply value of a register into another
```
mul <register2> <register1>
```
In C, this is equivalent to `register2 *= register1;`

- div: divide from value of a register into another
```
div <register2> <register1>
```
In C, this is equivalent to `register2 /= register1;`

- mod: apply modulo of a register into another
```
mod <register2> <register1>
```
In C, this is equivalent to `register2 %= register1;`

- incr: increment a register value
```
incr <register>
```
In C, this is equivalent to `++register;`

- decr: decrement a register value
```
decr <register>
```
In C, this is equivalent to `--register;`

- clr: clears register value
```
clr <register>
```
For integer and float registers, it sets them to 0. For string registers, it sets them to NULL

- jmp: jump between labels
```
jmp <label>
```
After executing all instructions in `<label>`, it will keep executing instructions after `jmp`

- jmpnz: jump between labels if register is not zero
```
jmpnz <register> <label>
```

- jmpz: jump between labels if register is zero
```
jmpz <register> <label>
```
