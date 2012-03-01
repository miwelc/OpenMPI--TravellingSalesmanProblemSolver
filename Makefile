
bbpar: bbpar.o libbb.o
	mpicxx bbpar.o libbb.o -o bbpar
	
bbpar.o: bbpar.cc
	mpicxx -c bbpar.cc


libbb.o: libbb.cc libbb.h
	mpicxx -c  libbb.cc 


clean:
	/bin/rm -f *.o bbpar



