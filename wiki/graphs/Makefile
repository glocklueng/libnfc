
all: libnfc-1.3.png libnfc-1.6.png

.SUFFIXES: .dot .png

.dot.png:
	dot -Tpng:cairo $< > $@

clean:
	rm libnfc-1.3.png libnfc-1.6.png
