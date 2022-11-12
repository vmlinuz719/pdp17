# pdp17
What if the PDP-8 were stretched to 16 bits?

`cc bus.c main.c cpu.c tty.c -o pdp17 -lpthread`

Suggested program:

```
a100
d270f
d00ff
de001
d0101
dc024
dc021
da005
de120
da002
d6501
de102
a100
g
```

