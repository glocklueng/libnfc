
PNG= libnfc-1.3.png libnfc-1.4.png libnfc-1.6.png

.SUFFIXES: .dot .png

all: ${PNG}

.dot.png:
	dot -Tpng:cairo $< > $@

clean:
	rm -f ${PNG}
