all:
	cd src && make && cd ..
	cd example && make && cd ..

install_all:
	mkdir -p /usr/lib
	mkdir -p /usr/include/psched
	cp src/libpsched.so /usr/lib/
	cp include/*.h /usr/include/psched/

clean:
	cd src && make clean && cd ..
	cd example && make clean && cd ..

