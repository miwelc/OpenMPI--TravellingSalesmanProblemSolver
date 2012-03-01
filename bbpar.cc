/* ******************************************************************** */
/*               Algoritmo Branch-And-Bound Secuencial                  */
/* ******************************************************************** */
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <mpi.h>
#include "libbb.h"
#include "funciones.h"

using namespace std;

unsigned int NCIUDADES;
int rank, size;

main (int argc, char **argv) {
	int size, rank;
    
    MPI_Init(&argc, &argv); // Inicializamos los procesos
    MPI_Comm_size(MPI_COMM_WORLD, &size); // Obtenemos el numero total de procesos
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // Obtenemos el valor de nuestro identificador	

	if(rank == 0 && argc < 3) {
		cerr << "La sintaxis es: bbseq <tamaï¿½o> <archivo>" << endl;
		exit(1);
	}
	
	NCIUDADES = atoi(argv[1]);
	
	int** tsp0 = reservarMatrizCuadrada(NCIUDADES);
	
	tNodo	nodo,         // nodo a explorar
			lnodo,        // hijo izquierdo
			rnodo,        // hijo derecho
			solucion;     // mejor solucion
	bool nueva_U;       // hay nuevo valor de c.s.
	bool nuevaRecibida;
	int  U, cotaFinalLocal, cotaFinalGlobal;             // valor de c.s.
	int iteraciones = 0;
	int iteracionesTotales;
	tPila pila;         // pila de nodos a explorar
	int estado = ACTIVO;
	int nodosExplorados = 0;

	U = INFINITO;                  // inicializa cota superior
	InicNodo (&nodo);              // inicializa estructura nodo

	if(rank == 0) {
		LeerMatriz (argv[2], tsp0);    // lee matriz de fichero
		if(!Inconsistente(tsp0)) {
			pila.push(nodo);
			tenemosToken = BLANCO;
		}
	}
	
	//El proceso 0 reparte la matriz tps0
	MPI_Bcast(tsp0[0], // Puntero al dato que vamos a enviar
	                NCIUDADES*NCIUDADES,  // Numero de datos a los que apunta el puntero
	                MPI_INT, // Tipo del dato a enviar
	                0, // Identificacion del proceso que envia el dato
	                MPI_COMM_WORLD); 
			            
	
	double t=MPI::Wtime();
	while(estado != TERMINAR) {
		Equilibrar_Carga(&pila, &estado, rank, size);
		
		if(estado == ACTIVO) {       // ciclo del Branch&Bound
			pila.pop(nodo);
			Ramifica (&nodo, &lnodo, &rnodo, tsp0);		
			nueva_U = false;
			nuevaRecibida = false;
			if (Solucion(&rnodo)) {
				if (rnodo.ci() < U) {    // se ha encontrado una solucion mejor
					U = rnodo.ci();
					nueva_U = true;
					CopiaNodo (&rnodo, &solucion);
				}
			}
			else {                    //  no es un nodo solucion
				if (rnodo.ci() < U) {     //  cota inferior menor que cota superior
					if (!pila.push(rnodo)) {
						printf ("Error: pila agotada\n");
						liberarMatriz(tsp0);
						exit (1);
					}
				}
			}
			if (Solucion(&lnodo)) {
				if (lnodo.ci() < U) {    // se ha encontrado una solucion mejor
					U = lnodo.ci();
					nueva_U = true;
					CopiaNodo (&lnodo,&solucion);
				}
			}
			else {                     // no es nodo solucion
				if (lnodo.ci() < U) {      // cota inferior menor que cota superior
					if (!pila.push(lnodo)) {
						printf ("Error: pila agotada\n");
						liberarMatriz(tsp0);
						exit (1);
					}
				}
			}
			
			difusionCotaSuperior(&U, &nueva_U, &nuevaRecibida, rank, size);
			if(nueva_U || nuevaRecibida)
				pila.acotar(U);
					
			iteraciones++;
		}
	}
    t=MPI::Wtime()-t;
    
    cotaFinalLocal = solucion.ci();
    MPI_Allreduce(&cotaFinalLocal, &cotaFinalGlobal, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&iteraciones, &iteracionesTotales, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    
    if(cotaFinalLocal == cotaFinalGlobal) { //Tenemos la solución
	    printf("Solucion:\n");
		EscribeNodo(&solucion);
	    cout<< "Tiempo gastado = "<<t<<endl;
		cout << "Numero de nodos totales explorados = " << iteracionesTotales << endl;
		cout << "N/T: " << iteracionesTotales/t << endl;
		liberarMatriz(tsp0);
	}
    
	MPI::Finalize();
}

