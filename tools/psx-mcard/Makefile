all: makemu

makemu: makemu.c
	gcc makemu.c -o makemu

run: makemu
	./makemu
	hexdump -C output.mcd
	hexdump -C ../no\$$psx/MEMCARD/kernel.mcd -n49

clean:
	rm -f makemu output.mcd
