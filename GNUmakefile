.PHONY: all clean upload uploadfs

all:
	platformio run

clean:
	platformio run -t clean
	rm -rf .pio
	rm -f data/certs.ar

upload:
	platformio run -t upload

uploadfs: data/certs.ar
	platformio run -t uploadfs

data/certs.ar: certs/isrg-root-x1.der certs/isrg-root-x2.der
	mkdir -p data
	ar q $@ $^
