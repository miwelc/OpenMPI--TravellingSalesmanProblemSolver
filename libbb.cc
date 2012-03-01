/* ************************************************************************ */
/*  Libreria de funciones para el Branch-Bound y manejo de la pila          */
/* ************************************************************************ */

#include <cstdio>
#include <cstdlib>
#include <mpi.h>
#include "libbb.h"
using namespace MPI;
extern unsigned int NCIUDADES;

// Tipos de mensajes que se env�an los procesos
const int  PETICION = 0;
const int NODOS = 1;
const int TOKEN = 2;
const int FIN = 3;

// Estados en los que se puede encontrar un proceso
const int ACTIVO = 0;
const int PASIVO = 1;

// Colores que pueden tener tanto los procesos como el token
const int BLANCO = 0;
const int NEGRO = 1;


// Comunicadores que usar� cada proceso
Intracomm comunicadorCarga;	// Para la distribuci�n de la carga
Intracomm comunicadorCota;	// Para la difusi�n de una nueva cota superior detectada

// Variables que indican el estado de cada proceso
extern int rank;	 // Identificador del proceso dentro de cada comunicador (coincide en ambos)
extern int size;	// N�mero de procesos que est�n resolviendo el problema
int estado;	// Estado del proceso {ACTIVO, PASIVO}
int color;	// Color del proceso {BLANCO,NEGRO}
int color_token; 	// Color del token la �ltima vez que estaba en poder del proceso
bool token_presente;  // Indica si el proceso posee el token
int anterior;	// Identificador del anterior proceso
int siguiente;	// Identificador del siguiente proceso
bool difundir_cs_local;	// Indica si el proceso puede difundir su cota inferior local
bool pendiente_retorno_cs;	// Indica si el proceso est� esperando a recibir la cota inferior de otro proceso


/* ********************************************************************* */
/* ****************** Funciones para el Branch-Bound  ********************* */
/* ********************************************************************* */

void LeerMatriz (char archivo[], int** tsp) {
  FILE *fp;
  int i, j;

  if (!(fp = fopen(archivo, "r" ))) {
    printf ("ERROR abriendo archivo %s en modo lectura.\n", archivo);
    exit(1);
  }
  printf ("-------------------------------------------------------------\n");
  for (i=0; i<NCIUDADES; i++) {
    for (j=0; j<NCIUDADES; j++) {
      fscanf( fp, "%d", &tsp[i][j] );
      printf ("%3d", tsp[i][j]);
    }
    fscanf (fp, "\n");
    printf ("\n");
  }
  printf ("-------------------------------------------------------------\n");
}


bool Inconsistente (int** tsp) {
  int  fila, columna;
  for (fila=0; fila<NCIUDADES; fila++) {   /* examina cada fila */
    int i, n_infinitos;
    for (i=0, n_infinitos=0; i<NCIUDADES; i++)
      if (tsp[fila][i]==INFINITO && i!=fila)
        n_infinitos ++;
    if (n_infinitos == NCIUDADES-1)
      return true;
  }
  for (columna=0; columna<NCIUDADES; columna++) { /* examina columnas */
    int i, n_infinitos;
    for (i=0, n_infinitos=0; i<NCIUDADES; i++)
      if (tsp[columna][i]==INFINITO && i!=columna)
        n_infinitos++;               /* increm el num de infinitos */
    if (n_infinitos == NCIUDADES-1)
      return true;
  }
  return false;
}

void Reduce (int** tsp, int *ci) {
  int min, v, w;
  for (v=0; v<NCIUDADES; v++) {
    for (w=0, min=INFINITO; w<NCIUDADES; w++)
      if (tsp[v][w] < min && v!=w)
        min = tsp[v][w];
    if (min!=0) {
      for (w=0; w<NCIUDADES; w++)
        if (tsp[v][w] != INFINITO && v!=w)
          tsp[v][w] -= min;
      *ci += min;       /* acumula el total restado para calc c.i. */
    }
  }
  for (w=0; w<NCIUDADES; w++) {
    for (v=0, min=INFINITO; v<NCIUDADES; v++)
      if (tsp[v][w] < min && v!=w)
        min = tsp[v][w];
    if (min !=0) {
      for (v=0; v<NCIUDADES; v++)
        if (tsp[v][w] != INFINITO && v!=w)
          tsp[v][w] -= min;
      *ci += min;     /* acumula cantidad restada en ci */
    }
  }
}

bool EligeArco (tNodo *nodo, int** tsp, tArco *arco) {
  int i, j;
  for (i=0; i<NCIUDADES; i++)
    if (nodo->incl()[i] == NULO)
      for (j=0; j<NCIUDADES; j++)
        if (tsp[i][j] == 0 && i!=j) {
          arco->v = i;
          arco->w = j;
          return true;
        }
  return false;
}

void IncluyeArco(tNodo *nodo, tArco arco) {
  nodo->incl()[arco.v] = arco.w;
  if (nodo->orig_excl() == arco.v) {
    int i;
    nodo->datos[1]++;
    for (i=0; i<NCIUDADES-2; i++)
      nodo->dest_excl()[i] = NULO;
  }
}


bool ExcluyeArco(tNodo *nodo, tArco arco) {
  int i;
  if (nodo->orig_excl() != arco.v)
    return false;
  for (i=0; i<NCIUDADES-2; i++)
    if (nodo->dest_excl()[i]==NULO) {
      nodo->dest_excl()[i] = arco.w;
      return true;
    }
  return false;
}

void PonArco(int** tsp, tArco arco) {
  int j;
  for (j=0; j<NCIUDADES; j++) {
    if (j!=arco.w)
      tsp[arco.v][j] = INFINITO;
    if (j!=arco.v)
      tsp[j][arco.w] = INFINITO;
  }
}

void QuitaArco(int** tsp, tArco arco) {
  tsp[arco.v][arco.w] = INFINITO;
}

void EliminaCiclos(tNodo *nodo, int** tsp) {
  int cnt, i, j;
  for (i=0; i<NCIUDADES; i++)
    for (cnt=2, j=nodo->incl()[i]; j!=NULO && cnt<NCIUDADES;
         cnt++, j=nodo->incl()[j])
      tsp[j][i] = INFINITO; /* pone <nodo[j],i> infinito */
}

void ApuntaArcos(tNodo *nodo, int** tsp) {
  int i;
  tArco arco;

  for (arco.v=0; arco.v<NCIUDADES; arco.v++)
    if ((arco.w=nodo->incl()[arco.v]) != NULO)
      PonArco (tsp, arco);
  for (arco.v=nodo->orig_excl(), i=0; i<NCIUDADES-2; i++)
    if ((arco.w=nodo->dest_excl()[i]) != NULO)
      QuitaArco (tsp, arco);
  EliminaCiclos (nodo, tsp);
}

void InfiereArcos(tNodo *nodo, int** tsp) {
  bool cambio;
  int cont, i, j;
  tArco arco;

  do {
    cambio = false;
    for  (i=0; i<NCIUDADES; i++)     /* para cada fila i */
      if (nodo->incl()[i] == NULO) {   /* si no hay incluido un arco <i,?> */
        for (cont=0, j=0; cont<=1 && j<NCIUDADES; j++)
          if (tsp[i][j] != INFINITO && i!=j) {
            cont++;  /* contabiliza entradas <i,?> no-INFINITO */
            arco.v = i;
            arco.w = j;
          }
        if (cont==1) {  /* hay una sola entrada <i,?> no-INFINITO */
          IncluyeArco(nodo, arco);
          PonArco(tsp, arco);
          EliminaCiclos (nodo, tsp);
          cambio = true;
        }
      }
  } while (cambio);
}

void Reconstruye (tNodo *nodo, int** tsp0, int** tsp) {
  int i, j;
  for (i=0; i<NCIUDADES; i++)
    for (j=0; j<NCIUDADES; j++)
      tsp[i][j] = tsp0[i][j];
  ApuntaArcos (nodo, tsp);
  EliminaCiclos (nodo, tsp);
  nodo->datos[0] = 0;
  Reduce (tsp,&nodo->datos[0]);
}

void HijoIzq (tNodo *nodo, tNodo *lnodo, int** tsp, tArco arco) {
  int** tsp2 = reservarMatrizCuadrada(NCIUDADES);;
  int i, j;

  CopiaNodo (nodo, lnodo);
  for (i=0; i<NCIUDADES; i++)
    for (j=0; j<NCIUDADES; j++)
      tsp2[i][j] = tsp[i][j];
  IncluyeArco(lnodo, arco);
  ApuntaArcos(lnodo, tsp2);
  InfiereArcos(lnodo, tsp2);
  Reduce(tsp2, &lnodo->datos[0]);
  liberarMatriz(tsp2);
}

void HijoDch (tNodo *nodo, tNodo *rnodo, int** tsp, tArco arco) {
  int** tsp2 = reservarMatrizCuadrada(NCIUDADES);
  int i, j;

  CopiaNodo (nodo, rnodo);
  for (i=0; i<NCIUDADES; i++)
    for (j=0; j<NCIUDADES; j++)
      tsp2[i][j] = tsp[i][j];
  ExcluyeArco(rnodo, arco);
  ApuntaArcos(rnodo, tsp2);
  InfiereArcos(rnodo, tsp2);
  Reduce(tsp2, &rnodo->datos[0]);

	liberarMatriz(tsp2);

}

void Ramifica (tNodo *nodo, tNodo *lnodo, tNodo *rnodo, int** tsp0) {
  int** tsp = reservarMatrizCuadrada(NCIUDADES);
  tArco arco;
  
  Reconstruye (nodo, tsp0, tsp);
  EligeArco (nodo, tsp, &arco);
  
  HijoIzq (nodo, lnodo, tsp, arco);
  HijoDch (nodo, rnodo, tsp, arco);

	liberarMatriz(tsp);

}

bool Solucion(tNodo *nodo) {
  int i;
  for (i=0; i<NCIUDADES; i++)
    if (nodo->incl()[i] == NULO)
      return false;
  return true;
}

int Tamanio (tNodo *nodo) {
  int i, cont;
  for (i=0, cont=0; i<NCIUDADES; i++)
    if (nodo->incl()[i] == NULO)
      cont++;
  return cont;
}

void InicNodo (tNodo *nodo) {
  nodo->datos[0] = nodo->datos[1] = 0;
  for(int i = 2; i < 2 * NCIUDADES; i++)
	nodo->datos[i] = NULO;
}

void CopiaNodo (tNodo *origen, tNodo *destino) {
  for(int i = 0; i < 2 * NCIUDADES; i++)
	destino->datos[i] = origen->datos[i];
}

void EscribeNodo (tNodo *nodo) {
  int i;
  printf ("ci=%d : ",nodo->ci());
  for (i=0; i<NCIUDADES; i++)
    if (nodo->incl()[i] != NULO)
      printf ("<%d,%d> ",i,nodo->incl()[i]);
  if (nodo->orig_excl() < NCIUDADES)
    for (i=0; i<NCIUDADES-2; i++)
      if (nodo->dest_excl()[i] != NULO)
        printf ("!<%d,%d> ",nodo->orig_excl(),nodo->dest_excl()[i]);
  printf ("\n");
}



/* ********************************************************************* */
/* **********         Funciones para manejo de la pila  de nodos        *************** */
/* ********************************************************************* */

bool tPila::push (tNodo& nodo) {
	if (llena())
		return false;

	// Copiar el nodo en el tope de la pila	
	for(int i = 0; i < 2 * NCIUDADES; i++)
		nodos[tope + i] = nodo.datos[i];

	// Modificar el tope de la pila
	tope += 2 * NCIUDADES;
  return true;
}

bool tPila::pop (tNodo& nodo) {
	if (vacia())
		return false;
	
	// Modificar el tope de la pila
	tope -= 2 * NCIUDADES;

	// Copiar los datos del nodo
	for(int i = 0; i < 2 * NCIUDADES; i++)
		nodo.datos[i] = nodos[tope + i];

	return true;
}


bool tPila::divide (tPila& pila2) {

	if (vacia() || tamanio()==1)
		return false;

	int mitad = tamanio() / 2;

	if(tamanio() % 2 == 0){ // La pila se puede dividir en dos partes iguales
		for(int i = 0; i < mitad; i++)
			for(int j = 0; j < 2 * NCIUDADES; j++)
				pila2.nodos[i * 2 * NCIUDADES + j] = nodos[(mitad + i) * 2 * NCIUDADES + j];
		tope = pila2.tope = mitad * 2 * NCIUDADES;
	}
	else{ // La pila no se puede dividir en dos partes iguales
		for(int i = 0; i < mitad; i++)
			for(int j = 0; j < 2 * NCIUDADES; j++)
				pila2.nodos[i * 2 * NCIUDADES + j] = nodos[(mitad + i + 1) * 2 * NCIUDADES + j];
		tope = (mitad + 1) * 2 * NCIUDADES;
		pila2.tope = mitad * 2 * NCIUDADES;
	}

	return true;
}

void tPila::acotar (int U) {
	int tope2 = 0;
	for (int i=0; i < tope; i += 2 * NCIUDADES)
		if (nodos[i] <= U) {
			for(int j = i; j < 2 * NCIUDADES; j++)
				nodos[tope2 + j] = nodos[i + j];
      			tope2 += 2 * NCIUDADES;
    		}
	tope = tope2;

}


/* ******************************************************************** */
//         Funciones de reserva dinamica de memoria
/* ******************************************************************** */

// Reserva en el HEAP una matriz cuadrada de dimension "orden".
int ** reservarMatrizCuadrada(unsigned int orden) {
	int** m = new int*[orden];
	m[0] = new int[orden*orden];
	for (unsigned int i = 1; i < orden; i++) {
		m[i] = m[i-1] + orden;
	}

	return m;
}

// Libera la memoria dinamica usada por matriz "m"
void liberarMatriz(int** m) {
	delete [] m[0];
	delete [] m;
}

