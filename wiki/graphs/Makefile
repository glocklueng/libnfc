
all: libnfc-1.5.png libnfc-1.8.png

.SUFFIXES: .dot .png

.dot.png:
	dot -Tpng:cairo $< > $@

clean:
	rm libnfc-1.5.png libnfc-1.8.png
