CC=gcc
CFLAGS=-m32 -I.
OBJ = x86_mmu_emu.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

x86_mmu_emu: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f *.o x86_mmu_emu
