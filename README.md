# minilang
Work in progress.

## The basics
- Set values using `mov`: specify register and value
```
mov s0 "Hello world"
mov i0 1337
mov f0 420.69
```

- Add values using `add`: specify two registers `r1` and `r2`, and sets `r1` to `r1 + r2`
```
mov i0 34
mov i1 35
add i0 i1
```

- Print values using `prnt` and `prntl`
```
prnt i0
prntl s0
```
